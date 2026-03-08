#include "common.h"

#include <freertos/FreeRTOS.h>

uint32_t get_now() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

uint32_t transpose_frequency(uint32_t frequency, int transpose) {

  if (transpose < 0) {
    return frequency >> -transpose;
  }

  if (transpose > 0) {
    return frequency << transpose;
  }

  return frequency;
}

uint32_t calculate_velocity(const envelope_data_t *envelope, uint32_t now,
                            uint32_t velocity, uint32_t begin,
                            uint32_t duration) {
  uint32_t age, mod, d, delta, r;

  age = now - begin;
  mod = 1000;

  if (age < envelope->attack) {
    mod = (age * 1000) / envelope->attack;
  } else if (age < envelope->attack + envelope->decay) {
    d = age - envelope->attack;
    delta = 1000 - envelope->sustain;

    mod = 1000 - (d * delta) / envelope->decay;
  } else if (age < duration - envelope->release) {
    mod = envelope->sustain;
  } else if (age < duration) {
    r = age - (duration - envelope->release);

    mod = (envelope->sustain * (envelope->release - r)) / envelope->release;
  } else {
    mod = 0;
  }

  return (velocity * mod) / 1000;
}

const envelope_data_t *find_envelope(const envelope_data_t *envelopes,
                                     size_t envelope_count, uint32_t program) {
  size_t i;

  for (i = 0; i < envelope_count; ++i) {
    const envelope_data_t *envelope = envelopes + i;

    if (envelope->program_from <= program && program < envelope->program_to) {
      return envelope;
    }
  }

  return NULL;
}
