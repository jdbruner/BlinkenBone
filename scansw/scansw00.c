// Fake switch scan

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
typedef uint64_t uint64;

int
main(int argc, const char *const *argv)
{
    const char *argv0 = (argc > 0) ? argv[0] : "scansw10";
    char radix = 'u';
    int width = 0;
    int zerofill = 0;
    char format[16];
    uint64_t bitmask = (1uL << 36) - 1;
    uint64_t result = 0; // default value
	
    while (1) {
        /* parse arguments */
        int c = getopt(argc, (char **)argv, "0d::o::x::n:v:?");
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
                width = MAX(0, MIN(width,12));
            }
            break;
        
        case 'n':
            /* number of significant bits, starting from the low end */
            if (optarg != NULL) {
                int nbits = atoi(optarg);
                if (nbits <= 0 || nbits >36)
                    nbits = 36;
                bitmask = (1uL << nbits) - 1;
            }
            break;

        case 'v':
            /* value to be printed (instead of 0) */
            if (optarg != NULL)
                result = (uint64_t)strtoll(optarg, NULL, 0);
            break;

        default:
            fprintf(stderr, "%s: unknown argument \"%c\"\n", argv0, c);
            /* fall through */
        case '?':
            fprintf(stderr, "Usage: \"%s [-0] [-d[N]|-o[N]|-x[N]] [-nN] [-vN]\"\n", argv0);
            return 1;
        }
    }

    /* create the format string for the final output and print it */
    snprintf(format, sizeof format, "%%%s%dl%c\n", zerofill ? "0" : "", width, radix);
    printf(format, result & bitmask);

    return result & 0177;
}
