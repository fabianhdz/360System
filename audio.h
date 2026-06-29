#ifndef AUDIO_H
#define AUDIO_H
#include <stdio.h>
#include <vector>
#include "hardware/irq.h"  // interrupts
#include "hardware/pwm.h"  // pwm 
#include "hardware/sync.h" // wait for interrupt 
#include "pico/stdlib.h"

static int data_position =0;
static int data_length =0;
static std::vector<uint8_t> audio_data;

void pwm_interrupt_handler();
void setup_audio();
void play_audio(uint8_t *audio_buffer,int length);





#endif