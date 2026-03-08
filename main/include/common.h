#pragma once

#include <stddef.h>
#include <stdint.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

#define CLAMP(T, TMIN, TMAX) MAX((TMIN), MIN((TMAX), (T)))

#define MAP(TY, FTY, SMIN, SMAX, DMIN, DMAX, T)                                \
  ((TY)((FTY)(((DMAX) - (DMIN)) * ((T) - (SMIN))) / (FTY)((SMAX) - (SMIN)) +   \
        (FTY)(DMIN)))

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

uint32_t get_now();
uint32_t transpose_frequency(uint32_t frequency, int transpose);
uint32_t calculate_velocity(const envelope_data_t *envelope, uint32_t now,
                            uint32_t velocity, uint32_t begin,
                            uint32_t duration);
const envelope_data_t *find_envelope(const envelope_data_t *envelopes,
                                     size_t envelope_count, uint32_t program);
