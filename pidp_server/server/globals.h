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

/*
 * This defines the panel-specific data and behaviors.
 */

/*
 * Panel config
 *
 * Panels comprise a set of switch controls and led controls.
 * Controls are mapped to one or more GPIO slices.
 * 
 * The list of controls is terminated by an entry with a NULL name.
 */
typedef struct control_slice_info {
   char *name;             // control name
   int is_input;           // input=1, output=0
   blinkenlight_control_type_t type; // control type
   unsigned slice_offset;  // offset of this slice within its bitfield
   unsigned bitlen;        // length of this slice
   unsigned row_index;     // GPIO row index
   unsigned bit_offset;    // bit offset within row (i.e., column)
   int active_low;         // signal is active low
   unsigned int radix;     // radix
   unsigned int fmax;      // maximum frequency
} control_slice_info_t;
extern const control_slice_info_t panel_controls[];
#define SWITCH_SLICE(name, slice_offset, bitlen, row_index, bit_offset) \
   { name, 1, input_switch, slice_offset, bitlen, row_index, bit_offset, 1, 8, 0 }
#define SWITCH_SLICE_ACTIVE_HIGH(name, slice_offset, bitlen, row_index, bit_offset) \
   { name, 1, input_switch, slice_offset, bitlen, row_index, bit_offset, 0, 8, 0 }
#define LED_SLICE(name, slice_offset, bitlen, row_index, bit_offset) \
   { name, 0, output_lamp, slice_offset, bitlen, row_index, bit_offset, 0, 8, 10 }

/*
 * program information
 */
/* These are defined by the panel-specific server */
extern const char PANEL_NAME[];
extern const char PANEL_DESCRIPTION[];
extern const char SERVERNAME[];
extern const char VERSION[];

/* These are defined by server_common */
extern char *program_info;       // description of this program
extern const char *program_name; // argv[0]
extern char *program_options;    // argv[1..argc]
extern int test_mode;            // test mode: diagprint and exit
extern int run_in_background;    // run in background

/*
 * Program argument handlers
 *
 * These parse the command line and environment and set server-specific
 * variables as appropriate.
 *
 * These return 1 (true) for success and 0 for failure.
 */
extern int parse_commandline(int argc, char **argv);
extern int parse_environment(void);

/*
 * Blinkenlight API RPC callback handlers
 *
 * A default set of handlers are defined (akin to a base class).
 * Panel-specific handlers are akin to a derived class. Specify
 * the function address to override the default or NULL to use
 * the default. The handler registration in server_common will
 * update those entries from NULL to the default handler address.
 * 
 * Note that panel_get_controlvalue does not correspond to a
 * Blinkenlight API event. It provides a way to override the
 * handling of a single control during a get_controlvalues
 * event.
 */
typedef struct blinkenlight_callback_handlers {
    int  (*panel_get_controlvalue)(blinkenlight_panel_t *p, blinkenlight_control_t *c);
    void (*panel_get_controlvalues)(blinkenlight_panel_t *p);
    void (*panel_set_controlvalue)(blinkenlight_panel_t *p, blinkenlight_control_t *c);
    void (*panel_set_controlvalues)(blinkenlight_panel_t *p, int force_all);
    int  (*panel_get_state)(blinkenlight_panel_t *p);
    void (*panel_set_state)(blinkenlight_panel_t *p, int new_state);
    int  (*panel_get_mode)(blinkenlight_panel_t *p);
    void (*panel_set_mode)(blinkenlight_panel_t *p, int new_mode);
    char *(*get_info)(void);
} blinkenlight_callback_handlers_t;
extern const blinkenlight_callback_handlers_t blinkenlight_callback_handlers_default;
extern blinkenlight_callback_handlers_t blinkenlight_callback_handlers;

/*
 * Panel-specific fixup of the Blinkenlight panel
 *
 * This is called after the panel is created and populated with
 * all of the controls, providing an opportunity to adjust the
 * panel or obtain some information about it (such as pointers
 * to some controls that require special handling later).
 */
extern void panel_creation_fixup(blinkenlight_panel_t *p);

/*
 * Panel-specific GPIO
 *
 * There are two types of row selection:
 *   - direct (each switch row and led row has its own GPIO)
 *       rows defines the GPIO pins for the switch rows
 *       num_rows is the number of switch rows
 *       ledrows defines the GPIO pins for the LED rows
 *       num_ledrows is the number of LED rows
 *   - encoded (the row number is encoded across three GPIO pins)
 *       row_encoder defines the GPIO pins for each bit of the row number
 * 
 * If encoded is used, num_rows and num_ledrows should be 0.
 * 
 * Regardless of the row encoding:
 *       cols defines the GPIO pins for the columns
 *       num_cols is the number of columns
 */
extern const unsigned ledrows[];     // LED rows
extern const unsigned num_ledrows;   // number of LED rows
extern const unsigned rows[];        // switch rows
extern const unsigned num_rows;      // number of switch rows

extern const unsigned row_encoder[]; // pins for each bit of row number

extern const unsigned cols[];        // columns
extern const unsigned num_cols;      // number of columns


/*
 * Panel-specific input (switch) and output (LED) fixup handlers
 *
 * The switch fixup handler is invoked for each switch row with the
 * binary value of that row. It can perform any panel-specific
 * action (e.g., rotation of the knobs on the 11/70 panel).
 * 
 * The LED fixup handler is invoked when a new value is set on an
 * LED control. It can perform a custom update of any or all LED bits
 * (gpio_ledstatus) such as to implement lamp test mode. If it handles
 * the value, it returns 1; otherwise, it returns 0 and the default
 * handler is used.
 */
extern void switch_fixup(int row, int switchscan);
extern int led_fixup(blinkenlight_panel_t *p, blinkenlight_control_t *c,
   int *panel_mode_ptr, uint32_t value, _Atomic uint32_t *gpio_ledstatus);

/*
 * other
 */
extern blinkenlight_panel_list_t *blinkenlight_panel_list;

#endif /* GLOBALS_H_ */
