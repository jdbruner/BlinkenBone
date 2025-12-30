/* gpio.c: the real-time process that handles multiplexing
   This version uses libgpiod-dev v1

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

#define _countof(x) (sizeof x / sizeof x[0])

#define GPIO_NUM    0

extern int knobValue[2];	// value for knobs. 0=ADDR, 1=DATA. see main.c.
void check_rotary_encoders(int switchscan);

static unsigned ledrows[] = { 20, 21, 22, 23, 24, 25 };               	// LED rows
static unsigned rows[] = { 16, 17, 18 };                               	// switch rows
static unsigned cols[] = { 26, 27, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 }; 	// columns

static const useconds_t intervl = 50; // light each row of leds 50 us (almost flickerfree at 32 phases)
static const int pullup_flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP;
static const int tristate_flags = 0;

void *
blink(void *terminate)
{
	char *argv0 = "pidp11";
	struct gpiod_chip *chip = NULL;
	struct gpiod_line_bulk bulk_ledrows = GPIOD_LINE_BULK_INITIALIZER;
	struct gpiod_line_bulk bulk_rows = GPIOD_LINE_BULK_INITIALIZER;
	struct gpiod_line_bulk bulk_cols = GPIOD_LINE_BULK_INITIALIZER;
	int ledrow_vals[_countof(ledrows)];
	int row_vals[_countof(rows)];
	int col_vals[_countof(cols)];
	struct sched_param sp = {.sched_priority = 98}; // maybe 99, 32, 31?
	char *cp;
	int i, j, switchscan;
	void *exitstatus = (void *)-1;

	// open the chip
	if ((chip = gpiod_chip_open_by_number(GPIO_NUM)) == NULL) {
		perror("gpiod_chip_open_by_number");
		goto out;
	}

	// configure the LED rows as inputs with no pull (inert)
	if (gpiod_chip_get_lines(chip, ledrows, _countof(ledrows), &bulk_ledrows) < 0) {
		perror("gpiod_chip_get_lines(ledrows)");
		goto out;
	}
	if (gpiod_line_request_bulk_input_flags(&bulk_ledrows, argv0, tristate_flags)) {
		perror("gpiod_line_request_bulk_input_flags(bulk_ledrows)");
		goto out;
	}

	// configure the switch rows as inputs with no pull (inert)
	if (gpiod_chip_get_lines(chip, rows, _countof(rows), &bulk_rows) < 0) {
		perror("gpiod_chip_get_lines(ledrows)");
		goto out;
	}
	if (gpiod_line_request_bulk_input_flags(&bulk_rows, argv0, tristate_flags)) {
		perror("gpiod_line_request_bulk_output(bulk_rows)");
		goto out;
	}

	// configure the columns as inputs with pull up
	if (gpiod_chip_get_lines(chip, cols, _countof(cols), &bulk_cols) < 0) {
		perror("gpiod_chip_get_lines(bulk_cols)");
		goto out;
	}
	if (gpiod_line_request_bulk_input_flags(&bulk_cols, argv0, pullup_flags) < 0) {
		perror("gpiod_line_request_bulk_input_flags(bulk_cols)");
		goto out;
	}

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
			if (gpiod_line_set_direction_input_bulk(&bulk_rows) < 0)
				goto out;

			for (int i = 0; i < _countof(ledrow_vals); i++)
				ledrow_vals[i] = 0;

			// light up each row of LEDs
			// drive one LED row low for each set of columns
			for (i = 0; i < _countof(ledrows); i++) {
				// light up the next row with the matching column values
				for (j = 0; j < _countof(cols); j++) {
					col_vals[j] = !(gpio_ledstatus[i] & (1 << j));
				}
				if (gpiod_line_set_direction_output_bulk(&bulk_cols, col_vals) < 0) {
					perror("gpiod_line_set_direction_output_bulk(bulk_cols)");
					goto out;
				}
				ledrow_vals[i] = 1;
				if (gpiod_line_set_direction_output_bulk(&bulk_ledrows, ledrow_vals) < 0) {
					perror("gpiod_line_set_direction_output_bulk(bulk_ledrows)");
					goto out;
				}

				usleep(intervl);

				// turn off the row
				ledrow_vals[i] = 0;
				if (gpiod_line_set_direction_output_bulk(&bulk_ledrows, ledrow_vals) < 0) {
					perror("gpiod_line_set_direction_output_bulk(bulk_ledrows)");
					goto out;
				}
				// usleep(10); /* probably not needed due to syscall overhead with libgpiod */
			}

			// prepare to read switches
			// configure LED rows and columns as inputs
			if (gpiod_line_set_direction_input_bulk(&bulk_ledrows) < 0) {
				perror("gpiod_line_set_direction_input_bulk(bulk_ledrows)");
				goto out;
			}
			if (gpiod_line_set_direction_input_bulk(&bulk_cols) < 0) {
				perror("gpiod_line_set_direction_input_bulk(bulk_cols)");
				goto out;
			}
			
			for (i = 0; i < _countof(row_vals); i++)
				row_vals[i] = 1;
			for (i = 0; i < _countof(rows); i++) {
				row_vals[i] = 0;
				if (gpiod_line_set_direction_output_bulk(&bulk_rows, row_vals) < 0) {
					perror("gpiod_line_set_direction_output_bulk(bulk_rows)");
					goto out;
				}
				usleep(1);
				if (gpiod_line_get_value_bulk(&bulk_cols, col_vals) < 0) {
					perror("gpiod_line_get_value_bulk(bulk_cols)");
					goto out;
				}
				switchscan = 0;
				for (j = 0; j < _countof(cols); j++)
					switchscan |= col_vals[j] << j;
				row_vals[i] = 1;
				if (i == 2)
					check_rotary_encoders(switchscan); // translate raw encoder data to switch position

				gpio_switchstatus[i] = switchscan;
			}
		}
	}
	gpiod_line_set_direction_input_bulk(&bulk_rows);
	return 0;

out:
	return (void *)-1;
}

void check_rotary_encoders(int switchscan)
{
	// 2 rotary encoders. Each has two switch pins. Normally, both are 0 - no rotation.
	// encoder 1: row1, bits 8,9. Encoder 2: row1, bits 10,11
	// Gray encoding: rotate up sequence   = 11 -> 01 -> 00 -> 10 -> 11
	// Gray encoding: rotate down sequence = 11 -> 10 -> 00 -> 01 -> 11

	static int lastCode[2] = {3,3};
	int code[2];
	int i;

	code[0] = (switchscan & 0x300) >> 8;
	code[1] = (switchscan & 0xC00) >> 10;
	switchscan = switchscan & 0xff;	// set the 4 bits to zero

//printf("code 0 = %d, code 1 = %d\n", code[0], code[1]);

	// detect rotation
	for (i=0;i<2;i++)
	{
		if ((code[i]==1) && (lastCode[i]==3))
			lastCode[i]=code[i];
		else if ((code[i]==2) && (lastCode[i]==3))
			lastCode[i]=code[i];
	}

	// detect end of rotation
	for (i=0;i<2;i++)
	{
		if ((code[i]==3) && (lastCode[i]==1))
		{
			lastCode[i]=code[i];
			switchscan = switchscan + (1<<((i*2)+8));
//			printf("%d end of UP %d %d\n",i, switchscan, (1<<((i*2)+8)));
			knobValue[i]++;	//bugfix 20181225

		}
		else if ((code[i]==3) && (lastCode[i]==2))
		{
			lastCode[i]=code[i];
			switchscan = switchscan + (2<<((i*2)+8));
//			printf("%d end of DOWN %d %d\n",i,switchscan, (2<<((i*2)+8)));
			knobValue[i]--;	// bugfix 20181225
		}
	}


//	if (knobValue[0]>7)
//		knobValue[0] = 0;
//	if (knobValue[1]>3)
//		knobValue[1] = 0;
//	if (knobValue[0]<0)
//		knobValue[0] = 7;
//	if (knobValue[1]<0)
//		knobValue[1] = 3;

	knobValue[0] = knobValue[0] & 7;
	knobValue[1] = knobValue[1] & 3;

	// end result: bits 8,9 are 00 normally, 01 if UP, 10 if DOWN. Same for bits 10,11 (knob 2)
	// these bits are not used, actually. Status is communicated through global variable knobValue[i]

}
