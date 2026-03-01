#include <engine.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/ledc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t get_now() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

static uint32_t transpose_frequency(uint32_t frequency, int transpose) {

  if (transpose < 0) {
    return frequency >> -transpose;
  }

  if (transpose > 0) {
    return frequency << transpose;
  }

  return frequency;
}

static int find_suitable_resolution(uint32_t frequency,
                                    uint32_t clock_frequency,
                                    uint32_t *result) {
  for (int resolution = LEDC_TIMER_BIT_MAX - 1; resolution >= LEDC_TIMER_1_BIT;
       --resolution) {

    uint32_t divider = clock_frequency / (frequency * (1 << resolution));
    if (divider >= 1 && divider < 1024) {

      (void)(result && (*result = resolution));

      return 1;
    }
  }

  return 0;
}

static void set_voice(engine_t *engine, voice_t *voice, uint32_t frequency,
                      uint32_t velocity) {

  if (!frequency || !velocity) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, voice->channel, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, voice->channel);
    return;
  }

  uint32_t max_duty, resolution;
  if (voice->current_frequency == frequency) {
    max_duty = ((1 << voice->current_resolution) - 1) >> 4;
  } else if (find_suitable_resolution(frequency, 80000000, &resolution)) {

    if (voice->current_resolution == resolution) {
      ledc_set_freq(LEDC_LOW_SPEED_MODE, voice->timer, frequency);
    } else {
      ledc_timer_config_t timer = {
          .speed_mode = LEDC_LOW_SPEED_MODE,
          .duty_resolution = resolution,
          .timer_num = voice->timer,
          .freq_hz = frequency,
          .clk_cfg = LEDC_USE_PLL_DIV_CLK,
      };
      ledc_timer_config(&timer);
    }

    max_duty = ((1 << resolution) - 1) >> 4;

    voice->current_frequency = frequency;
    voice->current_resolution = resolution;
  } else {
    max_duty = 0;
  }

  uint32_t duty =
      MAP(uint32_t, float, 0, engine->max_velocity, 0, max_duty, velocity);

  ledc_set_duty(LEDC_LOW_SPEED_MODE, voice->channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, voice->channel);
}

static voice_t *find_free_perfect_voice(engine_t *engine, uint32_t frequency) {

  for (size_t i = 0; i < engine->voice_count; ++i) {
    voice_t *voice = engine->voices + i;

    if (voice->owner && voice->owner->voice == voice) {
      continue;
    }

    if (voice->current_frequency != frequency) {
      continue;
    }

    return voice;
  }

  return NULL;
}

static voice_t *find_free_voice(engine_t *engine) {

  for (size_t i = 0; i < engine->voice_count; ++i) {
    voice_t *voice = engine->voices + i;

    if (voice->owner && voice->owner->voice == voice) {
      continue;
    }

    return voice;
  }

  return NULL;
}

static voice_t *find_first_perfect_voice(engine_t *engine, uint32_t frequency,
                                         uint32_t velocity) {

  for (size_t i = 0; i < engine->voice_count; ++i) {
    voice_t *voice = engine->voices + i;

    if (voice->current_frequency != frequency) {
      continue;
    }

    if (voice->current_velocity >= velocity) {
      continue;
    }

    return voice;
  }

  return NULL;
}

static voice_t *find_first_voice(engine_t *engine, uint32_t velocity) {

  for (size_t i = 0; i < engine->voice_count; ++i) {
    voice_t *voice = engine->voices + i;

    if (voice->current_velocity >= velocity) {
      continue;
    }

    return voice;
  }

  return NULL;
}

/**
 * algorithm:
 *  1. try to find a free voice that already has the perfect frequency
 *  2. try to find any free voice
 *  3. try to find a non-free voice that already has the perfect frequency
 *  4. try to find any non-free voice
 */
static voice_t *allocate_voice(engine_t *engine, uint32_t frequency,
                               uint32_t velocity) {

  voice_t *voice;

  voice = find_free_perfect_voice(engine, frequency);
  if (voice) {
    return voice;
  }

  voice = find_free_voice(engine);
  if (voice) {
    return voice;
  }

  if (ENGINE_VOICE_STEALING) {
    voice = find_first_perfect_voice(engine, frequency, velocity);
    if (voice) {
      return voice;
    }

    voice = find_first_voice(engine, velocity);
    if (voice) {
      return voice;
    }
  }

  return NULL;
}

static void free_and_set_voice(engine_t *engine, track_t *track) {
  if (!track->voice) {
    return;
  }

  track->cache = track->voice;

  set_voice(engine, track->voice, 0, 0);

  track->voice->owner = NULL;
  track->voice->current_velocity = 0;
  track->voice = NULL;
}

/**
 * @return if voice
 */
static int allocate_and_set_voice(engine_t *engine, track_t *track,
                                  const event_data_t *event) {
  uint32_t frequency = transpose_frequency(event->frequency, track->transpose);

  if (!track->voice) {
    if (!track->cache ||
        (track->cache->owner && track->cache->owner != track)) {
      track->voice = allocate_voice(engine, frequency, event->velocity);
    } else {
      track->voice = track->cache;
    }
  }

  if (track->voice) {
    track->voice->owner = track;
    track->voice->current_velocity = event->velocity;
    set_voice(engine, track->voice, frequency, event->velocity);
    return 1;
  }

  return 0;
}

/**
 * @return if end
 */
static int track_spin(engine_t *engine, track_t *track, uint32_t now) {

  const event_data_t *event;

  if (track->current_event >= track->event_count) {
    return 1;
  }

  event = track->events + track->current_event;

  if (now < event->time + event->duration) {
    if (!track->active && now >= event->time) {
      track->active = allocate_and_set_voice(engine, track, event);
    }
    return 0;
  }

  while (now >= event->time + event->duration) {
    if (++track->current_event >= track->event_count) {
      track->active = 0;
      free_and_set_voice(engine, track);
      return 1;
    }

    event = track->events + track->current_event;
  }

  if (now < event->time) {
    track->active = 0;
    free_and_set_voice(engine, track);
    return 0;
  }

  track->active = allocate_and_set_voice(engine, track, event);
  return 0;
}

void engine_init(

    engine_t *engine,

    const track_data_t *tracks, size_t track_count,

    const int *pins, size_t pin_count

) {
  engine->max_velocity = 0;

  engine->track_count = track_count;
  engine->tracks = (track_t *)calloc(engine->track_count, sizeof(track_t));

  for (size_t i = 0; i < engine->track_count; ++i) {

    const track_data_t *track_data = tracks + i;
    track_t *track = engine->tracks + i;

    track->events = track_data->events;
    track->event_count = track_data->event_count;

    track->transpose = track_data->transpose;

    track->current_event = 0;
    track->active = 0;

    track->voice = NULL;
    track->cache = NULL;

    for (size_t j = 0; j < track->event_count; ++j) {
      const event_data_t *event = track->events + j;

      if (engine->max_velocity < event->velocity) {
        engine->max_velocity = event->velocity;
      }
    }
  }

  engine->voice_count = MIN(pin_count, ENGINE_VOICE_MAX);
  engine->voices = (voice_t *)calloc(engine->voice_count, sizeof(voice_t));

  for (size_t i = 0; i < engine->voice_count; ++i) {
    voice_t *voice = engine->voices + i;

    voice->owner = NULL;

    voice->timer = (ledc_timer_t)i;
    voice->channel = (ledc_channel_t)i;

    voice->current_frequency = 0;
    voice->current_resolution = 0;

    ledc_channel_config_t channel = {
        .gpio_num = pins[i],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = voice->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = voice->timer,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
  }
}

void engine_terminate(engine_t *engine) {

  for (size_t i = 0; i < engine->voice_count; ++i) {
    voice_t *voice = engine->voices + i;

    set_voice(engine, voice, 0, 0);
  }

  free(engine->tracks);
  free(engine->voices);

  engine->track_count = 0;
  engine->tracks = NULL;

  engine->voice_count = 0;
  engine->voices = NULL;
}

int engine_spin(engine_t *engine) {

  uint32_t now = get_now();

  int end = 1;
  for (size_t i = 0; i < engine->track_count; ++i)
    end &= track_spin(engine, engine->tracks + i, now);

  return end;
}
