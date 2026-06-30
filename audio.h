#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

void setup_audio(void);
void play_audio(const uint8_t *audio_buffer, size_t length);

#endif
