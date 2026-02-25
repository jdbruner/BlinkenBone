/* server10.c: Blinkenlight API server to run on "PiDP10" replica

 Copyright (c) 2015-2016, Joerg Hoppe
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

 13-Feb-2026  JB    new main program to support PiDP10
 06-Jan-2026  JB    changes to use libgpiod instead of direct access to /dev/mem
 22-Mar-2016  JH    allow a control value to be distributed over several hw registers
 15-Mar-2016  JH    V 1.3 Low-pass for SimH output, display patterns for brightness levels
 09-Mar-2016  JH    V 1.2 inverted "Deposit" switch
 06-Mar-2016  JH    renamed from "blinkenlightd" to "pidp8_blinkenlightd"
 22-Feb-2016  JH    V 1.1 added panel modes LAMPTEST, POWERLESS
 13-Nov-2015  JH    V 1.0 created


 Blinkenlight API server, which controls lamps and switches
 on the Raspberry based "PiPDP10 replica from Oscar Vermeulen.

 Like the generic blinkenlightd,
 - PiDP10 controls are fix wired in (defined in panel_controls[]), no config file
 - Hardware interface to Raspberry is "gpio_enc.c"


 Timing & CPU load:
 There are 2 threads:
 a) the LED MUX loop, in gpio_enc.c, long intervl
 b) the averaging loop in gpiopattern.c
 (and SimH is the 3rd process running)

 To fine trim cpu load, use web based "rCPU"
 https://github.com/davidsblog/rCPU

 */

#define _GNU_SOURCE     // for asprintf
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h> 
#include <unistd.h>
#include <assert.h>

#include "blinkenlight_panels.h"
#include "blinkenlight_api_server_procs.h"

///// for Blinkenlight API server
#include "rpc_blinkenlight_api.h"
#include <rpc/pmap_clnt.h> // different name under "oncrpc for windows"?
#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

#include "print.h"

#include "globals.h"
#include "gpiopattern.h"

/*
 * Panel definition strings
 */
const char PANEL_NAME[] = "PDP10-KA10";
const char PANEL_DESCRIPTION[] = "PIDP10 panel";
const char SERVERNAME[] = "pidp10panel";
const char VERSION[] = "1.0.0";

/*
 * GPIO row and column definitions
 */
#define _countof(x) (sizeof x / sizeof x[0])

const unsigned row_encoder[] = { 4, 17, 27 };                           // pins for row number
const unsigned num_encoder = _countof(row_encoder);                     // number of encoded pins
const unsigned row_io = 22;                                             // pin for row type 

const unsigned num_ledrows = 7;                                         // number of LED rows
const unsigned num_rows = 5;                                            // number of switch rows

const unsigned cols[] = { 21,20,16,12,7,8,25,24,23,18,10,9,11,5,6,13,19,26 }; // column pins
const unsigned num_cols = _countof(cols);                               // number of columns

const control_slice_info_t panel_controls[] = {
    SWITCH_SLICE("SR", 0, 18, 0, 0),        // 0, 0777777
    SWITCH_SLICE("SR", 18, 18, 1, 0),       // 1, 0777777

    SWITCH_SLICE("MA", 0, 18, 2, 0),        // 2, 0777777

    SWITCH_SLICE("EXAM_NEXT", 0, 1, 3, 0),  // 3, 0000001
    SWITCH_SLICE("EXAM_THIS", 0, 1, 3, 1),  // 3, 0000002
    SWITCH_SLICE("XCT", 0, 1, 3, 2),        // 3, 0000004
    SWITCH_SLICE("RESET", 0, 1, 3, 3),      // 3, 0000010
    SWITCH_SLICE("STOP", 0, 1, 3, 4),       // 3, 0000020
    SWITCH_SLICE("CONT", 0, 1, 3, 5),       // 3, 0000040
    SWITCH_SLICE("START", 0, 1, 3, 6),      // 3, 0000100
    SWITCH_SLICE("READ_IN", 0, 1, 3, 7),    // 3, 0000200
    SWITCH_SLICE("DEP_NEXT", 0, 1, 3, 8),   // 3, 0000400
    SWITCH_SLICE("DEP_THIS", 0, 1, 3, 9),   // 3, 0001000

    SWITCH_SLICE("ADR_BRK", 0, 1, 4, 0),    // 4, 0000001
    SWITCH_SLICE("ADR_STOP", 0, 1, 4, 1),   // 4, 0000002
    SWITCH_SLICE("WRITE", 0, 1, 4, 2),      // 4, 0000004
    SWITCH_SLICE("DATA_FETCH", 0, 1, 4, 3), // 4, 0000010
    SWITCH_SLICE("INST_FETCH", 0, 1, 4, 4), // 4, 0000020
    SWITCH_SLICE("REP", 0, 1, 4, 5),        // 4, 0000040
    SWITCH_SLICE("NXM_STOP", 0, 1, 4, 6),   // 4, 0000100
    SWITCH_SLICE("PAR_STOP", 0, 1, 4, 7),   // 4, 0000200
    SWITCH_SLICE("SING_CYCL", 0, 1, 4, 8),  // 4, 0000400
    SWITCH_SLICE("SING_INST", 0, 1, 4, 9),  // 4, 0001000

    LED_SLICE("MB", 0, 18, 0, 0),           // 0, 0777777
    LED_SLICE("MB", 18, 18, 1, 0),          // 1, 0777777

    LED_SLICE("INSTR", 0, 18, 2, 0),        // 2, 0777777
    LED_SLICE("INSTR", 18, 18, 3, 0),       // 3, 0777777

#if notdef
    // component pieces of the instruction row
    LED_SLICE("AB", 0, 18, 2, 0),           // 2, 0777777

    LED_SLICE("IX", 0, 4, 3, 0),            // 3, 0000017
    LED_SLICE("IND", 0, 1, 3, 4),           // 3, 0000020
    LED_SLICE("AC", 0, 4, 3, 5),            // 3, 0000740
    LED_SLICE("IR", 0, 9, 3, 9),            // 3, 0777000
#endif

    LED_SLICE("PC", 0, 18, 4, 0),           // 4, 0777777

    LED_SLICE("PI_ENB", 0, 7, 5, 0),        // 5, 0000177
    LED_SLICE("PI_IOB", 0, 7, 5, 7),        // 5, 0037600
    LED_SLICE("PROG_STOP", 0, 1, 5, 14),    // 5, 0040000
    LED_SLICE("USER_MODE", 0, 1, 5, 15),    // 5, 0100000
    LED_SLICE("MEM_STOP", 0, 1, 5, 16),     // 5, 0200000
    LED_SLICE("POWER", 0, 1, 5, 17),        // 5, 0400000

    LED_SLICE("PI_REQ", 0, 7, 6, 0),        // 6, 0000177
    LED_SLICE("PI_PRO", 0, 7, 6, 7),        // 6, 0037600
    LED_SLICE("RUN", 0, 1, 6, 14),          // 6, 0040000
    LED_SLICE("PI_ON", 0, 1, 6, 15),        // 6, 0100000
    LED_SLICE("PI", 0, 1,6, 16),            // 6, 0200000
    LED_SLICE("MI", 0, 1, 6, 17),           // 6, 0400000

    { NULL }
};

/*
 *  PiDP10 controls that require special handling
 *  (none)
 */
// static blinkenlight_control_t

/*
 *  RPC server callbacks:
 *  here conversion between PiDP10 gpio and Blinkenlight API is done!
 */

// Special handling of control when its value is requested by the client
static int
pidp10_blinkenlight_api_panel_get_controlvalue(blinkenlight_panel_t *p,
    blinkenlight_control_t *c)
{
    return 0;
}

/*
 * Blinkenlight callback handlers
 * Non-NULL entries override the defaults defined in server_common
 * NULL entries are replaced with pointers to the respective defaults
 */
blinkenlight_callback_handlers_t blinkenlight_callback_handlers = {
//  .panel_get_controlvalue = pidp10_blinkenlight_api_panel_get_controlvalue
    // all others are NULL (default behavior)
};

/*
 * Post-panel creation fixup
 *
 * Get pointers to the controls we need to handle specially
 */
void
panel_creation_fixup(blinkenlight_panel_t *p)
{
    struct special_controls {
        char *name;
        blinkenlight_control_t **control_ptr;
        int is_input;
    };
    static const struct special_controls special_controls[] = {
        // { "EXAMPLE", &switch_EXAMPLE, 1 },
        { NULL }
    };
    const struct special_controls *spc;

    for (spc = special_controls; spc->name != NULL; spc++) {
        *spc->control_ptr =
            blinkenlight_panels_get_control_by_name(blinkenlight_panel_list,
                p, spc->name, spc->is_input);
        assert(spc->control_ptr != NULL);
    }
}

/*
 * print help
 */
static
void help(void)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "%s %s - Blinkenlight RPC server for PiDP10 \n",
    program_name, VERSION);
    fprintf(stderr, "  (compiled " __DATE__ " " __TIME__ ")\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Call:\n");
    fprintf(stderr, "%s [-b] [-v] [-t]\n", program_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "-b               : background operation: print to syslog (view with dmesg)\n");
    fprintf(stderr, "                   default output is stderr\n");
    fprintf(stderr, "-v               : verbose: tell what I'm doing\n");
    fprintf(stderr, "-t               : test mode\n");
    fprintf(stderr, "\n");
}

/*
 * read environment variable parameters into global vars
 * erroneous values are ignored
 * always returns 1 (success)
 */
int
parse_environment(void)
{
    // no-op
    return 1;
}

/*
 * read commandline paramaters into global vars
 * result: 1 = OK, 0 = error
 */
int
parse_commandline(int argc, char **argv)
{
    int i, c;

    opterr = 0;
    while ((c = getopt(argc, argv, "bvt")) != -1) {
        switch (c) {
        case 'v':
            print_level = LOG_DEBUG;
            break;
        case 'b':
            run_in_background = 1;
            break;
        case 't':
            test_mode = 1;
            break;
        case '?': // getopt detected an error. "opterr=0", so own error message here
            if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 0;
            break;
        default:
            assert(0); // getopt() got crazy?
            return 0;
        }
    }

    return 1;
}

/*
 * No special handling required for LEDs on the PiDP10
 */
int
led_fixup(blinkenlight_panel_t *p, blinkenlight_control_t *c, int *panel_mode_ptr,
        uint64_t value, _Atomic uint32_t *gpio_ledstatus)
{
    return 0;
}

/*
 * No special handling required for switches on the PiDP10
 */
void
switch_fixup(int row, int switchscan)
{
}
