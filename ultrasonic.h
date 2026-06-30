#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ULTRASONIC_MAX_SENSORS 8u

typedef struct {
    uint8_t trigger_pin;
    uint8_t echo_pin;
} ultrasonic_sensor_config_t;

typedef struct {
    uint32_t trigger_low_us;
    uint32_t trigger_high_us;
    uint32_t echo_timeout_us;
    uint32_t cooldown_us;
} ultrasonic_timing_config_t;

typedef enum {
    ULTRASONIC_EVENT_DISTANCE,
    ULTRASONIC_EVENT_TIMEOUT
} ultrasonic_event_type_t;

typedef struct {
    ultrasonic_event_type_t type;
    size_t sensor_index;
    float distance_cm;
} ultrasonic_event_t;

bool ultrasonic_init(
    const ultrasonic_sensor_config_t *sensors,
    size_t sensor_count,
    const ultrasonic_timing_config_t *timing
);
void ultrasonic_tick(void);
bool ultrasonic_take_event(ultrasonic_event_t *event);

#endif
