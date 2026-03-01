#pragma once

#include <stddef.h>
#include <stdint.h>

#include <driver/ledc.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define MAP(TY, FTY, SMIN, SMAX, DMIN, DMAX, T)                                \
  ((TY)((FTY)(((DMAX) - (DMIN)) * ((T) - (SMIN))) / (FTY)((SMAX) - (SMIN)) +   \
        (FTY)(DMIN)))

#define ENGINE_VOICE_MAX MIN(LEDC_TIMER_MAX, LEDC_CHANNEL_MAX)
#define ENGINE_VOICE_COOLDOWN 100

typedef struct __event_data event_data_t;
typedef struct __track_data track_data_t;
typedef struct __envelope_data envelope_data_t;

struct __event_data {
  uint32_t frequency;
  uint32_t time;
  uint32_t duration;
  uint32_t velocity;
};

struct __track_data {
  uint32_t program;

  const event_data_t *events;
  size_t event_count;

  int transpose;
};

struct __envelope_data {
  uint32_t program_from;
  uint32_t program_to;

  uint32_t attack;
  uint32_t decay;
  uint32_t sustain;
  uint32_t release;
};

typedef struct __track track_t;
typedef struct __voice voice_t;
typedef struct __engine engine_t;

struct __track {
  const track_data_t *data;

  uint32_t current_event;

  const envelope_data_t *envelope;

  voice_t *voice;
  voice_t *cache;
};

struct __voice {
  track_t *owner;

  ledc_timer_t timer;
  ledc_channel_t channel;

  uint32_t current_frequency, current_resolution;
  uint32_t current_velocity;
  uint32_t cooldown_end;
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
