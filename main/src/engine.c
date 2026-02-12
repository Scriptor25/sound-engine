#include <engine.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/ledc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t get_now() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

static voice_t *voice_allocate(engine_t *engine, track_t *owner_track) {
  for (int i = 0; i < MAX_VOICES; ++i) {

    voice_t *voice = engine->voices + i;

    if (voice->owner_track)
      continue;

    voice->owner_track = owner_track;
    return voice;
  }
  return NULL;
}

static void voice_free(voice_t *voice) { voice->owner_track = NULL; }

static void voice_set(voice_t *voice, uint32_t pin, uint32_t frequency,
                      uint32_t duty) {
  ledc_set_pin(pin, LEDC_LOW_SPEED_MODE, voice->channel);
  ledc_set_freq(LEDC_LOW_SPEED_MODE, voice->timer, frequency);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, voice->channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, voice->channel);
}

static int track_spin(engine_t *engine, track_t *track, uint32_t now) {

  const event_t *event;

  if (track->current_event >= track->event_count)
    return 1;

  event = track->events + track->current_event;

  if ((now - track->current_event_time) < event->duration)
    return 0;

  if (track->voice) {
    voice_set(track->voice, 0, 0, 0);
    voice_free(track->voice);
    track->voice = NULL;
  }

  if (++track->current_event >= track->event_count)
    return 1;

  track->current_event_time = now;
  event = track->events + track->current_event;

  if (event->frequency && (track->voice = voice_allocate(engine, track)))
    voice_set(track->voice, track->pin, event->frequency, 128);

  return 0;
}

static void engine_init_voices(engine_t *engine) {

  for (size_t i = 0; i < MAX_VOICES; ++i) {

    voice_t *voice = engine->voices + i;

    voice->channel = i;
    voice->timer = i;
    voice->owner_track = NULL;

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = i,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = -1,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = i,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = i,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
  }
}

static void engine_init_channels(engine_t *engine) {

  for (size_t i = 0; i < engine->track_count; ++i) {

    track_t *track = engine->tracks + i;

    track->current_event = 0;
    track->current_event_time = get_now();
    track->voice = NULL;
  }
}

void engine_init(engine_t *engine, const track_t *tracks, size_t track_count) {

  track_t *copy = calloc(track_count, sizeof(track_t));
  memcpy(copy, tracks, track_count * sizeof(track_t));

  engine->tracks = copy;
  engine->track_count = track_count;

  engine_init_voices(engine);
  engine_init_channels(engine);
}

void engine_terminate(engine_t *engine) {
  free(engine->tracks);

  engine->tracks = NULL;
  engine->track_count = 0;
}

int engine_spin(engine_t *engine) {

  uint32_t now = get_now();

  int eof = 0;
  for (size_t i = 0; i < engine->track_count; ++i)
    eof &= track_spin(engine, engine->tracks + i, now);

  return eof;
}
