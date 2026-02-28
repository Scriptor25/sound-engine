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
#define ENGINE_VOICE_STEALING true
#define ENGINE_VELOCITY_MAX 127

typedef struct __event_data event_data_t;
typedef struct __track_data track_data_t;

struct __event_data {
  uint32_t frequency;
  uint32_t time;
  uint32_t duration;
  uint32_t velocity;
};

struct __track_data {
  const event_data_t *events;
  size_t event_count;

  int transpose;
};

typedef struct __track track_t;
typedef struct __voice voice_t;
typedef struct __engine engine_t;

struct __track {
  const event_data_t *events;
  size_t event_count;

  int transpose;

  uint32_t current_event;
  int active;

  voice_t *voice;
  voice_t *cache;
};

struct __voice {
  track_t *owner;

  ledc_timer_t timer;
  ledc_channel_t channel;

  uint32_t current_frequency, current_resolution;
  uint32_t current_velocity;
};

struct __engine {
  track_t *tracks;
  size_t track_count;

  voice_t *voices;
  size_t voice_count;
};

void engine_init(

    engine_t *engine,

    const track_data_t *tracks, size_t track_count,

    const int *pins, size_t pin_count

);

void engine_terminate(engine_t *engine);

int engine_spin(engine_t *engine);
