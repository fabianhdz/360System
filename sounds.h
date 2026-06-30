#ifndef SOUNDS_H
#define SOUNDS_H

#include <stdint.h>

enum {
    ONEFOOT_LENGTH = 6821,
    TWOFEET_LENGTH = 6656,
    THREEFEET_LENGTH = 7481,
    FOURFEET_LENGTH = 8471,
    FIVEFEET_LENGTH = 9186,
    SIXFEET_LENGTH = 9076,
    SEVENFEET_LENGTH = 7811,
    EIGHTFEET_LENGTH = 6766,
    NINEFEET_LENGTH = 8526,
    TENFEET_LENGTH = 8031
};

extern const uint8_t one_foot[ONEFOOT_LENGTH];
extern const uint8_t two_feet[TWOFEET_LENGTH];
extern const uint8_t three_feet[THREEFEET_LENGTH];
extern const uint8_t four_feet[FOURFEET_LENGTH];
extern const uint8_t five_feet[FIVEFEET_LENGTH];
extern const uint8_t six_feet[SIXFEET_LENGTH];
extern const uint8_t seven_feet[SEVENFEET_LENGTH];
extern const uint8_t eight_feet[EIGHTFEET_LENGTH];
extern const uint8_t nine_feet[NINEFEET_LENGTH];
extern const uint8_t ten_feet[TENFEET_LENGTH];

#endif
