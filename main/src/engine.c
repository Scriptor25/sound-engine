#include <engine.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/ledc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t get_now() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

static void voice_set(track_t *track, uint32_t frequency) {

  int error;
  if (frequency) {
    error = ledc_set_freq(LEDC_LOW_SPEED_MODE, track->timer, frequency) &&
            ledc_set_freq(LEDC_LOW_SPEED_MODE, track->timer, frequency << 1) &&
            ledc_set_freq(LEDC_LOW_SPEED_MODE, track->timer, frequency >> 1);
  } else {
    error = 1;
  }

  uint32_t duty;
  if (error) {
    duty = 0;
  } else {
    duty = 64;
  }

  ledc_set_duty(LEDC_LOW_SPEED_MODE, track->channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, track->channel);
}

static int track_spin(engine_t *engine, track_t *track, uint32_t now) {

  const event_data_t *event;

  if (track->current_event >= track->event_count)
    return 1;

  event = track->events + track->current_event;

  if (!track->active && now >= event->time) {
    track->active = 1;
    voice_set(track, event->frequency);
  }

  if (now < event->time + event->duration)
    return 0;

  while (now >= event->time + event->duration) {
    if (++track->current_event >= track->event_count) {
      track->active = 0;
      voice_set(track, 0);
      return 1;
    }

    event = track->events + track->current_event;
  }

  if (now < event->time) {
    track->active = 0;
    voice_set(track, 0);
    return 0;
  }

  track->active = 1;
  voice_set(track, event->frequency);
  return 0;
}

void engine_init(

    engine_t *engine,

    const track_data_t *tracks, size_t track_count,

    const int *pins, size_t pin_count

) {

  for (size_t i = 0; i < MIN(pin_count, ENGINE_VOICE_MAX); ++i) {

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = (ledc_timer_t)i,
        .freq_hz = 1000,
        .clk_cfg = LEDC_USE_PLL_DIV_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = pins[i],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)i,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)i,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
  }

  engine->tracks = (track_t *)calloc(track_count, sizeof(track_t));
  engine->track_count = track_count;

  for (size_t i = 0; i < engine->track_count; ++i) {

    const track_data_t *track_data = tracks + i;
    track_t *track = engine->tracks + i;

    track->events = track_data->events;
    track->event_count = track_data->event_count;

    track->current_event = 0;
    track->active = 0;

    track->timer = (ledc_timer_t)i;
    track->channel = (ledc_channel_t)i;
  }
}

void engine_terminate(engine_t *engine) {

  free(engine->tracks);

  engine->track_count = 0;
  engine->tracks = NULL;
}

int engine_spin(engine_t *engine) {

  uint32_t now = get_now();

  int end = 1;
  for (size_t i = 0; i < engine->track_count; ++i)
    end &= track_spin(engine, engine->tracks + i, now);

  return end;
}
