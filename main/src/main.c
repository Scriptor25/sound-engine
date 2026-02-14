#include <engine.h>
#include <golden.h>

#include <stdio.h>

#include <freertos/FreeRTOS.h>

const int PINS[] = {0, 1, 2, 3, 4, 5, 14, 15, 18, 19, 20, 21, 22};

void app_main(void) {

  engine_t engine;
  engine_init(

      &engine,

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
