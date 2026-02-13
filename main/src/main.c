#include <engine.h>
#include <golden.h>

#include <stdio.h>

#include <freertos/FreeRTOS.h>

void app_main(void) {

  engine_t engine;
  engine_init(&engine, golden_data, sizeof(golden_data) / sizeof(track_t));

  printf("starting playback...\n");

  for (;;) {
    if (engine_spin(&engine))
      break;
    vTaskDelay(1);
  }

  printf("playback finished.\n");

  engine_terminate(&engine);
}
