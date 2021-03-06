

#ifndef LEDFader_h
#define LEDFader_h

#include <inttypes.h>

#include "pixeltypes.h"

//#include "printf_wrapper.h"

// RGB triplet of 8-bit vals for input/output use
typedef union {
    struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    }; 
    uint8_t raw[3];
} rgb_t;

// RGB triplet unsigned ints for internal use of 100x scale
// used instead of floating point
typedef union {
    struct {
        int16_t r;
        int16_t g;
        int16_t b;
    };
    int16_t raw[3];
} rgbint_t;

typedef struct {
    rgb_t dest;  // the eventual destination color we want to hit
    rgbint_t m100x;  // slope of the amount of to move each tick
    uint16_t stepcnt;  // number of steps for our fade run
} rgbfader_t;


class LEDFader
{
    uint8_t nLEDs;
    rgbfader_t* faders;
    CRGB* leds;

    void setDestN(rgb_t newcolor, int steps, uint8_t ledn);
    uint8_t getNewVal(uint16_t stepcnt, int16_t m100x, uint8_t dest, uint8_t curr);

 public:
    LEDFader(CRGB* nleds, rgbfader_t* nfaders, uint8_t nnLEDs) 
        { leds = nleds; faders = nfaders; nLEDs = nnLEDs; }

    void setCurr(rgb_t* color, uint16_t ledn);
    void setDest(rgb_t color, int steps, uint16_t ledn);
    void update(void);
};



#endif
