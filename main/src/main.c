#include <engine.h>
#include <where_is_my_mind.h>

#include <stdio.h>

#include <freertos/FreeRTOS.h>

void app_main(void) {

  engine_t engine;
  engine_init(&engine, where_is_my_mind_data,
              sizeof(where_is_my_mind_data) / sizeof(track_t));

  printf("starting playback...\n");

  for (;;) {
    if (engine_spin(&engine))
      break;
    vTaskDelay(1);
  }

  printf("playback finished.\n");

  engine_terminate(&engine);
}
