/* globals.h: Global definitions for PiDP Blinkenlight API server

   Copyright (c) 2015-2016, Joerg Hoppe
   Copyright (c) 2026, John D. Bruner
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

   06-Jan-2026  JB      refactored
   13-Nov-2015  JH      created
*/


#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "blinkenlight_panels.h"

// global panel config
extern blinkenlight_panel_list_t *blinkenlight_panel_list;

// program information
extern char *program_info;       // description of this program
extern const char *program_name; // argv[0]
extern char *program_options;    // argv[1..argc]

// Panel-specific GPIO row and column definitions
extern const unsigned ledrows[];     // LED rows
extern const unsigned num_ledrows;   // number of LED rows
extern const unsigned rows[];        // switch rows
extern const unsigned num_rows;      // number of switch rows
extern const unsigned cols[];        // columns
extern const unsigned num_cols;      // number of columns

// Panel-specific input (switch) and output (LED) fixup handlers
extern void switch_fixup(int row, int switchscan);
extern int led_fixup(blinkenlight_panel_t *p, blinkenlight_control_t *c,
   int *panel_mode_ptr, uint32_t value, _Atomic uint32_t *gpio_ledstatus);

#endif /* GLOBALS_H_ */
