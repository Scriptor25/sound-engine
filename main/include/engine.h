#pragma once

#include <stddef.h>
#include <stdint.h>

#include <driver/ledc.h>

typedef struct __event event_t;
typedef struct __track track_t;
typedef struct __engine engine_t;

struct __event {
  uint32_t frequency;
  uint32_t duration;
};

struct __track {
  uint8_t pin;

  const event_t *events;
  size_t event_count;

  uint32_t current_event;
  uint32_t current_event_time;

  ledc_timer_t timer;
  ledc_channel_t channel;
};

struct __engine {
  track_t *tracks;
  size_t track_count;
};

void engine_init(engine_t *engine, const track_t *tracks, size_t track_count);
void engine_terminate(engine_t *engine);
int engine_spin(engine_t *engine);
