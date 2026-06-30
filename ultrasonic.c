#include "ultrasonic.h"

#include <limits.h>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#define SPEED_OF_SOUND_CM_PER_US 0.0343f

typedef struct {
    volatile uint32_t echo_start_us;
    volatile uint32_t echo_end_us;
    volatile bool echo_started;
    volatile bool pulse_received;
} ultrasonic_capture_t;

typedef enum {
    MEASUREMENT_IDLE,
    MEASUREMENT_TRIGGER_LOW,
    MEASUREMENT_TRIGGER_HIGH,
    MEASUREMENT_WAIT_ECHO,
    MEASUREMENT_COOLDOWN
} measurement_phase_t;

static ultrasonic_sensor_config_t sensor_configs[ULTRASONIC_MAX_SENSORS];
static ultrasonic_capture_t sensor_captures[ULTRASONIC_MAX_SENSORS];
static ultrasonic_timing_config_t timing_config;
static size_t configured_sensor_count;
static volatile uint32_t active_sensor_index;
static volatile measurement_phase_t measurement_phase = MEASUREMENT_IDLE;
static uint32_t phase_deadline_us;
static ultrasonic_event_t pending_event;
static bool event_pending;

static bool deadline_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static bool timing_is_valid(const ultrasonic_timing_config_t *timing)
{
    return timing->trigger_low_us > 0u &&
           timing->trigger_high_us > 0u &&
           timing->echo_timeout_us > 0u &&
           timing->cooldown_us > 0u &&
           timing->trigger_low_us <= INT32_MAX &&
           timing->trigger_high_us <= INT32_MAX &&
           timing->echo_timeout_us <= INT32_MAX &&
           timing->cooldown_us <= INT32_MAX;
}

static int sensor_index_from_echo_pin(uint gpio)
{
    for (size_t i = 0; i < configured_sensor_count; ++i) {
        if ((uint)sensor_configs[i].echo_pin == gpio) {
            return (int)i;
        }
    }

    return -1;
}

static void echo_isr(uint gpio, uint32_t events)
{
    int sensor_index = sensor_index_from_echo_pin(gpio);

    if (sensor_index < 0 ||
        (uint32_t)sensor_index != active_sensor_index ||
        measurement_phase != MEASUREMENT_WAIT_ECHO) {
        return;
    }

    ultrasonic_capture_t *capture = &sensor_captures[sensor_index];
    uint32_t now = time_us_32();

    if ((events & GPIO_IRQ_EDGE_RISE) != 0u) {
        capture->echo_start_us = now;
        capture->echo_started = true;
    }

    if ((events & GPIO_IRQ_EDGE_FALL) != 0u && capture->echo_started) {
        capture->echo_end_us = now;
        capture->pulse_received = true;
    }
}

static void begin_measurement(uint32_t sensor_index, uint32_t now)
{
    ultrasonic_capture_t *capture = &sensor_captures[sensor_index];
    uint32_t interrupt_state;

    gpio_put(sensor_configs[sensor_index].trigger_pin, 0);

    // Clear the ISR handoff before starting a new trigger pulse.
    interrupt_state = save_and_disable_interrupts();
    active_sensor_index = sensor_index;
    capture->echo_started = false;
    capture->pulse_received = false;
    measurement_phase = MEASUREMENT_TRIGGER_LOW;
    restore_interrupts(interrupt_state);

    phase_deadline_us = now + timing_config.trigger_low_us;
}

static bool take_received_pulse(uint32_t sensor_index, uint32_t *pulse_duration_us)
{
    ultrasonic_capture_t *capture = &sensor_captures[sensor_index];
    uint32_t interrupt_state = save_and_disable_interrupts();
    bool received = capture->pulse_received;

    if (received) {
        *pulse_duration_us = capture->echo_end_us - capture->echo_start_us;
        capture->pulse_received = false;
        capture->echo_started = false;
        measurement_phase = MEASUREMENT_COOLDOWN;
    }

    restore_interrupts(interrupt_state);
    return received;
}

static void cancel_echo_wait(uint32_t sensor_index)
{
    uint32_t interrupt_state = save_and_disable_interrupts();

    sensor_captures[sensor_index].pulse_received = false;
    sensor_captures[sensor_index].echo_started = false;
    measurement_phase = MEASUREMENT_COOLDOWN;

    restore_interrupts(interrupt_state);
}

static void publish_distance(uint32_t sensor_index, uint32_t pulse_duration_us)
{
    pending_event.type = ULTRASONIC_EVENT_DISTANCE;
    pending_event.sensor_index = sensor_index;
    pending_event.distance_cm =
        (float)pulse_duration_us * SPEED_OF_SOUND_CM_PER_US / 2.0f;
    event_pending = true;
}

static void publish_timeout(uint32_t sensor_index)
{
    pending_event.type = ULTRASONIC_EVENT_TIMEOUT;
    pending_event.sensor_index = sensor_index;
    pending_event.distance_cm = 0.0f;
    event_pending = true;
}

bool ultrasonic_init(
    const ultrasonic_sensor_config_t *sensors,
    size_t sensor_count,
    const ultrasonic_timing_config_t *timing
)
{
    if (sensors == NULL ||
        timing == NULL ||
        sensor_count == 0u ||
        sensor_count > ULTRASONIC_MAX_SENSORS ||
        !timing_is_valid(timing)) {
        return false;
    }

    configured_sensor_count = sensor_count;
    timing_config = *timing;
    active_sensor_index = 0;
    measurement_phase = MEASUREMENT_IDLE;
    event_pending = false;

    for (size_t i = 0; i < sensor_count; ++i) {
        sensor_configs[i] = sensors[i];
        sensor_captures[i].echo_started = false;
        sensor_captures[i].pulse_received = false;

        gpio_init(sensor_configs[i].trigger_pin);
        gpio_set_dir(sensor_configs[i].trigger_pin, GPIO_OUT);
        gpio_put(sensor_configs[i].trigger_pin, 0);

        gpio_init(sensor_configs[i].echo_pin);
        gpio_set_dir(sensor_configs[i].echo_pin, GPIO_IN);
        gpio_set_irq_enabled_with_callback(
            sensor_configs[i].echo_pin,
            GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
            true,
            echo_isr
        );
    }

    return true;
}

void ultrasonic_tick(void)
{
    uint32_t now = time_us_32();
    uint32_t sensor_index = active_sensor_index;

    switch (measurement_phase) {
        case MEASUREMENT_IDLE:
            begin_measurement(0, now);
            break;

        case MEASUREMENT_TRIGGER_LOW:
            if (deadline_reached(now, phase_deadline_us)) {
                gpio_put(sensor_configs[sensor_index].trigger_pin, 1);
                measurement_phase = MEASUREMENT_TRIGGER_HIGH;
                phase_deadline_us = now + timing_config.trigger_high_us;
            }
            break;

        case MEASUREMENT_TRIGGER_HIGH:
            if (deadline_reached(now, phase_deadline_us)) {
                gpio_put(sensor_configs[sensor_index].trigger_pin, 0);
                measurement_phase = MEASUREMENT_WAIT_ECHO;
                phase_deadline_us = now + timing_config.echo_timeout_us;
            }
            break;

        case MEASUREMENT_WAIT_ECHO: {
            uint32_t pulse_duration_us;

            if (take_received_pulse(sensor_index, &pulse_duration_us)) {
                publish_distance(sensor_index, pulse_duration_us);
                phase_deadline_us = time_us_32() + timing_config.cooldown_us;
            } else if (deadline_reached(now, phase_deadline_us)) {
                cancel_echo_wait(sensor_index);
                publish_timeout(sensor_index);
                phase_deadline_us = time_us_32() + timing_config.cooldown_us;
            }
            break;
        }

        case MEASUREMENT_COOLDOWN:
            if (!event_pending && deadline_reached(now, phase_deadline_us)) {
                uint32_t next_sensor =
                    (sensor_index + 1u) % (uint32_t)configured_sensor_count;
                begin_measurement(next_sensor, now);
            }
            break;
    }
}

bool ultrasonic_take_event(ultrasonic_event_t *event)
{
    if (event == NULL || !event_pending) {
        return false;
    }

    *event = pending_event;
    event_pending = false;
    return true;
}
