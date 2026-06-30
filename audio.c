#include "audio.h"

#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#define AUDIO_PIN 27
#define AUDIO_SAMPLE_REPEAT 8u

static volatile size_t data_position = 0;
static volatile size_t data_length = 0;
static const uint8_t *volatile audio_data = NULL;

static void pwm_interrupt_handler(void);

void setup_audio(void)
{
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    uint audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    // Fire an interrupt after each PWM cycle to advance audio playback.
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Use the PWM duty cycle as the 8-bit audio sample amplitude.
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 6.74f);
    pwm_config_set_wrap(&config, 250);
    pwm_init(audio_pin_slice, &config, true);

    pwm_set_gpio_level(AUDIO_PIN, 0);
}

static void pwm_interrupt_handler(void)
{
    size_t position = data_position;

    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
    if (position < data_length * AUDIO_SAMPLE_REPEAT) {
        // Hold each sample for eight PWM cycles to produce the speech sample rate.
        pwm_set_gpio_level(
            AUDIO_PIN,
            audio_data[position / AUDIO_SAMPLE_REPEAT]
        );
        data_position = position + 1u;
    }
}

void play_audio(const uint8_t *audio_buffer, size_t length)
{
    // Prevent the PWM ISR from observing partially updated playback state.
    uint32_t interrupt_state = save_and_disable_interrupts();

    audio_data = audio_buffer;
    data_length = length;
    data_position = 0;
    restore_interrupts(interrupt_state);

    pwm_set_gpio_level(AUDIO_PIN, 0);
    while (data_position < data_length * AUDIO_SAMPLE_REPEAT) {
        // Sleep until the PWM interrupt advances playback.
        __wfi();
    }
    pwm_set_gpio_level(AUDIO_PIN, 0);
}
