/* getcsw.c: Scan console switches using blinkenlight server
 *
 * Copyright (c) 2025 John D. Bruner
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JOERG HOPPE BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <sys/param.h>
#include "blinkenlight_api_client.h"

#define SERVER_NAME "localhost"
#define PDP1170_NAME "11/70"
#define PDP1170_SWITCH_REGISTER "SR"

int
main(int argc, const char *const *argv)
{
    int retval = 1;
    const char *argv0 = (argc > 0) ? argv[0] : "blscansw";
    char radix = 'u';
    int width = 0;
    int zerofill = 0;
    unsigned long bitmask = ~0uL;
    char format[16];

    while (1) {
        /* parse arguments */
        int c = getopt(argc, (char **)argv, "0d::o::x::n:?");
        if (c == -1)
            break;

        switch (c) {
        case '0':
            /* normally used with nonzero width, left fill with zeros */
            zerofill = 1;
            break;

        case 'd':
        case 'u':
        case 'o':
        case 'x':
            /* sthe numeric radix formatter (decimal, octal, hexadecimal) */
            radix = (c == 'd') ? 'u' : c;
            if (optarg != NULL) {
                width = atoi(optarg);
                width = MAX(0, MIN(width,24));
            }
            break;
        
        case 'n':
            /* number of significant bits, starting from the low end */
            if (optarg != NULL) {
                int nbits = atoi(optarg);
                if (nbits <= 0 || nbits >= 8 * sizeof(unsigned long))
                    bitmask = ~0uL;
                else
                    bitmask = (1uL << nbits) - 1;
            }
            break;

        default:
            fprintf(stderr, "%s: unknown argument \"%c\"\n", c);
            // fall through
        case '?':
            fprintf(stderr, "Usage: \"%s [-0] [-d[N]|-o[N]|-x[N]] [-nN]\"\n", argv0);
            return -1;
        }
    }

    /* create the format string for the final output */
    snprintf(format, sizeof format, "%%%s%dl%c\n", zerofill ? "0" : "", width, radix);

    /* connect to the blinkenlight server */
    blinkenlight_api_client_t *blinkenlight_api_client = blinkenlight_api_client_constructor();

	if (blinkenlight_api_client_connect(blinkenlight_api_client, SERVER_NAME) != 0) {
		fputs(blinkenlight_api_client_get_error_text(blinkenlight_api_client), stderr);
		return 1;
	}

	/* load defined panels and controls from server */
	if (blinkenlight_api_client_get_panels_and_controls(blinkenlight_api_client) != 0) {
		fputs(blinkenlight_api_client_get_error_text(blinkenlight_api_client), stderr);
		goto out;
	}

    /* get the PDP-11/70 panel and its input controls */
    blinkenlight_panel_t *pdp1170_panel =
        blinkenlight_panels_get_panel_by_name(blinkenlight_api_client->panel_list, PDP1170_NAME);
    
    if (pdp1170_panel == NULL) {
        fprintf(stderr, "%s: %s panel not found\n", argv0, PDP1170_NAME);
        goto out;
    }

    if (blinkenlight_api_client_get_inputcontrols_values(blinkenlight_api_client, pdp1170_panel) != 0) {
        fputs(blinkenlight_api_client_get_error_text(blinkenlight_api_client), stderr);
        goto out;
    }

    /* get the current value of the switch register */
    blinkenlight_control_t *switch_register =
        blinkenlight_panels_get_control_by_name(blinkenlight_api_client->panel_list,
                                                pdp1170_panel, PDP1170_SWITCH_REGISTER, 1);
    if (switch_register == NULL) {
        fprintf(stderr, "%s: %s control not found\n", argv0, PDP1170_SWITCH_REGISTER);
        goto out;
    }

    /* print the switch register according to the format determined above */
    printf(format, (unsigned long)switch_register->value & bitmask);
    retval = 0;

out:
    if (blinkenlight_api_client != NULL) {
        blinkenlight_api_client_disconnect(blinkenlight_api_client);
        blinkenlight_api_client_destructor(blinkenlight_api_client);
    }
    return retval;
}
