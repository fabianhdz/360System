
#include "audio.h"


#define AUDIO_PIN 27



void setup_audio()
{
     gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    // Setup PWM interrupt to fire when PWM cycle is complete
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);
    // set the handle function above
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_interrupt_handler); 
    irq_set_enabled(PWM_IRQ_WRAP, true);

    // Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    /* 
     * 11 KHz is fine for speech. Phone lines generally sample at 8 KHz
     * 
     * 
     * So clkdiv should be as follows for given sample rate
     *  8.0f for 11 KHz
     *  4.0f for 22 KHz
     *  2.0f for 44 KHz etc
     */
    pwm_config_set_clkdiv(&config, 6.74f); 
    pwm_config_set_wrap(&config, 250); 
    pwm_init(audio_pin_slice, &config, true);

    pwm_set_gpio_level(AUDIO_PIN, 0);
}
void pwm_interrupt_handler() {
    pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));    
        // set pwm level 
    if(data_position<(data_length<<3)-1)
    {
        // allow the pwm value to repeat for 8 cycles this is >>3 
        pwm_set_gpio_level(AUDIO_PIN, audio_data[data_position>>3]);  
        data_position++;
    }
}
void play_audio(uint8_t *audio_buffer, int length) {
    data_position =0;
    if(data_length != length)
    {
        data_length = length;
        audio_data.assign(audio_buffer,audio_buffer+length);
    }
    pwm_set_gpio_level(AUDIO_PIN,0);
    while(data_position<(data_length<<3)-1)
    {
            __wfi();
    }
    pwm_set_gpio_level(AUDIO_PIN,0);
    
}


