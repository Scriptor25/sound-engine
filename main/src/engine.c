#include <engine.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/ledc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t get_now() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

static void voice_set(track_t *track, uint32_t frequency, uint32_t duty) {
  if (frequency)
    ledc_set_freq(LEDC_LOW_SPEED_MODE, track->timer, frequency);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, track->channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, track->channel);
}

static int track_spin(engine_t *engine, track_t *track, uint32_t now) {

  const event_t *event;

  if (track->current_event >= track->event_count)
    return 1;

  event = track->events + track->current_event;

  if ((now - track->current_event_time) < event->duration)
    return 0;

  if (++track->current_event >= track->event_count) {
    voice_set(track, 0, 0);
    return 1;
  }

  track->current_event_time = now;
  event = track->events + track->current_event;

  if (event->frequency)
    voice_set(track, event->frequency, 24);
  else
    voice_set(track, 0, 0);

  return 0;
}

void engine_init(engine_t *engine, const track_t *tracks, size_t track_count) {

  track_t *copy = calloc(track_count, sizeof(track_t));
  memcpy(copy, tracks, track_count * sizeof(track_t));

  engine->tracks = copy;
  engine->track_count = track_count;

  for (size_t i = 0; i < engine->track_count; ++i) {

    track_t *track = engine->tracks + i;

    track->current_event = 0;
    track->current_event_time = get_now();

    track->timer = i;
    track->channel = i;

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = track->timer,
        .freq_hz = 1000,
        .clk_cfg = LEDC_USE_PLL_DIV_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = track->pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = track->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = track->timer,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
  }
}

void engine_terminate(engine_t *engine) {
  free(engine->tracks);

  engine->tracks = NULL;
  engine->track_count = 0;
}

int engine_spin(engine_t *engine) {

  uint32_t now = get_now();

  int eof = 1;
  for (size_t i = 0; i < engine->track_count; ++i)
    eof &= track_spin(engine, engine->tracks + i, now);

  return eof;
}
