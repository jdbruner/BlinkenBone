/*
 * PiDP-11 switch scan
 */

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h> 
#include <time.h>               // Needed for nanosleep
#include <pthread.h>            // Needed for pthread
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <gpiolib.h>

/*
 * GPIO row and column definitions
 */
#define _countof(x) (sizeof x / sizeof x[0])
const unsigned rows[] = { 16, 17, 18 };                                  // switch rows
const unsigned num_rows = _countof(rows);                                // number of switch rows
const unsigned cols[] = { 26, 27, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };    // columns
const unsigned num_cols = _countof(cols);                                // number of columns

int
main(int argc, const char *const *argv)
{
    const char *argv0 = (argc > 0) ? argv[0] : "scansw10";
    char radix = 'u';
    int width = 0;
    int zerofill = 0;
    unsigned bitmask = (1u << 22) - 1;
    char format[16];
    int switchscan[_countof(rows)];
    unsigned result;
    register unsigned i, j;
    
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
            /* the numeric radix formatter (decimal, octal, hexadecimal) */
            radix = (c == 'd') ? 'u' : c;
            if (optarg != NULL) {
                width = atoi(optarg);
                width = MAX(0, MIN(width,8));
            }
            break;
        
        case 'n':
            /* number of significant bits, starting from the low end */
            if (optarg != NULL) {
                int nbits = atoi(optarg);
                if (nbits <= 0 || nbits > 22)
                    nbits = 22;
                bitmask = (1u << nbits) - 1;
            }
            break;

        default:
            fprintf(stderr, "%s: unknown argument \"%c\"\n", argv0, c);
            /* fall through */
        case '?':
            fprintf(stderr, "Usage: \"%s [-0] [-d[N]|-o[N]|-x[N]] [-nN]\"\n", argv0);
            return 1;
        }
    }

    /* create the format string for the final output */
    snprintf(format, sizeof format, "%%%s%dl%c\n", zerofill ? "0" : "", width, radix);


    /* initialize and map gpiolib */
    if (gpiolib_init() < 0) {
        fprintf(stderr, "gpiolib_init failed\n");
        return 1;
    }
    if (gpiolib_mmap() != 0) {
        fprintf(stderr, "gpiolib_mmap: %s\n", strerror(errno));
        return 1;
    }

    /* initialize switch rows (output, high) */
    for (i = 0; i < num_rows; i++) {
        gpio_set_fsel(rows[i], GPIO_FSEL_OUTPUT);
        gpio_set_drive(rows[i], DRIVE_HIGH);
    }

    /* initialize columns as inputs with pull up */
    for (i = 0; i < num_cols; i++) {
        gpio_set_fsel(cols[i], GPIO_FSEL_INPUT);
        gpio_set_pull(cols[i], PULL_UP);
    } 

    /* enable each row and read the switches in that row */
    for (i = 0; i < num_rows; i++) {
        gpio_set_drive(rows[i], DRIVE_LOW);
        usleep(1); // allow inputs to settle
        switchscan[i] = 0;
        for (j = 0; j < num_cols; j++)
            switchscan[i] |= (gpio_get_level(cols[j]) == DRIVE_HIGH) << j;
        gpio_set_drive(rows[i], DRIVE_HIGH);
    }

    result = ~((switchscan[1] << num_cols) | switchscan[0]) & bitmask;

    /* print the switch register according to the format determined above */
    printf(format, result);

    return result & 0177;
}