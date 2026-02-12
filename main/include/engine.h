#pragma once

#include <stddef.h>
#include <stdint.h>

#include <driver/ledc.h>

#define MAX_VOICES                                                             \
  ((int)LEDC_TIMER_MAX < (int)LEDC_CHANNEL_MAX ? (int)LEDC_TIMER_MAX           \
                                               : (int)LEDC_CHANNEL_MAX)

typedef struct __event event_t;
typedef struct __track track_t;
typedef struct __voice voice_t;
typedef struct __engine engine_t;

typedef struct __event {
  uint32_t frequency;
  uint32_t duration;
} event_t;

typedef struct __track {
  uint8_t pin;

  const event_t *events;
  size_t event_count;

  uint32_t current_event;
  uint32_t current_event_time;

  voice_t *voice;
} track_t;

typedef struct __voice {
  uint32_t frequency;
  track_t *owner_track;

  ledc_timer_t timer;
  ledc_channel_t channel;
} voice_t;

typedef struct __engine {
  track_t *tracks;
  size_t track_count;

  voice_t voices[MAX_VOICES];
} engine_t;

void engine_init(engine_t *engine, const track_t *tracks, size_t track_count);
void engine_terminate(engine_t *engine);
int engine_spin(engine_t *engine);
