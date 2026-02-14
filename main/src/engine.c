#include <engine.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/ledc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static uint32_t get_now() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

static int find_suitable_resolution(uint32_t frequency,
                                    uint32_t clock_frequency,
                                    uint32_t *result) {
  for (int resolution = LEDC_TIMER_BIT_MAX - 1; resolution >= LEDC_TIMER_1_BIT;
       --resolution) {

    uint32_t divider = clock_frequency / (frequency * (1 << resolution));
    if (divider >= 1 && divider < 1024) {

      (void)(result && (*result = resolution));

      return 1;
    }
  }

  return 0;
}

static void set_voice(track_t *track, uint32_t frequency) {

  if (!frequency) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, track->channel, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, track->channel);
    return;
  }

  if (track->transpose < 0) {
    frequency >>= -track->transpose;
  } else if (track->transpose > 0) {
    frequency <<= track->transpose;
  }

  uint32_t duty, resolution;
  if (track->current_frequency == frequency) {
    duty = ((1 << track->current_resolution) - 1) >> 4;
  } else if (find_suitable_resolution(frequency, 80000000, &resolution)) {

    if (track->current_resolution == resolution) {
      ledc_set_freq(LEDC_LOW_SPEED_MODE, track->timer, frequency);
    } else {
      ledc_timer_config_t timer = {
          .speed_mode = LEDC_LOW_SPEED_MODE,
          .duty_resolution = resolution,
          .timer_num = track->timer,
          .freq_hz = frequency,
          .clk_cfg = LEDC_USE_PLL_DIV_CLK,
      };
      ledc_timer_config(&timer);
    }

    duty = ((1 << resolution) - 1) >> 4;

    track->current_frequency = frequency;
    track->current_resolution = resolution;
  } else {
    duty = 0;
  }

  ledc_set_duty(LEDC_LOW_SPEED_MODE, track->channel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, track->channel);
}

static int track_spin(engine_t *engine, track_t *track, uint32_t now) {

  const event_data_t *event;

  if (track->current_event >= track->event_count)
    return 1;

  event = track->events + track->current_event;

  if (now < event->time + event->duration) {
    if (!track->active && now >= event->time) {
      track->active = 1;
      set_voice(track, event->frequency);
    }
    return 0;
  }

  while (now >= event->time + event->duration) {
    if (++track->current_event >= track->event_count) {
      track->active = 0;
      set_voice(track, 0);
      return 1;
    }

    event = track->events + track->current_event;
  }

  if (now < event->time) {
    track->active = 0;
    set_voice(track, 0);
    return 0;
  }

  track->active = 1;
  set_voice(track, event->frequency);
  return 0;
}

void engine_init(

    engine_t *engine,

    const track_data_t *tracks, size_t track_count,

    const int *pins, size_t pin_count

) {

  size_t count = MIN(track_count, MIN(pin_count, ENGINE_VOICE_MAX));

  for (size_t i = 0; i < count; ++i) {
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

  engine->tracks = (track_t *)calloc(count, sizeof(track_t));
  engine->track_count = count;

  for (size_t i = 0; i < engine->track_count; ++i) {

    const track_data_t *track_data = tracks + i;
    track_t *track = engine->tracks + i;

    track->events = track_data->events;
    track->event_count = track_data->event_count;

    track->transpose = track_data->transpose;

    track->current_event = 0;
    track->active = 0;

    track->timer = (ledc_timer_t)i;
    track->channel = (ledc_channel_t)i;

    track->current_frequency = 0;
    track->current_resolution = 0;
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
