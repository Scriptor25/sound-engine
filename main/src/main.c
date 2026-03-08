#include "engine.h"
#include "songs/golden.h"

#include <stdio.h>

#include <freertos/FreeRTOS.h>

const envelope_data_t ENVELOPES[] = {
    // piano
    {
        .program_from = 0,
        .program_to = 16,

        .attack = 5,
        .decay = 40,
        .sustain = 400,
        .release = 80,
    },
    // organ
    {
        .program_from = 16,
        .program_to = 24,

        .attack = 10,
        .decay = 0,
        .sustain = 1000,
        .release = 40,
    },
    // guitar
    {
        .program_from = 24,
        .program_to = 32,

        .attack = 5,
        .decay = 60,
        .sustain = 300,
        .release = 120,
    },
    // bass
    {
        .program_from = 32,
        .program_to = 40,

        .attack = 8,
        .decay = 70,
        .sustain = 500,
        .release = 120,
    },
    // strings
    {
        .program_from = 40,
        .program_to = 48,

        .attack = 120,
        .decay = 0,
        .sustain = 1000,
        .release = 200,
    },
    // choir
    {
        .program_from = 48,
        .program_to = 56,

        .attack = 60,
        .decay = 40,
        .sustain = 800,
        .release = 150,
    },
    // brass
    {
        .program_from = 56,
        .program_to = 64,

        .attack = 20,
        .decay = 80,
        .sustain = 700,
        .release = 120,
    },
    // reeds
    {
        .program_from = 64,
        .program_to = 72,

        .attack = 25,
        .decay = 60,
        .sustain = 700,
        .release = 120,
    },
    // flutes
    {
        .program_from = 72,
        .program_to = 80,

        .attack = 40,
        .decay = 20,
        .sustain = 900,
        .release = 120,
    },
    // synth leads
    {
        .program_from = 80,
        .program_to = 88,

        .attack = 5,
        .decay = 30,
        .sustain = 800,
        .release = 60,
    },
    // synth pads
    {
        .program_from = 88,
        .program_to = 96,

        .attack = 180,
        .decay = 0,
        .sustain = 1000,
        .release = 300,
    },
    // synth effects
    {
        .program_from = 96,
        .program_to = 104,

        .attack = 120,
        .decay = 40,
        .sustain = 700,
        .release = 200,
    },
    // ethnic
    {
        .program_from = 104,
        .program_to = 112,

        .attack = 10,
        .decay = 80,
        .sustain = 500,
        .release = 150,
    },
    // drums
    // {
    //     .program_from = 112,
    //     .program_to = 120,
    //
    //     .attack = 2,
    //     .decay = 120,
    //     .sustain = 0,
    //     .release = 0,
    // },
    // effects
    // {
    //     .program_from = 120,
    //     .program_to = 128,
    //
    //     .attack = 5,
    //     .decay = 200,
    //     .sustain = 0,
    //     .release = 0,
    // },
};

const int PINS[] = {0, 1, 2, 3};

void app_main(void) {

  engine_t engine;
  engine_init(

      &engine,

      ENVELOPES, sizeof(ENVELOPES) / sizeof(envelope_data_t),

      golden_data, sizeof(golden_data) / sizeof(track_data_t),

      PINS, sizeof(PINS) / sizeof(int)

  );

  printf("starting playback...\n");

  for (;;) {
    if (engine_spin(&engine))
      break;
    vTaskDelay(1);
  }

  printf("playback finished.\n");

  engine_terminate(&engine);
}
