
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "sounds.h"
#include "audio.h"


#define NUM_SENSORS 2

int trigger_pins[NUM_SENSORS] ={0,3};//pins 1,5,9, 26
int echo_pins[NUM_SENSORS] ={1,4};//2,6,10, 27
int motor_pins[NUM_SENSORS]={2,5};//4,7,11, 25
volatile float distance[NUM_SENSORS] = {0,0};
volatile uint32_t echo_start_times[NUM_SENSORS];
volatile uint32_t echo_end_times[NUM_SENSORS];
volatile bool pulse_received[NUM_SENSORS];
volatile uint32_t starttime[NUM_SENSORS];

void audio_core1_task(void)
{
    setup_audio();
    while(true)
    {
        uint32_t message = multicore_fifo_pop_blocking();

        switch(message)
        {
            case 1:
                play_audio(one_foot,ONEFOOT_LENGTH);
                break;
            case 2:
                play_audio(two_feet,TWOFEET_LENGTH);
                break;
            case 3:
                play_audio(three_feet,THREEFEET_LENGTH);
                break;
            case 4:
                play_audio(four_feet,FOURFEET_LENGTH);
                break;
            case 5:
                play_audio(five_feet,FIVEFEET_LENGTH);
                break;
            case 6:
                play_audio(six_feet,SIXFEET_LENGTH);
                break;
            case 7:
                play_audio(seven_feet,SEVENFEET_LENGTH);
                break;
            case 8:
                play_audio(eight_feet,EIGHTFEET_LENGTH);
                break;
            case 9:
                play_audio(nine_feet,NINEFEET_LENGTH);
                break;
            case 10:
                play_audio(ten_feet,TENFEET_LENGTH);
                break;
            
        }

    }
} 
void echo_isr(uint gpio, uint32_t events) {
   // static uint last_echo_pin = 0; //Used to stored the last pin that trigger an interrupt
    uint32_t now = time_us_32();  //current time

    // Determine which sensor triggered the interrupt
    uint sensor_index = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (echo_pins[i] == gpio) {
            sensor_index = i;
            break;
        }
    }

    if (events & GPIO_IRQ_EDGE_RISE) {
        // Rising edge: start of pulse
        echo_start_times[sensor_index] = now;
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        // Falling edge: end of pulse
        echo_end_times[sensor_index] = now;
        pulse_received[sensor_index] = true;
    }

    //last_echo_pin = gpio;
}
//checks if new pulses have been receive
bool any_pulse_received() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (pulse_received[i]) {
            return true;
        }
    }
    return false;
}

void signalMotor(float previous_Distance,float newDistance, int index)
{

     if((newDistance <= 308.8) && (newDistance <= previous_Distance-10))
    {
       gpio_put(motor_pins[index],1);
       
    }
    else
    {
         gpio_put(motor_pins[index],0);
    }
}
void signalaudio(float previous_Distance,float newDistance)
{
    if(newDistance>=30.48 && newDistance < 60.96)
    {
        multicore_fifo_push_blocking(1);
    }
    else if(newDistance >= 60.96 && newDistance < 91.44)
    {
         multicore_fifo_push_blocking(2);
    }
    else if(newDistance >= 91.44 && newDistance <121.92 )
    {
         multicore_fifo_push_blocking(3);
    }
}
void reset_sensor(int index)
{
    gpio_put(trigger_pins[index],0);
    sleep_us(3);
    gpio_put(trigger_pins[index],1);
    sleep_us(20);
    gpio_put(trigger_pins[index],0);
    starttime[index] = time_us_32();
}
void setup() {
    stdio_init_all();
        gpio_init(25);
        gpio_set_dir(25,GPIO_OUT);
    for (int i = 0; i < NUM_SENSORS; i++) {
        gpio_init(trigger_pins[i]);
        gpio_set_dir(trigger_pins[i], GPIO_OUT);
        gpio_init(echo_pins[i]);
        gpio_set_dir(echo_pins[i], GPIO_IN);
        gpio_init(motor_pins[i]);
        gpio_set_dir(motor_pins[i],GPIO_OUT);
        gpio_set_irq_enabled_with_callback(echo_pins[i], GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &echo_isr);
    }
}

void loop() {
    // Send trigger pulses
    gpio_put(25,1);
    uint32_t timeout = 5000000;// 5 seconds
    for (int i = 0; i < NUM_SENSORS; i++) {
        gpio_put(trigger_pins[i],0);
        sleep_us(3);
        gpio_put(trigger_pins[i], 1);
        sleep_us(20);
        gpio_put(trigger_pins[i], 0);
        pulse_received[i] = false;
        starttime[i] = time_us_32();

    }
    float previousDistance =0;
    // Wait for pulses to be received
    while (true) {
        for (int i = 0; i < NUM_SENSORS; i++) {
            if (pulse_received[i]) {
                previousDistance = distance[i];
                uint32_t pulse_duration = echo_end_times[i] - echo_start_times[i];
                distance[i] = pulse_duration * 0.0343 / 2.0;
                printf("Sensor %d: Distance = %.2f cm\n", i, distance[i]);
                signalMotor(previousDistance,distance[i],i);
                if(i == 0){
                    signalaudio(previousDistance,distance[i]);
                 }
                sleep_ms(500);
                 gpio_put(trigger_pins[i],0);
                 sleep_us(3);
                 gpio_put(trigger_pins[i], 1);
                 sleep_us(20);
                 gpio_put(trigger_pins[i], 0);
                 pulse_received[i] = false;
                 starttime[i] = time_us_32();   
            }
            else
            {
                if(time_us_32()-starttime[i] > timeout)//if hasnt receive pulse for longer than 2 seconds
                {
                    reset_sensor(i);
                }
            }
        }

        if (!any_pulse_received()) {
            sleep_ms(1);

        }
    }
}
int main() {
    
    setup();
    multicore_launch_core1(audio_core1_task);
    while (true) {
        loop();
    }
    return 0;
}