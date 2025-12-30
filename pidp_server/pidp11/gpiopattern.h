/* gpiopattern.h: pattern generator, transforms data between Blinkenlight APi and gpio-MUX

   Copyright (c) 2016, Joerg Hoppe
   j_hoppe@t-online.de, www.retrocmp.com

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   JOERG HOPPE BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   22-Jun-2025  JB      use atomics to avoid races
   14-Mar-2016  JH      created
*/
#ifndef GPIOPATTERN_H_
#define GPIOPATTERN_H_

#include <stdint.h>
#include "blinkenlight_panels.h"

#if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMIC_)
#include <stdatomic.h>
#else
#define _Atomic volatile
#endif

// in one cycle more than GPIOPATTERN_LED_BRIGHTNESS_LEVELS events from server
// must be sampled, else loss of resolution
extern long gpiopattern_update_period_us;
#define GPIOPATTERN_UPDATE_PERIOD_US 50000  // 1/20 sec for screen update

// LED brightness levels (not changeable without code rework)
// For N brightness levels there are N-1 display phases
#define GPIOPATTERN_LED_BRIGHTNESS_LEVELS	32
#define GPIOPATTERN_LED_BRIGHTNESS_PHASES	(GPIOPATTERN_LED_BRIGHTNESS_LEVELS-1)

extern blinkenlight_panel_t *gpiopattern_blinkenlight_panel;

// bitfields:
//   3 rows of up to 12 switches
//   8 ledrows of double-buffered patterns of up to 12 LEDs
extern _Atomic uint32_t gpio_switchstatus[3]; 
extern _Atomic uint32_t gpiopattern_ledstatus_phases[2][GPIOPATTERN_LED_BRIGHTNESS_PHASES][8];

// phase indicies for double-buffering:
//  read index: used by GPIO mux
//  write index: data from Blinkenlight API (always opposite of the read index)
extern _Atomic int gpiopattern_ledstatus_phases_readidx;
#define gpiopattern_ledstatus_phases_writeidx (!gpiopattern_ledstatus_phases_readidx)

// worker thread that updates the LED status patterns
extern void *gpiopattern_update_leds(void *terminate);
#endif
