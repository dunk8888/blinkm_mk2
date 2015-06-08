

#include "config.h"
#include "led_fader.h"

// declared by user of led_fader
extern rgb_t leds[];
extern fader_t faders[];


//
inline uint8_t scale8( uint8_t i, fract8 scale)
{
    return ((uint16_t)i * (uint16_t)(scale) ) >> 8;
}

// blend between a start color and a destination color
// by an 8-bit "fractional" amount ranging from 0-255 (0=start, 255=dest)
// return true when at top of fader
bool led_blend( rgb_t* curr, rgb_t* start, rgb_t* dest, fract8 blend_amount )
{
    if( blend_amount == 0) {
        curr->r = start->r;
        curr->g = start->g;
        curr->b = start->b;
    }
    else if( blend_amount == 255) {
        curr->r = dest->r;
        curr->g = dest->g;
        curr->b = dest->b;
        return true;
    }
    else {
        fract8 keep_amount = 256 - blend_amount;
        
        curr->r = scale8( curr->r,   keep_amount) +
                  scale8( dest->r,   blend_amount);

        curr->g = scale8( curr->g,   keep_amount) +
                  scale8( dest->g,   blend_amount);

        curr->b = scale8( curr->b,   keep_amount) +
                  scale8( dest->b,   blend_amount);
    }
    return false;
}

// given a duration,
// compute how much a fader from 0-65535 will travel per msec tick
// to complete range in fade_millis
// compute the fractional amount the fader will move for each (1 or 10) msec tick
//
inline uint16_t led_compute_faderposinc( uint8_t millistep, uint16_t fade_millis )
{
    uint16_t bf = ((uint32_t)65535 * millistep ) / fade_millis;
    return bf;
}


//
// Update all the faders, update the current state of all LEDs
// called every tick (e.g. every 10 msec)
//
void led_update_faders()
{
    uint8_t i;
    for( i=0; i< NUM_LEDS; i++ ) {
        //dbgln("led:");
        rgb_t* cur   = &leds[i];
        rgb_t* start = &(faders[i].last);
        rgb_t* end   = &(faders[i].dest);
        uint8_t faderpos = faders[i].faderpos/256;
        
        bool donefading = led_blend( cur, start, end, faderpos );  // do next increment of fade
        if( !donefading ) 
            faders[i].faderpos += faders[i].faderposinc;
        else {
            cur->r = end->r;
            cur->g = end->g;
            cur->b = end->b;
        }
    }
}
