 /* server11.c: Blinkenlight API server to run on "PiDP11" replica

 Copyright (c) 2015-2016, Joerg Hoppe
 Copyright (c) 2026, John D. Bruner
 j_hoppe@t-online.de, www.retrocmp.com
 jdbruner@live.com

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

 06-Jan-2026  JB    refactored
 02-Jan-2026  JB    configurable knob rotation direction
 17-Oct-2025  JB    changes to use libgpiod instead of direct access to /dev/mem
 27-Dec-2018  SC/MH OV: added MH fix occasional blinking LEDs (LAMPTEST in the gpiopattern thread)
 03-Feb-2018  JH    fixed SUPER-USER-KERNEL encoding
 07-Sep-2017  MH    Added further command line option (-L)
 05-Jul-2017  MH    Added more options to the command line (-h -a -d -s)
 04-Jul-2017  MH    V 1.4.1 fix: ADDR and DATA SELECT rotary positions + KSU modes
 08-May-2016  JH    V 1.4 fix: MMR0 converted code -> led pattern BEFORE history/low pass
 01-Apr-2016  OV    almost perfect before VCF SE
 22-Mar-2016  JH    allow a control value to be distributed over several hw registers
 20-Mar-2016  OV    test hack to convert pidp8 into pidp11 server.

 15-Mar-2016  JH    V 1.3 Low-pass for SimH output, display patterns for brightness levels
 09-Mar-2016  JH    V 1.2 inverted "Deposit" switch
 06-Mar-2016  JH    renamed from "blinkenlightd" to "pidp8_blinkenlightd"
 22-Feb-2016  JH    V 1.1 added panel modes LAMPTEST, POWERLESS
 13-Nov-2015  JH    V 1.0 created


 Blinkenlight API server, which controls lamps and switches
 on the Raspberry based "PiPDP11"

 Like the generic blinkenlightd,
 - PiDP11 controls are fix wired in (defined in panel_controls[]), no config file
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
#include <rpc/pmap_clnt.h> 
#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

#include "print.h"

#include "globals.h"
#include "gpiopattern.h"

/*
 * Panel definition strings
 */
const char PANEL_NAME[] = "11/70";
const char PANEL_DESCRIPTION[] = "PIDP11 panel";
const char SERVERNAME[] = "pidp11panel";
const char VERSION[] = "2.1.0";

/*
 * Panel state
 */
int panel_lock = 0;     // Panel lock state - default to unlocked

/* default start value for knobs (match Panelsim)
 *    knobValue[0]      knobValue[1]
 *    0 = Prog Phy      0 = Bus Reg
 *    1 = Cons Phy      1 = Data Paths
 *    2 = Kernel D      2 = uADR CPU FPU
 *    3 = Super D       3 = Display Register
 *    4 = User D
 *    5 = User I
 *    6 = Super I
 *    7 = Kernel I
 */
int knobValue[2] = { 1, 1 }; // Cons Phy , Data Paths

// add this to knobValue[*] to increment position
// (invert this to -1 if knobs rotate backwards)
int knobIncrement = 1;

// Map rotary positioner to ADDR SELECT pseudo switches
int knobAddrMap[8] = { 7, 4, 6, 3, 1, 0, 2, 5 };

// Map rotary positioner to DATA SELECT pseudo switches
int knobDataMap[4] = { 3, 2, 0, 1 };

/*
 * GPIO row and column definitions
 */
#define _countof(x) (sizeof x / sizeof x[0])
const unsigned ledrows[] = { 20, 21, 22, 23, 24, 25 };                   // LED rows
const unsigned num_ledrows = _countof(ledrows);                          // number of LED rows
const unsigned rows[] = { 16, 17, 18 };                                  // switch rows
const unsigned num_rows = _countof(rows);                                // number of switch rows
const unsigned cols[] = { 26, 27, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };    // columns
const unsigned num_cols = _countof(cols);                                // number of columns

/*
 * 11/70 panel definition - all switches and LEDs
 */
const control_slice_info_t panel_controls[] = {
    SWITCH_SLICE("SR", 0, 12, 0, 0),        // 0, 0xfff. bits 0..11
    SWITCH_SLICE("SR", 12, 10, 1, 0),       // 1  0x3ff, bits 12..21
    SWITCH_SLICE("LAMPTEST", 0, 1, 2, 0),   // 2, 0x01 (or should it be 1,0,0 fur null?)
    SWITCH_SLICE("LOAD_ADRS", 0, 1, 2, 1),  // 2, 0x02
    SWITCH_SLICE("EXAM", 0, 1, 2, 2),       // 2, 0x04
    SWITCH_SLICE("DEPOSIT", 0, 1, 2, 3),    // 2, 0x08
    SWITCH_SLICE("CONT", 0, 1, 2, 4),       // 2, 0x010
    SWITCH_SLICE("HALT", 0, 1, 2, 5),       // 2, 0x020
    SWITCH_SLICE("S_BUS_CYCLE", 0, 1, 2, 6),// 2, 0x040
    SWITCH_SLICE("START", 0, 1, 2, 7),      // 2, 0x080
    LED_SLICE("ADDRESS", 0, 12, 0, 0),      // 0,0xfff, bits 0..11
    LED_SLICE("ADDRESS", 12, 10, 1, 0),     // 0,0x3ff, bits 12..21
    LED_SLICE("DATA", 0, 12, 3, 0),         // 3,0xfff, bits 0..11
    LED_SLICE("DATA", 12, 4, 4, 0),         // 4,0xf, bits 12..15
    LED_SLICE("PARITY_HIGH", 0, 1, 4, 5),   // 4, 0x20
    LED_SLICE("PARITY_LOW", 0, 1, 4, 4),    // 4, 0x10
    LED_SLICE("PAR_ERR", 0, 1, 2, 11),      // 2, 0x800
    LED_SLICE("ADRS_ERR", 0, 1, 2, 10),     // 2, 0x400
    LED_SLICE("RUN", 0, 1, 2, 9),           // 2, 0x200
    LED_SLICE("PAUSE", 0, 1, 2, 8),         // 2, 0x100
    LED_SLICE("MASTER", 0, 1, 2, 7),        // 2, 0x80
    LED_SLICE("MMR0_MODE", 0, 3, 2, 4),
    LED_SLICE("DATA_SPACE", 0, 1, 2, 3),    // 2, 0x08
    LED_SLICE("ADDRESSING_16", 0, 1, 2, 2), // 2, 0x04
    LED_SLICE("ADDRESSING_18", 0, 1, 2, 1), // 2, 0x02
    LED_SLICE("ADDRESSING_22", 0, 1, 2, 0), // 2, 0x01
    SWITCH_SLICE("ADDR_SELECT", 0, 3, 0, 0),
    SWITCH_SLICE("DATA_SELECT", 0, 2, 0, 3),
    LED_SLICE("ADDR_SELECT_FEEDBACK", 0, 4, 4, 6),
    LED_SLICE("ADDR_SELECT_FEEDBACK", 4, 4, 5, 5),
    LED_SLICE("DATA_SELECT_FEEDBACK", 0, 2, 4, 10), // bus_reg, data_paths
    LED_SLICE("DATA_SELECT_FEEDBACK", 2, 2, 5, 10), // disp_reg, muaddr
    SWITCH_SLICE("PANEL_LOCK", 0, 1, 0, 0), // dummy, always 0
    SWITCH_SLICE_ACTIVE_HIGH("POWER", 0, 1, 1, 10),     // 1, 0x400
    { NULL }
};

/*
 * PiDP11 controls that require special handling
 */
static blinkenlight_control_t *switch_PANEL_LOCK, *switch_LAMPTEST,
    *switch_ADDR_SELECT, *switch_DATA_SELECT, *leds_MMR0_MODE,
    *leds_ADDR_SELECT, *leds_DATA_SELECT;

/*
 * RPC server handler and callback overrides
 */

// Special handling of control when its value is requested by the client
static int
pidp11_blinkenlight_api_panel_get_controlvalue(blinkenlight_panel_t *p,
    blinkenlight_control_t *c)
{
#if 0
    if (c == switch_POWER) {
        static int pwrDebounce = 0;

        c->value = ((gpio_switchstatus[1] & 1<<10) == 0); // send "power switch" signal
        /*
         * In Oscar's original implementation, the panel server invokes a command script:
         * If the HALT switch is up. then the simulator is rebooted - basically, by writing
         * an "exit" command into a temp file that simh (and the script that invokes it)
         * looks at. If the HALT switch is down, the entire machine is shutdown.
         *
         * To avoid the implicit requirement that this is running as root (or can sudo),
         * this behavior is moved to simh. It exits the simulator with a zero or nonzero
         * status (depending upon the halt switch). The PiDP11 shell script will restart
         * simh if the exit status is zero and exit (terminating the service) otherwise.
         */
        if ((c->value)==0) {
            // do it only once, when power button is triggered
            if (pwrDebounce==0) {
                char *buffer = NULL;
                pwrDebounce=1;  // do it only once, when power button is triggered
                
                if (switch_HALT->value == 0 && asprintf(&buffer, "/opt/pidp11/bin/rebootsimh.sh")) {
                    FILE *bootfil = popen(buffer, "r");
                    printf("\r\n--> Rebooting...\r\n");
                    pclose(bootfil);
                } else if (asprintf(&buffer, "/opt/pidp11/bin/down.sh")) {
                    FILE *bootfil = popen(buffer, "r");
                    printf("--> System shutdown - allow 15 seconds before power off\r\n");
                    pclose(bootfil);
                }
                if (buffer != NULL)
                    free(buffer);
            }
        } else
            pwrDebounce=0;  // power button released
    } else
#endif
    if (c == switch_PANEL_LOCK)
        c->value = panel_lock; // send "panel lock" switch as defined by -L
    else if (c == switch_ADDR_SELECT) {
        c->value = knobAddrMap[knobValue[0]];
        leds_ADDR_SELECT->value = 1<<knobValue[0];
    } else if (c == switch_DATA_SELECT) {
        c->value = knobDataMap[knobValue[1]];
        leds_DATA_SELECT->value = 1<<knobValue[1];
    } else
        return 0;   // for everything else, handle normally
    return 1;
}

// special handling after value of a control has been set
static void
pidp11_blinkenlight_api_panel_set_controlvalue(blinkenlight_panel_t *p,
    blinkenlight_control_t *c)
{
    // convert MMR= code number to LED pattern BEFORE historybuffer/lowpass is applied to value
    if (c == leds_MMR0_MODE) {
        // input:  0=K, 1=off, 2=S, 3=U (see src/REALCONS/realcons_console_pdp11_70.c)
        // leds: kernel = reg[2].4, super= reg[2].5 user=reg[2].6
        switch (c->value) {
        case 0:
            c->value = 1 ; // KERNEL;
            break;
        case 2:
            c->value = 2 ; // SUPER;
            break;
        case 3:
            c->value = 4 ; // USER;
            break;
        default: // encode any other(s) as "all off"
            c->value = 0;
        }
    }
}

/*
 * Blinkenlight callback handlers
 * Non-NULL entries override the defaults defined in server_common
 * NULL entries are replaced with pointers to the respective defaults
 */
blinkenlight_callback_handlers_t blinkenlight_callback_handlers = {
    .panel_get_controlvalue = pidp11_blinkenlight_api_panel_get_controlvalue,
    .panel_set_controlvalue = pidp11_blinkenlight_api_panel_set_controlvalue
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
        { "PANEL_LOCK", &switch_PANEL_LOCK, 1 },
        { "LAMPTEST", &switch_LAMPTEST, 1 },
        { "ADDR_SELECT", &switch_ADDR_SELECT, 1 },
        { "DATA_SELECT", &switch_DATA_SELECT, 1 },
        { "MMR0_MODE", &leds_MMR0_MODE, 0  },
        { "ADDR_SELECT_FEEDBACK", &leds_ADDR_SELECT, 0 },
        { "DATA_SELECT_FEEDBACK", &leds_DATA_SELECT, 0 },
        { NULL }
    };
    const struct special_controls *spc;

    for (spc = special_controls; spc->name != NULL; spc++) {
        *spc->control_ptr =
            blinkenlight_panels_get_control_by_name(blinkenlight_panel_list,
                p, spc->name, spc->is_input);
        assert(*spc->control_ptr != NULL);
    }
}

/*
 * print help
 */
static void
help(void)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "%s %s - Blinkenlight RPC server for PiDP11 \n", SERVERNAME, VERSION);
    fprintf(stderr, "  (compiled " __DATE__ " " __TIME__ ")\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [-h] [-b] [-v] [-t] [-L] [-a 0..7] [-d 0..3] [-r] [-s <n>]\n", program_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "  -h          display this help and exit\n");
    fprintf(stderr, "  -b          background operation: print to syslog (view with dmesg)\n");
    fprintf(stderr, "                default output is stderr\n");
    fprintf(stderr, "  -v          verbose: tell what I'm doing\n");
    fprintf(stderr, "  -t          test mode\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -L          permanently engage PANEL LOCK\n");
    fprintf(stderr, "  -a 0..7     starting position of the ADDR SELECT knob\n");
    fprintf(stderr, "                clockwise from 0=PROG PHY .. 7=KERNEL I\n");
    fprintf(stderr, "                default is -a%d\n", knobValue[0]);
    fprintf(stderr, "  -d 0..3     starting position of the DATA SELECT knob\n");
    fprintf(stderr, "                clockwise from 0=BUS REG .. 3=DISPLAY REGISTER\n");
    fprintf(stderr, "                default is -d%d\n", knobValue[0]);
    fprintf(stderr, "  -r          reverse knob rotation direction\n");
    fprintf(stderr, "  -s <n>      refresh value for panel updates: use with caution\n");
    fprintf(stderr, "                default is -s%ld\n", gpiopattern_update_period_us);
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
    char *cp;
    int i;

    // address and data select knob rotation direction
    if ((cp = getenv("PIDP_11_ROTATION")) != NULL && !strcmp(cp, "FLIP"))
        knobIncrement = -1;
    
    // initial position of address select knob
    if ((cp = getenv("PIDP_11_KNOB_ADDR")) != NULL && (i = atoi(cp)) >= 0 && i < 8)
        knobValue[0] = i;
    
    // initial position of data select knob
    if ((cp = getenv("PIDP_11_KNOB_DATA")) != NULL && (i = atoi(cp)) >= 0 && i < 4)
        knobValue[1] = i;

    return 1;
}

/*
 * read commandline parameters into global vars
 * result: 1 = OK, 0 = error
 */
int
parse_commandline(int argc, char **argv)
{
    int i, c;

    opterr = 0;
    while ((c = getopt(argc, argv, "hbvtLra:d:s:")) != -1) {
        switch (c) {
        case 'h':
            help();
            exit(0);
        case 'b':
            run_in_background = 1;
            break;
        case 'v':
            print_level = LOG_DEBUG;
            break;
        case 't':
            test_mode = 1;
            break;
        case 'L':
            panel_lock = 1;
            break;
        case 'a':
            knobValue[0] = *optarg & 0x7;
            break;
        case 'd':
            knobValue[1] = *optarg & 0x3;
            break;
        case 'r':
            knobIncrement = -1;
            break;
        case 's':
            { char *eos; gpiopattern_update_period_us = strtol(optarg, &eos, 10); }
            if (!gpiopattern_update_period_us) {
                fprintf(stderr, "Illegal value to `-s' (must be integer).\n");
                return 0;
            }
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
 * PIDP11-specific handling of the lamp test switch, and the
 * address select and data select knobs
 */
int
led_fixup(blinkenlight_panel_t *p, blinkenlight_control_t *c, int *panel_mode_ptr,
    uint64_t value, _Atomic uint32_t *gpio_ledstatus)
{

    int panel_mode = p->mode;

    // local LAMPTEST overrides mode set over API
    if (!switch_LAMPTEST->value)        // prototype has lamptest inverted
        panel_mode = RPC_PARAM_VALUE_PANEL_MODE_LAMPTEST ;   
    if (panel_mode_ptr != NULL)
        *panel_mode_ptr = panel_mode;

    // address select - map knobValue[0] to LED values
    if (c == leds_ADDR_SELECT) {
        // circumvent wiring defintions, hard coded logic here:
        // val:   UD  SD  KD CPHY     UI  SI  KI PPHY
        // leds: 4.6 4.7 4.8 4.9     5.5 5.6 5.7 5.8
#define REGMASK_LED_USER_D 0x40
#define REGMASK_LED_SUPER_D 0x80
#define REGMASK_LED_KERNEL_D 0x100
#define REGMASK_LED_CONS_PHY 0x200
#define REGMASK_ADDR_ALL4 0x3C0

#define REGMASK_LED_USER_I 0x40
#define REGMASK_LED_SUPER_I 0x80
#define REGMASK_LED_KERNEL_I 0x100
#define REGMASK_LED_PROG_PHY 0x200
#define REGMASK_ADDR_ALL5 0x3C0

        int mask4 = 0;
        int mask5 = 0;
        switch (panel_mode) {
        case RPC_PARAM_VALUE_PANEL_MODE_NORMAL:
            switch(knobValue[0]) {
            case 0: mask5 |= REGMASK_LED_PROG_PHY; break ;
            case 1: mask4 |= REGMASK_LED_CONS_PHY; break ;
            case 2: mask4 |= REGMASK_LED_KERNEL_D; break ;
            case 3: mask4 |= REGMASK_LED_SUPER_D ; break ;
            case 4: mask4 |= REGMASK_LED_USER_D ; break ;
            case 5: mask5 |= REGMASK_LED_USER_I ; break ;
            case 6: mask5 |= REGMASK_LED_SUPER_I ; break ;
            case 7: mask5 |= REGMASK_LED_KERNEL_I; break ;
            }
        break;

        case RPC_PARAM_VALUE_PANEL_MODE_LAMPTEST:
        case RPC_PARAM_VALUE_PANEL_MODE_ALLTEST:
            mask4 = REGMASK_ADDR_ALL4 ; // all ON
            mask5 = REGMASK_ADDR_ALL5 ; // all ON
            break;


        case RPC_PARAM_VALUE_PANEL_MODE_POWERLESS:
            mask4 = 0 ; // all off
            mask5 =0;
            break;
        }
        // mask all out and set selective
        gpio_ledstatus[4] = (gpio_ledstatus[4] & ~REGMASK_ADDR_ALL4) | mask4 ;
        gpio_ledstatus[5] = (gpio_ledstatus[5] & ~REGMASK_ADDR_ALL5) | mask5 ;

        return 1;
    }

    // data select - map knobValue[1] to LED values
    if (c == leds_DATA_SELECT) {
        // circumvent wiring defintions, hard coded logic here:
        // val:   DP  BR   uAD DR
        // leds: 4.10 4.11 5.10 5.11
#define REGMASK_LED_DATA_PATHS 0x400
#define REGMASK_LED_BUS_REG 0x800
#define REGMASK_DATA_ALL4 0xC00

#define REGMASK_LED_UADR 0x400
#define REGMASK_LED_DISREG 0x800
#define REGMASK_DATA_ALL5 0xC00

        int mask4 = 0;
        int mask5 = 0;
        switch (panel_mode) {
        case RPC_PARAM_VALUE_PANEL_MODE_NORMAL:
            switch(knobValue[1]) {
                case 0:
                case 4:
                    mask4 |= REGMASK_LED_BUS_REG ;
                    break ;
                case 1:
                case 5:
                    mask4 |= REGMASK_LED_DATA_PATHS ;
                    break ;
                case 2:
                case 6:
                    mask5 |= REGMASK_LED_UADR;
                    break ;
                case 3: 
                case 7:
                    mask5 |= REGMASK_LED_DISREG;
                    break ;
            }
        break;

        case RPC_PARAM_VALUE_PANEL_MODE_LAMPTEST:
        case RPC_PARAM_VALUE_PANEL_MODE_ALLTEST:
            mask4 = REGMASK_DATA_ALL4 ; // all ON
            mask5 = REGMASK_DATA_ALL5 ; // all ON
            break;


        case RPC_PARAM_VALUE_PANEL_MODE_POWERLESS:
            mask4 = 0 ; // all off
        mask5 = 0;
            break;
        }
        // mask all out and set selective
        gpio_ledstatus[4] = (gpio_ledstatus[4] & ~REGMASK_DATA_ALL4) | mask4 ;
        gpio_ledstatus[5] = (gpio_ledstatus[5] & ~REGMASK_DATA_ALL5) | mask5 ;

        return 1;
    }

    return 0;
}

/*
 * PIDP11-specific handling of the rotary knobs (switch row 2)
 *
 * Kudos to Johnny Billquist <bqt@softjar.se> for the underlying approach
 */
void
switch_fixup(int row, int switchscan)
{
    // 2 rotary encoders. Each has two switch pins. Normally, both are 0 - no rotation.
    // encoder 1: row1, bits 8,9. Encoder 2: row1, bits 10,11
    // Gray encoding: rotate up sequence   = 11 -> 01 -> 00 -> 10 -> 11
    // Gray encoding: rotate down sequence = 11 -> 10 -> 00 -> 01 -> 11

    // Movement direction based upon previous code and current code
    // Use by looking up gray_shift[previous][current]
    const static enum direction { NOP, CW, CCW } gray_shift[4][4] = {
        { NOP, CCW, CW, NOP },
        { CW, NOP, NOP, CCW },
        { CCW, NOP, NOP, CW },
        { NOP, CW, CCW, NOP }
    };
    const unsigned KNOB_SCALE = 4; // 4 code changes per knob position
    static struct {
        const int scanshift;    // shift offset of bits in switchscan
        const unsigned mask;    // (scale * number of positions) - 1
        unsigned state;         // current state (scale * current position)
        unsigned previous;      // previous value
    } knob[2] = {
        { 8, (KNOB_SCALE * 8) - 1 },    // address select knob
        { 10, (KNOB_SCALE * 4) - 1 }    // data select knob
    };
    static int first_run = 1;
    int i;

    // There's nothing to do unless this is switch row 2
    if (row != 2)
        return;
    
    // For each knob, determine the movement direction and adjust
    // the state accordingly. The knobValue only changes when there
    // have been cumulative KNOB_SCALE changes in the same direction.
    for (i = 0; i < 2; i++) {
        unsigned current = (switchscan >> knob[i].scanshift) & 3;
        if (first_run) {
            // Initialize the knob state based upon the current knobValue,
            // which may have been set by a command line argument or
            // environment variable
            knob[i].state = knobValue[i] * KNOB_SCALE;
        } else {
            // Update the knob state based upon the previous and
            // current Gray codes
            switch (gray_shift[knob[i].previous][current]) {
            case CW:    knob[i].state += knobIncrement; break;
            case CCW:   knob[i].state -= knobIncrement; break;
            }
        }
        knob[i].previous = current;
        knob[i].state &= knob[i].mask;
        knobValue[i] = knob[i].state / KNOB_SCALE;
    }
    first_run = 0;
}
