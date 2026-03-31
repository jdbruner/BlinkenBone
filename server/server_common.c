 /* server_common.c: common code for all Blinkenlight API panel servers

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

 This is the code that is common to all Blinkenlight API panel
 servers. It provides the generic handling of the Blinkenlight
 API interface, which is extended and overriddden by the panel-
 specific servers.
*/

#define _GNU_SOURCE     // for asprintf
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <inttypes.h> 
#include <unistd.h>
#include <signal.h>
#include <assert.h>

#include "blinkenlight_panels.h"
#include "blinkenlight_api_server_procs.h"

///// for Blinkenlight API server
#include "rpc_blinkenlight_api.h"
#include <rpc/pmap_clnt.h> 
#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

#include "bitcalc.h"
#include "print.h"

#include "globals.h"
#include "gpiopattern.h"

char *program_info = NULL;      // description of this program
const char *program_name = NULL;// argv[0]
char *program_options = NULL;   // argv[1..argc]
int test_mode = 0;              // test mode: diagprint and exit
int run_in_background = 0;      // run in background, print to syslog

/*
 * Override for get control value for an individual control
 * Returns 1 if it handled the control, 0 otherwise
 */
static int
blinkenlight_api_panel_get_controlvalue(blinkenlight_panel_t *p,
    blinkenlight_control_t *c)
{
    return 0;
}

/*
 *  RPC server callbacks:
 *  here conversion between gpio and Blinkenlight API is done!
 */
static void
on_blinkenlight_api_panel_get_controlvalues(blinkenlight_panel_t *p)
{
    // gets called when RPC client wants panel input control values
    //- converts gpio switches to Blinkenlight API switch conrols (RPI->Blinkenlight API)
    unsigned i;

    for (i = 0; i < p->controls_count; i++) {
        blinkenlight_control_t *c = &p->controls[i];
        if (c->is_input) {
            // check for special handling for this control
            if ((*blinkenlight_callback_handlers.panel_get_controlvalue)(p, c))
                continue;

            // mount switch value from register bit fields
            unsigned i_register_wiring;
            blinkenlight_control_blinkenbus_register_wiring_t *bbrw;
            uint64_t temp_value = 0; // Do not use c->value because the gpiopattern thread does 20181227

            for (i_register_wiring = 0; i_register_wiring < c->blinkenbus_register_wiring_count;
                    i_register_wiring++) {
                uint32_t regvalbits; // value of current register
                // for all registers assigned whole or in part to control
                bbrw = &(c->blinkenbus_register_wiring[i_register_wiring]);

                regvalbits = gpio_switchstatus[bbrw->blinkenbus_register_address];
                if (bbrw->blinkenbus_levels_active_low) //  inputs "low active"
                    regvalbits = ~regvalbits;
                regvalbits &= bbrw->blinkenbus_bitmask; // bits of value, unshiftet
                regvalbits >>= bbrw->blinkenbus_lsb;
                // OR in the bits of current register
                temp_value |= (uint64_t) regvalbits << bbrw->control_value_bit_offset;  // 20181227
            }
            if (c->mirrored_bit_order)
                c->value = mirror_bits(temp_value, c->value_bitlen); // individual fixup/logic 20181227
            else // 20181227
                c->value = temp_value; //20181227
        }
    }
}

// called after value of a control has been set
static void
on_blinkenlight_api_panel_set_controlvalue(blinkenlight_panel_t *p,
    blinkenlight_control_t *c)
{
    // no-op
}

// gets called when RPC client updated panel control values
static void
on_blinkenlight_api_panel_set_controlvalues(blinkenlight_panel_t *p, int force_all)
{
    // Conversion of the Blinkenlight API leds to GPIOs is done
    // in the averaging thread. Therefore, the averaging thread
    // needs to be informed about the panel
    //
    // THIS WORKS ONLY BECAUSE ONLY ONE PANEL is provided by this server!
    // NO PANEL SWITCH ALLOWED!

    gpiopattern_blinkenlight_panel = p;
}

// get panel active state
static int
on_blinkenlight_api_panel_get_state(blinkenlight_panel_t *p)
{
    // dummy: "all BlinkenBoards of panel active"
    return RPC_PARAM_VALUE_PANEL_BLINKENBOARDS_STATE_ACTIVE;
}

// set panel active state
static void
on_blinkenlight_api_panel_set_state(blinkenlight_panel_t *p, int new_state)
{
    // no-op
}

// get selftest/powerless mode
static int
on_blinkenlight_api_panel_get_mode(blinkenlight_panel_t *p)
{
    return p->mode;
}

// set selftest/powerless mode
static void
on_blinkenlight_api_panel_set_mode(blinkenlight_panel_t *p, int new_state)
{
    p->mode = new_state;
    // GPIO logic here
    (*blinkenlight_callback_handlers.panel_set_controlvalues)(p, 1);
}

// get server info
static char *
on_blinkenlight_api_get_info()
{
    char *info = NULL;
    if (asprintf(&info, "Server info ...............: %s\n"
        "Server program name........: %s\n"
        "Server command line options: %s\n"
        "Server compile time .......: " __DATE__ " " __TIME__ "\n",
        program_info, program_name, program_options) < 0)
        info = "Server info *unknown*\n";
    return info;
}

/*
 * Default Blinkenlight RPC event handlers
 *
 * These are used when no override is defined by the specific panel.
 */
const blinkenlight_callback_handlers_t blinkenlight_callback_handlers_default = {
    blinkenlight_api_panel_get_controlvalue,
    on_blinkenlight_api_panel_get_controlvalues,
    on_blinkenlight_api_panel_set_controlvalue,
    on_blinkenlight_api_panel_set_controlvalues,
    on_blinkenlight_api_panel_get_state,
    on_blinkenlight_api_panel_set_state,
    on_blinkenlight_api_panel_get_mode,
    on_blinkenlight_api_panel_set_mode,
    on_blinkenlight_api_get_info
};

/*
 * Update the panel-specific set of handlers, replacing any NULL entries with
 * pointers to the respective default handlers. Register all RPC event handlers.
 */
static void
register_blinkenlight_callbacks()
{
// if the panel-specific handler is NULL, use the default one
#define POPULATE_DEFAULT(handler) \
    if (blinkenlight_callback_handlers.handler == NULL) \
        blinkenlight_callback_handlers.handler = \
            blinkenlight_callback_handlers_default.handler

// register a Blinklight event handler
#define REGISTER_BL_EVENT(handler) \
    blinkenlight_api_##handler##_evt = \
        blinkenlight_callback_handlers.handler

// update handler with default (if needed) and register event
#define OVERRIDE_AND_REGISTER(handler) \
    POPULATE_DEFAULT(handler); REGISTER_BL_EVENT(handler)

    POPULATE_DEFAULT(panel_get_controlvalue);
    OVERRIDE_AND_REGISTER(panel_get_controlvalues);
    OVERRIDE_AND_REGISTER(panel_set_controlvalue);
    OVERRIDE_AND_REGISTER(panel_set_controlvalues);
    OVERRIDE_AND_REGISTER(panel_get_state);
    OVERRIDE_AND_REGISTER(panel_set_state);
    OVERRIDE_AND_REGISTER(panel_get_mode);
    OVERRIDE_AND_REGISTER(panel_set_mode);
    OVERRIDE_AND_REGISTER(get_info);
}

/*
 * Start the parallel thread which operates the GPIO mux.
 */
void *blink(void *ptr); // the real-time GPIO multiplexing process to start up

pthread_t blink_thread;
_Atomic int blink_thread_terminate = 0;
pthread_t gpiopattern_thread;
_Atomic int gpiopattern_thread_terminate = 0;
// blinkenlight_api_server runs on the main thread
_Atomic int blinkenlight_thread_terminate = 0;

static void
gpio_mux_thread_start()
{
    int res;
    res = pthread_create(&blink_thread, NULL, blink, &blink_thread_terminate);
    if (res) {
        print(LOG_ERR, "Error creating gpio_mux thread, return code %d\n", res);
        exit(EXIT_FAILURE);
    }
    sleep(2); // allow 2 sec for multiplex to start
}

static void
gpiopattern_start_thread()
{
    int res;
    gpiopattern_blinkenlight_panel = NULL; // wait for first API transmission to start

    res = pthread_create(&gpiopattern_thread, NULL, gpiopattern_update_leds,
            &gpiopattern_thread_terminate);
    if (res) {
        print(LOG_ERR, "Error creating gpiopattern thread, return code %d\n", res);
        exit(EXIT_FAILURE);
    }
}

/******************************************************
 * Server for Blinkenlight API
 * see code generated by rpcgen
 */
void
blinkenlight_api_server(void)
{
    register SVCXPRT *transp;

// entry to server stub

    void blinkenlightd_1(struct svc_req *rqstp, register SVCXPRT *transp);

    pmap_unset(BLINKENLIGHTD, BLINKENLIGHTD_VERS);

    transp = svcudp_create(RPC_ANYSOCK);
    if (transp == NULL) {
        print(LOG_ERR, "%s", "cannot create udp service.");
        exit(1);
    }
    if (!svc_register(transp, BLINKENLIGHTD, BLINKENLIGHTD_VERS, blinkenlightd_1, IPPROTO_UDP)) {
        print(LOG_ERR, "%s", "unable to register (BLINKENLIGHTD, BLINKENLIGHTD_VERS, udp).");
        exit(1);
    }

    transp = svctcp_create(RPC_ANYSOCK, 0, 0);
    if (transp == NULL) {
        print(LOG_ERR, "%s", "cannot create tcp service.");
        exit(1);
    }
    if (!svc_register(transp, BLINKENLIGHTD, BLINKENLIGHTD_VERS, blinkenlightd_1, IPPROTO_TCP)) {
        print(LOG_ERR, "%s", "unable to register (BLINKENLIGHTD, BLINKENLIGHTD_VERS, tcp).");
        exit(1);
    }

    // svc_run();
    // alternate implementation of svn_run() with periodically timeout and
    //  calling of callback
    {
        fd_set readfds;
        struct timeval tv;
        int dtbsz = getdtablesize();
        while (!blinkenlight_thread_terminate) {
            readfds = svc_fdset;
            tv.tv_sec = 0;
            tv.tv_usec = 1000 * 2; // every 10 ms*time_slice_ms;
            switch (select(dtbsz, &readfds, NULL, NULL, &tv)) {
            case -1:
                if (errno == EINTR)
                    continue;
                perror("select");
                return;
            case 0: // timeout
                    // provide the panel simulation with computing time
                // not needed:RPC calls control value get/set callbacks
                break;
            default:
                svc_getreqset(&readfds);
                break;
            }
            /**/
        }
    }
}

/*
 * print program info
 */
static void
info(void)
{
    print(LOG_INFO, "\n");
    print(LOG_NOTICE, "*** %s %s - server for %s ***\n", SERVERNAME, VERSION, PANEL_NAME);
    print(LOG_NOTICE, "    Compiled " __DATE__ " " __TIME__ "\n");
    print(LOG_NOTICE, "    Copyright (C) 2015-2026 Joerg Hoppe, Oscar Vermeulen, John D. Bruner.\n");
    print(LOG_NOTICE, "    www.retrocmp.com, obsolescence.wix.com/obsolescence\n");
    print(LOG_NOTICE, "\n");
}

/*
 * Define part of the fix controls of the PiDP server
 * for the Blinkenlight API interface structs
 * - control_value_bit_offset: position of bitfield in final control value
 * - bitlen: count of bits in status register to be mounted into value
 * - gpio_switchstatus_index: gpio_switchstatus[idx] is mounted here
 * - bit_offset: LSB of bitfield in gpio_switchstatus[] to be shifted this much
 *
 * See http://retrocmp.com/projects/blinkenbone/blinkenbone-physical-panels/173-blinkenbone-blinkenlightd-patch-field-decoding-and-console-panel-simulator
 */
static void
define_slice(blinkenlight_panel_t *p, const control_slice_info_t *csi)
{
    blinkenlight_control_t *c;
    blinkenlight_control_blinkenbus_register_wiring_t *bbrw;

    // control already there?
    c = blinkenlight_panels_get_control_by_name(blinkenlight_panel_list, p, csi->name, csi->is_input);
    if (c == NULL) {
        c = blinkenlight_add_control(blinkenlight_panel_list, p);
        assert(c != NULL);
        strcpy(c->name, csi->name);
        c->is_input = csi->is_input;
        c->type = csi->type;
        c->encoding = binary;
        c->radix = csi->radix;
        c->fmax = csi->fmax;
    }
    // shift and mask data are saved in the "register wiring" struct.
    bbrw = blinkenlight_add_register_wiring(c);
    bbrw->blinkenbus_board_address = 0; // simulate 1 board with unlimited registers
    bbrw->board_register_address = csi->row_index; // register here mux row
    bbrw->control_value_bit_offset = csi->slice_offset;
    bbrw->blinkenbus_lsb = csi->bit_offset;
    bbrw->blinkenbus_msb = bbrw->blinkenbus_lsb + csi->bitlen - 1;
    bbrw->blinkenbus_levels_active_low = csi->active_low;
}

static void
register_controls()
{
    blinkenlight_panel_t *p;
    const control_slice_info_t *csi;

    // one global panel list ...
    blinkenlight_panel_list = blinkenlight_panels_constructor();
    // ... with one panel
    p = blinkenlight_add_panel(blinkenlight_panel_list);
    strcpy(p->name, PANEL_NAME);
    strcpy(p->info, PANEL_DESCRIPTION);

    /*
     * Construct high-level Blinkenlight API controls from
     * descriptions of hardware switch- and led-registers
     */
    for (csi = panel_controls; csi->name != NULL; csi++)
        (void)define_slice(p, csi);
    
    /*
     * Do any post-creation actions that are panel-specific.
     * Notably, this is the place to get pointers to controls
     * that will subsequently be used in  panel-specific handlers.
     */
    panel_creation_fixup(p);

    blinkenlight_panels_config_fixup(blinkenlight_panel_list);
}

/*
 * Termination signal handler - set terminate flags for worker threads.
 */
static void
kill_handler(int signum)
{
    blink_thread_terminate = 1;
    gpiopattern_thread_terminate = 1;
    blinkenlight_thread_terminate = 1;
}

/*
 * Main program
 */
int
main(int argc, char *argv[])
{
    const int killsignals[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM };
    size_t program_options_size;
    int i;

    assert(num_ledrows <= GPIOPATTERN_MAX_LED_ROWS);
    assert(num_rows <= GPIOPATTERN_MAX_SWITCH_ROWS);

    program_name = argv[0];
    program_options_size = 0;
    for (i = 1; i < argc; i++)
        program_options_size += strlen(argv[i]) + 1;
    if ((program_options = malloc(program_options_size)) != NULL) {
        char *cp = program_options;
        *cp = '\0';
        for (i = 1; i < argc; i++) {
            cp = stpcpy(cp, argv[i]);
            if (i < argc-1)
                *cp++ = ' ';
        }  
    } else {
        perror("malloc");
        program_options = "** error **";
    }
    
    print_level = LOG_NOTICE;
    if (!parse_environment() || !parse_commandline(argc, argv))
        return 1;

    if (asprintf(&program_info,
        "%s - Blinkenlight API server daemon for %s %s",
        SERVERNAME, PANEL_NAME, VERSION) < 0)
        return 1;

    // start logging
    print_open(run_in_background); // if background, then syslog
    print(LOG_INFO, "Start\n");

    // link set/get events
    register_blinkenlight_callbacks();

    // register all controls
    register_controls();

    if (test_mode) {
        printf("Dump of register <-> control data struct:\n");
        blinkenlight_panels_diagprint(blinkenlight_panel_list, stdout);
        exit(0);
    }

    gpio_mux_thread_start();
    gpiopattern_start_thread();

    // catch non-ignored HUP, INT, QUIT, TERM signals
    // and terminate all threads cleanly
    for (i = 0; i < sizeof killsignals / sizeof *killsignals; i++) {
            const struct sigaction kill_action = { .sa_handler = kill_handler, .sa_flags = SA_RESETHAND };
            struct sigaction old_action;

            if (sigaction(killsignals[i], NULL, &old_action) == 0 &&
            old_action.sa_handler != SIG_IGN)
                sigaction(killsignals[i], &kill_action, NULL);
    }

    blinkenlight_api_server();

    print_close();

    // wait for mux and pattern threads to finish
    pthread_join(blink_thread, NULL);
    pthread_join(gpiopattern_thread, NULL);

    blinkenlight_panels_destructor(blinkenlight_panel_list);

    return 0;
}
