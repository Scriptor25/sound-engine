#pragma once

#include "common.h"

#include <driver/ledc.h>

#define ENGINE_VOICE_MAX MIN((int)LEDC_TIMER_MAX, (int)LEDC_CHANNEL_MAX)
#define ENGINE_RESOLUTION_POWER 1

typedef struct __track track_t;
typedef struct __voice voice_t;
typedef struct __engine engine_t;

struct __track {
  const track_data_t *data;
  const envelope_data_t *envelope;

  uint32_t current_event;

  voice_t *voice, *cache;
};

struct __voice {
  ledc_timer_t timer;
  ledc_channel_t channel;

  uint32_t current_frequency, current_resolution;

  track_t *current_owner;
  uint32_t current_velocity, current_cooldown;
};

struct __engine {
  const envelope_data_t *envelopes;
  size_t envelope_count;

  track_t *tracks;
  size_t track_count;

  voice_t *voices;
  size_t voice_count;

  uint32_t max_velocity;
};

void engine_init(

    engine_t *engine,

    const envelope_data_t *envelopes, size_t envelope_count,

    const track_data_t *tracks, size_t track_count,

    const int *pins, size_t pin_count

);

void engine_terminate(engine_t *engine);

int engine_spin(engine_t *engine);
