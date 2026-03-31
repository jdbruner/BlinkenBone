/* gpio.c: the real-time process that handles multiplexing
   This version uses libgpiod-dev v1

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
 17-Oct-2025  JB    changes to use libgpiod instead of direct access to /dev/mem
 02-Jan-2026  JB    configurable knob rotation direction, refactor


 gpio.c from Oscar Vermeules PiDP8-sources.
 Slightest possible modification by Joerg.
 Updated to use libgpiod by John Bruner.
 See www.obsolescenceguaranteed.blogspot.com

 The only communication with the Blinkenlight interface:
 external variable gpiopattern_ledstatus_phases is read to determine which leds to light.
 external variable gpio_switchstatus is updated with current switch settings.
 */

#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <gpiod.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "globals.h"
#include "gpiopattern.h"

// invoke a function that returns a pointer
// if it returns NULL, print the error and return (goto out)
#define INVOKE_PTR(result, func) if ((result = func) == (void *)0) {\
    fprintf(stderr, "[%d] %s: %s\n", __LINE__, #func, strerror(errno)); \
    goto out; \
}

// invoke a function that returns 0 for success, -1 for failure
// if it fails, print the error and return (goto out)
#define INVOKE(func) if (func < 0) {\
    fprintf(stderr, "[%d] %s: %s\n", __LINE__, #func, strerror(errno)); \
    goto out; \
}

// release/free item at pointer if non-NULL using specified function
#define RELEASE(ptr, func) if (ptr != (void *)0) func(ptr)

// gpiod_chip_get_lines in libgpiod v1 doesn't modify the input line numbers,
// so it should declare this as const - but it doesn't.
// This is harmless, so just ignore the warning
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

#define GPIO_NUM    0

static const useconds_t intervl = 50; // light each row of leds 50 us (almost flickerfree at 32 phases)
static const int pullup_flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
static const int tristate_flags = 0;

void *
blink(void *terminate)
{
    struct gpiod_chip *chip = NULL;
    struct gpiod_line_bulk bulk_ledrows = GPIOD_LINE_BULK_INITIALIZER;
    struct gpiod_line_bulk bulk_rows = GPIOD_LINE_BULK_INITIALIZER;
    struct gpiod_line_bulk bulk_cols = GPIOD_LINE_BULK_INITIALIZER;
    int *ledrow_vals = NULL;
    int *row_vals = NULL;
    int *col_vals = NULL;
    struct sched_param sp = {.sched_priority = 98}; // maybe 99, 32, 31?
    char *cp;
    int i, j, switchscan;
    void *exitstatus = (void *)-1;

    // open the chip
    INVOKE_PTR(chip, gpiod_chip_open_by_number(GPIO_NUM));

    // allocate the value arrays
    INVOKE_PTR(ledrow_vals, malloc(num_ledrows * sizeof(int)));
    INVOKE_PTR(row_vals, malloc(num_rows * sizeof(int)));
    INVOKE_PTR(col_vals, malloc(num_cols * sizeof(int)));

    // configure the LED rows as inputs with no pull (inert)
    INVOKE(gpiod_chip_get_lines(chip, ledrows, num_ledrows, &bulk_ledrows));
    INVOKE(gpiod_line_request_bulk_input_flags(&bulk_ledrows, program_name, tristate_flags));

    // configure the switch rows as inputs with no pull (inert)
    INVOKE(gpiod_chip_get_lines(chip, rows, num_rows, &bulk_rows));
    INVOKE(gpiod_line_request_bulk_input_flags(&bulk_rows, program_name, tristate_flags));

    // configure the columns as inputs with pull up
    INVOKE(gpiod_chip_get_lines(chip, cols, num_cols, &bulk_cols));
    INVOKE(gpiod_line_request_bulk_input_flags(&bulk_cols, program_name, pullup_flags));

    // set thread to real time priority -----------------
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp))
        fprintf(stderr, "warning: failed to set RT priority\n");

    while (!*(_Atomic int *)terminate) {
        unsigned phase;

        // display all phases circular
        for (phase = 0; phase < GPIOPATTERN_LED_BRIGHTNESS_PHASES; phase++) {
            // each phase must be exact same duration, so include switch scanning here
            _Atomic uint32_t *gpio_ledstatus =
                gpiopattern_ledstatus_phases[gpiopattern_ledstatus_phases_readidx][phase];

            // configure switch rows as inputs
            INVOKE(gpiod_line_set_direction_input_bulk(&bulk_rows));

            for (int i = 0; i < num_ledrows; i++)
                ledrow_vals[i] = 0;

            // light up each row of LEDs
            // drive one LED row low for each set of columns
            for (i = 0; i < num_ledrows; i++) {
                // light up the next row with the matching column values
                for (j = 0; j < num_cols; j++)
                    col_vals[j] = !(gpio_ledstatus[i] & (1 << j));
                INVOKE(gpiod_line_set_direction_output_bulk(&bulk_cols, col_vals));
                ledrow_vals[i] = 1;
                INVOKE(gpiod_line_set_direction_output_bulk(&bulk_ledrows, ledrow_vals));

                usleep(intervl);

                // turn off the row
                ledrow_vals[i] = 0;
                INVOKE(gpiod_line_set_direction_output_bulk(&bulk_ledrows, ledrow_vals));
                // usleep(10); /* probably not needed due to syscall overhead with libgpiod */
            }

            // prepare to read switches
            // configure LED rows and columns as inputs
            INVOKE(gpiod_line_set_direction_input_bulk(&bulk_ledrows));
            INVOKE(gpiod_line_set_direction_input_bulk(&bulk_cols));
            
            for (i = 0; i < num_rows; i++)
                row_vals[i] = 1;
            for (i = 0; i < num_rows; i++) {
                row_vals[i] = 0;
                INVOKE(gpiod_line_set_direction_output_bulk(&bulk_rows, row_vals));
                usleep(1);
                INVOKE(gpiod_line_get_value_bulk(&bulk_cols, col_vals));
                switchscan = 0;
                for (j = 0; j < num_cols; j++)
                    switchscan |= col_vals[j] << j;
                row_vals[i] = 1;
                switch_fixup(i, switchscan);

                gpio_switchstatus[i] = switchscan;
            }
        }
    }
    gpiod_line_set_direction_input_bulk(&bulk_rows);
    exitstatus = NULL;

out:
    RELEASE(chip, gpiod_chip_close);
    RELEASE(ledrow_vals, free);
    RELEASE(row_vals, free);
    RELEASE(col_vals, free);
    return exitstatus;
}
