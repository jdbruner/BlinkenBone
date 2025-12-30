/* gpio.c: the real-time process that handles multiplexing
   This version uses libgpiod-dev v2

 Copyright (c) 2015-2023, Oscar Vermeulen, Joerg Hoppe, John D. Bruner
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
 01-Sep-2023  JB	rewritten for libgpiod
 22-Jun-2025  JB    use atomics to avoid races
 15-Oct-2025  JB    rewritten for libgpiod v2 (breaking change)

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

#define _countof(x) (sizeof x / sizeof x[0])

static const char *GPIO_CHIP = "/dev/gpiochip0";
static const char *argv0 = "server11";

extern int knobValue[2];	// value for knobs. 0=ADDR, 1=DATA. see main.c.
void check_rotary_encoders(int switchscan);

static const unsigned ledrows[] = { 20, 21, 22, 23, 24, 25 };               	// LED rows
static const unsigned rows[] = { 16, 17, 18 };                               	// switch rows
static const unsigned cols[] = { 26, 27, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 }; 	// columns

static const useconds_t intervl = 50; // light each row of leds 50 us (almost flickerfree at 32 phases)

void *
blink(void *terminate)
{
    struct gpiod_chip *chip = NULL;
    struct gpiod_request_config *request_config = NULL;
    struct gpiod_line_settings *tristate_settings = NULL;
    struct gpiod_line_settings *input_settings = NULL;
    struct gpiod_line_settings *output_settings = NULL;
    struct gpiod_line_config *ledrow_output_config = NULL;
    struct gpiod_line_config *row_tristate_config = NULL;
    struct gpiod_line_config *row_output_config = NULL;
    struct gpiod_line_config *col_input_config = NULL;
    struct gpiod_line_config *col_output_config = NULL;
    struct gpiod_line_request *ledrow_request = NULL;
    struct gpiod_line_request *row_request = NULL;
    struct gpiod_line_request *col_request = NULL;
    enum gpiod_line_value col_vals[_countof(cols)];
    struct sched_param rtschedparam = { .sched_priority = 98 };
    int i, j, switchscan;
    void *exitstatus = (void *)-1;

    // open the chip
    INVOKE_PTR(chip, gpiod_chip_open(GPIO_CHIP));

    // request configuration (with name of this program)
    INVOKE_PTR(request_config, gpiod_request_config_new());
    gpiod_request_config_set_consumer(request_config, argv0);

    // tristate line settings (input with no pull)
    INVOKE_PTR(tristate_settings, gpiod_line_settings_new());
    INVOKE(gpiod_line_settings_set_direction(tristate_settings, GPIOD_LINE_DIRECTION_INPUT));
    INVOKE(gpiod_line_settings_set_bias(tristate_settings, GPIOD_LINE_BIAS_DISABLED));

    // input line settings (with pull-up)
    INVOKE_PTR(input_settings, gpiod_line_settings_new());
    INVOKE(gpiod_line_settings_set_direction(input_settings, GPIOD_LINE_DIRECTION_INPUT));
    INVOKE(gpiod_line_settings_set_bias(input_settings, GPIOD_LINE_BIAS_PULL_UP));

    // output line settings
    INVOKE_PTR(output_settings, gpiod_line_settings_new());
    INVOKE(gpiod_line_settings_set_direction(output_settings, GPIOD_LINE_DIRECTION_OUTPUT));
    INVOKE(gpiod_line_settings_set_bias(output_settings, GPIOD_LINE_BIAS_DISABLED));

    // create configurations for LED rows (output with all lines inactive)
    INVOKE_PTR(ledrow_output_config, gpiod_line_config_new());
    INVOKE(gpiod_line_settings_set_output_value(output_settings, GPIOD_LINE_VALUE_INACTIVE));
    INVOKE(gpiod_line_config_add_line_settings(ledrow_output_config, ledrows, _countof(ledrows), output_settings));

    // create configurations for switch rows (tristate, output with all lines active)
    INVOKE_PTR(row_tristate_config, gpiod_line_config_new());
    INVOKE(gpiod_line_config_add_line_settings(row_tristate_config, rows, _countof(rows), tristate_settings));
    INVOKE_PTR(row_output_config, gpiod_line_config_new());
    INVOKE(gpiod_line_settings_set_output_value(output_settings, GPIOD_LINE_VALUE_ACTIVE));
    INVOKE(gpiod_line_config_add_line_settings(row_output_config, rows, _countof(rows), output_settings));
    
    // create configurations for columns (output with all lines active, input with pull up)
    INVOKE_PTR(col_output_config, gpiod_line_config_new());
    INVOKE(gpiod_line_config_add_line_settings(col_output_config, cols, _countof(cols), output_settings));
    INVOKE_PTR(col_input_config, gpiod_line_config_new());
    INVOKE(gpiod_line_config_add_line_settings(col_input_config, cols, _countof(cols), input_settings));

    // request the GPIO lines for the ledrows, rows, and cols
    INVOKE_PTR(ledrow_request, gpiod_chip_request_lines(chip, request_config, ledrow_output_config));
    INVOKE_PTR(row_request, gpiod_chip_request_lines(chip, request_config, row_tristate_config));
    INVOKE_PTR(col_request, gpiod_chip_request_lines(chip, request_config, col_output_config));

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

            // configure switch rows as tristate, columns as outputs
            INVOKE(gpiod_line_request_reconfigure_lines(row_request, row_tristate_config));
            INVOKE(gpiod_line_request_reconfigure_lines(col_request, col_output_config));

            // light up each row of LEDs
            // drive one LED row low for each set of columns
            for (i = 0; i < _countof(ledrows); i++) {
                // light up the next row with the matching column values (inverted)
                for (j = 0; j < _countof(cols); j++)
                    col_vals[j] = (gpio_ledstatus[i] & (1 << j)) ? GPIOD_LINE_VALUE_INACTIVE : GPIOD_LINE_VALUE_ACTIVE;

                INVOKE(gpiod_line_request_set_values(col_request, col_vals));
                INVOKE(gpiod_line_request_set_value(ledrow_request, ledrows[i], GPIOD_LINE_VALUE_ACTIVE));
                usleep(intervl);

                // turn off the row (allowing time to settle)
                INVOKE(gpiod_line_request_set_value(ledrow_request, ledrows[i], GPIOD_LINE_VALUE_INACTIVE));
                usleep(1);
            }

            // prepare to read switches
            // configure switch rows as outputs, columns as inputs
            INVOKE(gpiod_line_request_reconfigure_lines(row_request, row_output_config));
            INVOKE(gpiod_line_request_reconfigure_lines(col_request, col_input_config));

            // enable each row and read the switches in that row
            for (i = 0; i < _countof(rows); i++) {
                INVOKE(gpiod_line_request_set_value(row_request, rows[i], GPIOD_LINE_VALUE_INACTIVE));
                usleep(1); // allow inputs to settle
                INVOKE(gpiod_line_request_get_values(col_request, col_vals));
                switchscan = 0;
                for (j = 0; j < _countof(cols); j++)
                    switchscan |= (col_vals[j] == GPIOD_LINE_VALUE_ACTIVE) << j;
                INVOKE(gpiod_line_request_set_value(row_request, rows[i], GPIOD_LINE_VALUE_ACTIVE));

                if (i == 2)
                    check_rotary_encoders(switchscan); // translate raw encoder data to switch position
                gpio_switchstatus[i] = switchscan;
            }
        }
    }

    // before exiting, reset the switch rows to tristate
    INVOKE(gpiod_line_request_reconfigure_lines(row_request, row_tristate_config));
    exitstatus = NULL;  // success

out:
    // clean up and exit
    RELEASE(ledrow_request, gpiod_line_request_release);
    RELEASE(row_request, gpiod_line_request_release);
    RELEASE(col_request, gpiod_line_request_release);

    RELEASE(ledrow_output_config, gpiod_line_config_free);
    RELEASE(row_tristate_config, gpiod_line_config_free);
    RELEASE(row_output_config, gpiod_line_config_free);
    RELEASE(col_input_config, gpiod_line_config_free);
    RELEASE(col_output_config, gpiod_line_config_free);
    
    RELEASE(tristate_settings, gpiod_line_settings_free);
    RELEASE(input_settings, gpiod_line_settings_free);
    RELEASE(output_settings, gpiod_line_settings_free);

    RELEASE(request_config, gpiod_request_config_free);

    RELEASE(chip, gpiod_chip_close);

    // if we are exiting due to an error, terminate the process
    if (exitstatus != NULL)
        exit(1);

    return exitstatus;
}

void
check_rotary_encoders(int switchscan)
{
    // 2 rotary encoders. Each has two switch pins. Normally, both are 0 - no rotation.
    // encoder 1: row1, bits 8,9. Encoder 2: row1, bits 10,11
    // Gray encoding: rotate up sequence   = 11 -> 01 -> 00 -> 10 -> 11
    // Gray encoding: rotate down sequence = 11 -> 10 -> 00 -> 01 -> 11

    static int lastCode[2] = { 3, 3 };
    int code[2];
    int i;

    code[0] = (switchscan & 0x300) >> 8;
    code[1] = (switchscan & 0xC00) >> 10;
    switchscan = switchscan & 0xff;	// set the 4 bits to zero

    // detect rotation
    for (i = 0; i < 2; i++) {
        if ((code[i] == 1) && (lastCode[i] == 3))
            lastCode[i] = code[i];
        else if ((code[i] == 2) && (lastCode[i] == 3))
            lastCode[i] = code[i];
    }

    // detect end of rotation
    for (i = 0; i < 2; i++) {
        if ((code[i] == 3) && (lastCode[i] == 1)) {
            lastCode[i] = code[i];
            switchscan = switchscan + (1 << ((i * 2) + 8));
            knobValue[i]++;

        } else if ((code[i] == 3) && (lastCode[i] == 2)) {
            lastCode[i] = code[i];
            switchscan = switchscan + (2 << ((i * 2) + 8));
            knobValue[i]--;
        }
    }

    knobValue[0] = knobValue[0] & 7;
    knobValue[1] = knobValue[1] & 3;

    // end result: bits 8,9 are 00 normally, 01 if UP, 10 if DOWN. Same for bits 10,11 (knob 2)
    // these bits are not used, actually. Status is communicated through global variable knobValue[i]
}
