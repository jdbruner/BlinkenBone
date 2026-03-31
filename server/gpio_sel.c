/* gpio_sel.c: the real-time process that handles multiplexing
   This version uses direct selection of rows (dedicated GPIOs)
   This version uses pinctrl/gpiolib

 Copyright (c) 2015-2026, Oscar Vermeulen, Joerg Hoppe, John D. Bruner
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

 14-Aug-2019  OV    fix for Raspberry Pi 4's different pullup configuration
 01-Jul-2017  MH    remove INP_GPIO before OUT_GPIO and change knobValue
 01-Apr-2016  OV    almost perfect before VCF SE
 15-Mar-2016  JH    display patterns for brightness levels
 16-Nov-2015  JH    acquired from Oscar
 01-Sep-2023  JB    rewritten for libgpiod
 22-Jun-2025  JB    use atomics to avoid races
 15-Oct-2025  JB    rewritten for libgpiod v2 (breaking change)
 17-Oct-2025  JB    changes to use libgpiod instead of direct access to /dev/mem
 02-Jan-2026  JB    configurable knob rotation direction, refactor
 16-Feb-2026  JB    create version using pinctrl/gpiolib instead of libgpiod

 gpio.c from Oscar Vermeules PiDP8-sources.
 Slightest possible modification by Joerg.
 Updated to use pinctrl/gpiolib by John Bruner.
 Inspiration from Richard Cornwell's ka10_pipanel.c
 See www.obsolescenceguaranteed.blogspot.com

 The only communication with the Blinkenlight interface:
 external variable gpiopattern_ledstatus_phases is read to determine which leds to light.
 external variable gpio_switchstatus is updated with current switch settings.

 LED and switch rows and columns are defined by the main program, so this
 code can be used for different emulated panels (PiDP11, PiDP8, etc.)
 */

#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <gpiolib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "gpiopattern.h"

static const useconds_t intervl = 50; // light each row of leds 50 us (almost flickerfree at 32 phases)

void *
blink(void *terminate)
{
    struct sched_param rtschedparam = { .sched_priority = 98 };
    int i, j, switchscan;
    void *exitstatus = (void *)-1;

    // initialize and map gpiolib
    if (gpiolib_init() < 0) {
        fprintf(stderr, "gpiolib_init failed\n");
        goto out;
    }
    if (gpiolib_mmap() != 0) {
        fprintf(stderr, "gpiolib_mmap: %s\n", strerror(errno));
        goto out;
    }

    // initialize LED rows (output, low)
    for (i = 0; i < num_ledrows; i++) {
        gpio_set_fsel(ledrows[i], GPIO_FSEL_OUTPUT);
        gpio_set_drive(ledrows[i], DRIVE_LOW);
    }

    // initialize switch rows (output, high)
    for (i = 0; i < num_rows; i++) {
        gpio_set_fsel(rows[i], GPIO_FSEL_OUTPUT);
        gpio_set_drive(rows[i], DRIVE_HIGH);
    }

    // initialize columns as inputs with pull up
    for (i = 0; i < num_cols; i++) {
        gpio_set_fsel(cols[i], GPIO_FSEL_INPUT);
        gpio_set_pull(cols[i], PULL_UP);
    } 

    // set thread to real time priority
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &rtschedparam))
        fprintf(stderr, "warning: failed to set RT priority (%s)\n", strerror(errno));

    // main loop
    while (!*(_Atomic int *)terminate) {
        unsigned phase;

        // display all phases circular
        for (phase = 0; phase < GPIOPATTERN_LED_BRIGHTNESS_PHASES; phase++) {
            // each phase must be exact same duration, so include switch scanning here
            _Atomic uint32_t *gpio_ledstatus =
                gpiopattern_ledstatus_phases[gpiopattern_ledstatus_phases_readidx][phase];

            // configure columns as outputs
            for (int i = 0; i < num_cols; i++)
                gpio_set_fsel(cols[i], GPIO_FSEL_OUTPUT);

            // light up each row of LEDs
            // drive one LED row low for each set of columns
            for (i = 0; i < num_ledrows; i++) {
                // set the column values (inverted)
                for (j = 0; j < num_cols; j++)
                    gpio_set_drive(cols[j], (gpio_ledstatus[i] & (1 << j)) ? DRIVE_LOW : DRIVE_HIGH);

                // light up the row
                gpio_set_drive(ledrows[i], DRIVE_HIGH);
                usleep(intervl);

                // turn off the row (allowing time to settle)
                gpio_set_drive(ledrows[i], DRIVE_LOW);
                usleep(1);
            }

            // prepare to read switches
            // configure columns as inputs
            // TODO: do we need to set the pull again?
            for (int i = 0; i < num_cols; i++)
                gpio_set_fsel(cols[i], GPIO_FSEL_INPUT);

            // enable each row and read the switches in that row
            for (i = 0; i < num_rows; i++) {
                gpio_set_drive(rows[i], DRIVE_LOW);
                usleep(1); // allow inputs to settle
                switchscan = 0;
                for (j = 0; j < num_cols; j++)
                    switchscan |= (gpio_get_level(cols[j]) == DRIVE_HIGH) << j;
                gpio_set_drive(rows[i], DRIVE_HIGH);

                switch_fixup(i, switchscan);    // panel-specific fixups (as needed)

                gpio_switchstatus[i] = switchscan;
            }
        }
    }

    exitstatus = NULL;  // success

out:
    // if we are exiting due to an error, terminate the process
    if (exitstatus != NULL)
        exit(1);

    return exitstatus;
}
