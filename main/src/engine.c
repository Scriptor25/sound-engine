#include "engine.h"

static int find_suitable_resolution(uint32_t frequency,
                                    uint32_t clock_frequency,
                                    uint32_t *result) {
  int resolution;
  uint32_t divider;

  for (resolution = LEDC_TIMER_BIT_MAX - 1; resolution >= LEDC_TIMER_1_BIT;
       --resolution) {

    divider = clock_frequency / (frequency * (1 << resolution));
    if (divider >= 1 && divider < 1024) {

      if (result) {
        *result = resolution;
      }

      return 1;
    }
  }

  return 0;
}

static const envelope_data_t *get_envelope(engine_t *engine, uint32_t program) {
  return find_envelope(engine->envelopes, engine->envelope_count, program);
}

static void clear_voice(voice_t *voice) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, voice->channel, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, voice->channel);
}

static void set_voice(engine_t *engine, voice_t *voice, uint32_t frequency,
                      uint32_t velocity) {

  uint32_t max_duty, resolution, duty;

  if (!engine || !frequency || !velocity) {
    clear_voice(voice);
    return;
  }

  if (voice->current_frequency == frequency) {
    max_duty =
        ((1 << voice->current_resolution) - 1) >> ENGINE_RESOLUTION_POWER;
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

    max_duty = ((1 << resolution) - 1) >> ENGINE_RESOLUTION_POWER;

    voice->current_frequency = frequency;
    voice->current_resolution = resolution;
  } else {
    max_duty = 0;
  }

  duty = MAP(uint32_t, float, 0, engine->max_velocity, 0, max_duty, velocity);

  ledc_set_duty(LEDC_LOW_SPEED_MODE, voice->channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, voice->channel);
}

static voice_t *find_voice(engine_t *engine, uint32_t now, uint32_t frequency,
                           uint32_t velocity, int ignore_owner,
                           int ignore_frequency) {
  size_t i;
  voice_t *voice;

  for (i = 0; i < engine->voice_count; ++i) {
    voice = engine->voices + i;

    if (!ignore_owner && voice->current_owner) {
      continue;
    }

    if (!ignore_owner && voice->current_cooldown > now) {
      continue;
    }

    if (!ignore_frequency && voice->current_frequency != frequency) {
      continue;
    }

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
static voice_t *allocate_voice(engine_t *engine, uint32_t now,
                               uint32_t frequency, uint32_t velocity) {

  voice_t *voice;

  voice = find_voice(engine, now, frequency, velocity, 0, 0);
  if (voice) {
    return voice;
  }

  voice = find_voice(engine, now, frequency, velocity, 0, 1);
  if (voice) {
    return voice;
  }

  voice = find_voice(engine, now, frequency, velocity, 1, 0);
  if (voice) {
    return voice;
  }

  voice = find_voice(engine, now, frequency, velocity, 1, 1);
  if (voice) {
    return voice;
  }

  return NULL;
}

static void free_and_clear_voice(track_t *track, uint32_t now) {
  if (!track->voice) {
    return;
  }

  clear_voice(track->voice);

  track->voice->current_owner = NULL;
  track->voice->current_velocity = 0;
  track->voice->current_cooldown = now + 10;

  track->voice = NULL;
}

static void allocate_and_set_voice(engine_t *engine, track_t *track,
                                   uint32_t now, const event_data_t *event) {
  uint32_t frequency, velocity;

  frequency = transpose_frequency(event->frequency, track->data->transpose);
  velocity = calculate_velocity(track->envelope, now, event->velocity,
                                event->time, event->duration);

  if (!track->voice) {

    if (track->cache && !track->cache->current_owner) {
      track->voice = track->cache;
    } else {
      track->voice = allocate_voice(engine, now, frequency, event->velocity);

      if (!track->voice) {
        return;
      }

      if (track->voice->current_owner) {
        track->voice->current_owner->voice = NULL;
      }
    }
  }

  track->cache = track->voice;

  track->voice->current_owner = track;
  track->voice->current_velocity = event->velocity;

  set_voice(engine, track->voice, frequency / 1000, velocity);
}

static int track_spin(engine_t *engine, track_t *track, uint32_t now) {

  const event_data_t *event;

  if (track->current_event >= track->data->event_count) {
    return 1;
  }

  event = track->data->events + track->current_event;

  if (now < event->time + event->duration) {
    if (now >= event->time) {
      allocate_and_set_voice(engine, track, now, event);
    }
    return 0;
  }

  while (now >= event->time + event->duration) {
    if (++track->current_event >= track->data->event_count) {
      free_and_clear_voice(track, now);
      return 1;
    }

    event = track->data->events + track->current_event;
  }

  if (now < event->time) {
    free_and_clear_voice(track, now);
    return 0;
  }

  allocate_and_set_voice(engine, track, now, event);
  return 0;
}

void engine_init(

    engine_t *engine,

    const envelope_data_t *envelopes, size_t envelope_count,

    const track_data_t *tracks, size_t track_count,

    const int *pins, size_t pin_count

) {
  size_t i, j;
  track_t *track;
  voice_t *voice;
  const event_data_t *event;

  engine->envelope_count = envelope_count;
  engine->envelopes = envelopes;

  engine->max_velocity = 0;

  engine->track_count = track_count;
  engine->tracks = (track_t *)calloc(engine->track_count, sizeof(track_t));

  for (i = 0; i < engine->track_count; ++i) {
    track = engine->tracks + i;

    track->data = tracks + i;

    track->current_event = 0;

    track->envelope = get_envelope(engine, track->data->program);

    track->voice = NULL;
    track->cache = NULL;

    for (j = 0; j < track->data->event_count; ++j) {
      event = track->data->events + j;

      if (engine->max_velocity < event->velocity) {
        engine->max_velocity = event->velocity;
      }
    }
  }

  engine->voice_count = MIN(pin_count, ENGINE_VOICE_MAX);
  engine->voices = (voice_t *)calloc(engine->voice_count, sizeof(voice_t));

  for (i = 0; i < engine->voice_count; ++i) {
    voice = engine->voices + i;

    voice->timer = (ledc_timer_t)i;
    voice->channel = (ledc_channel_t)i;

    voice->current_frequency = 0;
    voice->current_resolution = 0;

    voice->current_owner = NULL;
    voice->current_velocity = 0;
    voice->current_cooldown = 0;

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
  size_t i;

  for (i = 0; i < engine->voice_count; ++i) {
    clear_voice(engine->voices + i);
  }

  free(engine->tracks);
  free(engine->voices);

  engine->track_count = 0;
  engine->tracks = NULL;

  engine->voice_count = 0;
  engine->voices = NULL;
}

int engine_spin(engine_t *engine) {
  uint32_t now;
  int end;
  size_t i;
  track_t *track;

  now = get_now();

  end = 1;
  for (i = 0; i < engine->track_count; ++i) {
    track = engine->tracks + i;
    if (track->envelope) {
      end &= track_spin(engine, engine->tracks + i, now);
    }
  }

  return end;
}
