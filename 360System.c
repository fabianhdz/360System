#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "audio.h"
#include "log.h"
#include "sounds.h"
#include "ultrasonic.h"

#define NUM_SENSORS 2u
#define STATUS_LED_PIN 25u
#define DISTANCE_FILTER_SAMPLES 3u
#define MIN_VALID_DISTANCE_CM 2.0f
#define MAX_VALID_DISTANCE_CM 400.0f
#define MOTOR_MAX_DISTANCE_CM 308.8f
#define MOTOR_APPROACH_ON_CM 10.0f
#define MOTOR_APPROACH_OFF_CM 3.0f
#define CM_PER_FOOT 30.48f

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
    float distance_samples[DISTANCE_FILTER_SAMPLES];
    size_t next_sample;
    size_t sample_count;
    float filtered_distance_cm;
    bool filter_ready;
    bool motor_on;
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
    {.motor_pin = 2},
    {.motor_pin = 5}
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

static bool distance_is_valid(float distance_cm)
{
    return distance_cm >= MIN_VALID_DISTANCE_CM &&
           distance_cm <= MAX_VALID_DISTANCE_CM;
}

static float median_of_three(float first, float second, float third)
{
    if (first > second) {
        float temporary = first;
        first = second;
        second = temporary;
    }
    if (second > third) {
        float temporary = second;
        second = third;
        third = temporary;
    }
    if (first > second) {
        second = first;
    }

    return second;
}

static bool filter_distance(application_sensor_t *sensor, float distance_cm)
{
    sensor->distance_samples[sensor->next_sample] = distance_cm;
    sensor->next_sample =
        (sensor->next_sample + 1u) % DISTANCE_FILTER_SAMPLES;

    if (sensor->sample_count < DISTANCE_FILTER_SAMPLES) {
        ++sensor->sample_count;
    }

    if (sensor->sample_count < DISTANCE_FILTER_SAMPLES) {
        return false;
    }

    sensor->filtered_distance_cm = median_of_three(
        sensor->distance_samples[0],
        sensor->distance_samples[1],
        sensor->distance_samples[2]
    );
    return true;
}

static void signal_motor(
    application_sensor_t *sensor,
    float previous_distance,
    float new_distance
)
{
    float approach_distance = previous_distance - new_distance;

    if (new_distance > MOTOR_MAX_DISTANCE_CM) {
        sensor->motor_on = false;
    } else if (!sensor->motor_on &&
               approach_distance >= MOTOR_APPROACH_ON_CM) {
        sensor->motor_on = true;
    } else if (sensor->motor_on &&
               approach_distance <= MOTOR_APPROACH_OFF_CM) {
        sensor->motor_on = false;
    }

    gpio_put(sensor->motor_pin, sensor->motor_on);
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
    uint32_t whole_feet;

    if (new_distance < CM_PER_FOOT) {
        return;
    }

    whole_feet = (uint32_t)(new_distance / CM_PER_FOOT);
    if (whole_feet >= (uint32_t)AUDIO_ONE_FOOT &&
        whole_feet <= (uint32_t)AUDIO_TEN_FEET) {
        queue_audio((audio_message_t)whole_feet);
    }
}

static void handle_ultrasonic_event(const ultrasonic_event_t *event)
{
    application_sensor_t *sensor;
    float previous_distance;
    bool previously_ready;

    if (event->sensor_index >= NUM_SENSORS) {
        return;
    }

    sensor = &application_sensors[event->sensor_index];

    if (event->type == ULTRASONIC_EVENT_TIMEOUT) {
        sensor->motor_on = false;
        sensor->filter_ready = false;
        sensor->sample_count = 0;
        sensor->next_sample = 0;
        gpio_put(sensor->motor_pin, 0);
        LOG(
            "Sensor %u: echo timeout\n",
            (unsigned int)event->sensor_index
        );
        return;
    }

    if (!distance_is_valid(event->distance_cm)) {
        LOG(
            "Sensor %u: invalid distance %.2f cm\n",
            (unsigned int)event->sensor_index,
            event->distance_cm
        );
        return;
    }

    previously_ready = sensor->filter_ready;
    previous_distance = sensor->filtered_distance_cm;
    sensor->filter_ready = filter_distance(sensor, event->distance_cm);

    LOG(
        "Sensor %u: raw = %.2f cm",
        (unsigned int)event->sensor_index,
        event->distance_cm
    );

    if (!sensor->filter_ready) {
        LOG(" (filter warming up)\n");
        return;
    }

    LOG(", filtered = %.2f cm\n", sensor->filtered_distance_cm);

    if (previously_ready) {
        signal_motor(
            sensor,
            previous_distance,
            sensor->filtered_distance_cm
        );
    }

    if (event->sensor_index == 0u) {
        signal_audio(sensor->filtered_distance_cm);
    }
}

static bool setup_hardware(void)
{
#if ENABLE_DEBUG_LOGS
    stdio_init_all();
#endif

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
        LOG("Failed to initialize ultrasonic sensors\n");
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
