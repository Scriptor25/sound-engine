#include <engine.h>

#include <golden.h>

void app_main(void) {

  engine_t engine;
  engine_init(&engine, golden_data, sizeof(golden_data) / sizeof(track_t));

  for (; !engine_spin(&engine);)
    ;

  engine_terminate(&engine);
}
