#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "audio.h"
#include "sounds.h"
#include "ultrasonic.h"

#define NUM_SENSORS 2u
#define STATUS_LED_PIN 25u

typedef enum {
    AUDIO_ONE_FOOT = 1,
    AUDIO_TWO_FEET,
    AUDIO_THREE_FEET,
    AUDIO_FOUR_FEET,
    AUDIO_FIVE_FEET,
    AUDIO_SIX_FEET,
    AUDIO_SEVEN_FEET,
    AUDIO_EIGHT_FEET,
    AUDIO_NINE_FEET,
    AUDIO_TEN_FEET,
    AUDIO_MESSAGE_COUNT
} audio_message_t;

typedef struct {
    const uint8_t *data;
    size_t length;
} audio_clip_t;

typedef struct {
    uint8_t motor_pin;
    float distance_cm;
} application_sensor_t;

static const ultrasonic_sensor_config_t ultrasonic_sensors[NUM_SENSORS] = {
    {.trigger_pin = 0, .echo_pin = 1},
    {.trigger_pin = 3, .echo_pin = 4}
};

static const ultrasonic_timing_config_t ultrasonic_timing = {
    .trigger_low_us = 3,
    .trigger_high_us = 20,
    .echo_timeout_us = 30000,
    .cooldown_us = 60000
};

static const audio_clip_t audio_clips[AUDIO_MESSAGE_COUNT - 1] = {
    {.data = one_foot, .length = ONEFOOT_LENGTH},
    {.data = two_feet, .length = TWOFEET_LENGTH},
    {.data = three_feet, .length = THREEFEET_LENGTH},
    {.data = four_feet, .length = FOURFEET_LENGTH},
    {.data = five_feet, .length = FIVEFEET_LENGTH},
    {.data = six_feet, .length = SIXFEET_LENGTH},
    {.data = seven_feet, .length = SEVENFEET_LENGTH},
    {.data = eight_feet, .length = EIGHTFEET_LENGTH},
    {.data = nine_feet, .length = NINEFEET_LENGTH},
    {.data = ten_feet, .length = TENFEET_LENGTH}
};

static application_sensor_t application_sensors[NUM_SENSORS] = {
    {.motor_pin = 2, .distance_cm = 0.0f},
    {.motor_pin = 5, .distance_cm = 0.0f}
};

static void audio_core1_task(void)
{
    setup_audio();

    while (true) {
        uint32_t message = multicore_fifo_pop_blocking();

        if (message >= AUDIO_ONE_FOOT && message < AUDIO_MESSAGE_COUNT) {
            const audio_clip_t *clip = &audio_clips[message - AUDIO_ONE_FOOT];
            play_audio(clip->data, clip->length);
        }
    }
}

static void signal_motor(float previous_distance, float new_distance, size_t sensor_index)
{
    bool approaching = new_distance <= previous_distance - 10.0f;
    bool within_range = new_distance <= 308.8f;

    gpio_put(
        application_sensors[sensor_index].motor_pin,
        approaching && within_range
    );
}

static void queue_audio(audio_message_t message)
{
    // Drop the request instead of stalling sensor acquisition if audio is behind.
    if (multicore_fifo_wready()) {
        multicore_fifo_push_blocking((uint32_t)message);
    }
}

static void signal_audio(float new_distance)
{
    if (new_distance >= 30.48f && new_distance < 60.96f) {
        queue_audio(AUDIO_ONE_FOOT);
    } else if (new_distance >= 60.96f && new_distance < 91.44f) {
        queue_audio(AUDIO_TWO_FEET);
    } else if (new_distance >= 91.44f && new_distance < 121.92f) {
        queue_audio(AUDIO_THREE_FEET);
    }
}

static void handle_ultrasonic_event(const ultrasonic_event_t *event)
{
    application_sensor_t *sensor;
    float previous_distance;

    if (event->sensor_index >= NUM_SENSORS) {
        return;
    }

    sensor = &application_sensors[event->sensor_index];

    if (event->type == ULTRASONIC_EVENT_TIMEOUT) {
        gpio_put(sensor->motor_pin, 0);
        printf(
            "Sensor %u: echo timeout\n",
            (unsigned int)event->sensor_index
        );
        return;
    }

    previous_distance = sensor->distance_cm;
    sensor->distance_cm = event->distance_cm;

    printf(
        "Sensor %u: Distance = %.2f cm\n",
        (unsigned int)event->sensor_index,
        sensor->distance_cm
    );

    signal_motor(previous_distance, sensor->distance_cm, event->sensor_index);
    if (event->sensor_index == 0u) {
        signal_audio(sensor->distance_cm);
    }
}

static bool setup_hardware(void)
{
    stdio_init_all();

    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, 1);

    for (size_t i = 0; i < NUM_SENSORS; ++i) {
        gpio_init(application_sensors[i].motor_pin);
        gpio_set_dir(application_sensors[i].motor_pin, GPIO_OUT);
        gpio_put(application_sensors[i].motor_pin, 0);
    }

    return ultrasonic_init(
        ultrasonic_sensors,
        NUM_SENSORS,
        &ultrasonic_timing
    );
}

int main(void)
{
    ultrasonic_event_t event;

    if (!setup_hardware()) {
        printf("Failed to initialize ultrasonic sensors\n");
        return 1;
    }

    multicore_launch_core1(audio_core1_task);

    while (true) {
        ultrasonic_tick();
        while (ultrasonic_take_event(&event)) {
            handle_ultrasonic_event(&event);
        }
        tight_loop_contents();
    }
}
