// PiDP-10 front panel driver.
//
// 20240302 OV for final PCB layout

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h> 
#include <time.h>				//Needed for nanosleep
#include <pthread.h>			//Needed for pthread
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <gpiolib.h>
typedef uint64_t uint64;
#include "scansw10.h"

// the interface variables shared with other threads
volatile u_int32_t gpio_switchstatus[5] ; // bitfields: 5 rows of up to 18 switches
volatile u_int32_t gpio_ledstatus[8] ;	// bitfields: 7 ledrows of up to 18 LEDs

// for multiplexing, sleep time between row switches in ns (but excl. overhead!)
long intervl = 50000;

// GPIO pins: function & definitions
//		GPIO 22 ('xIO') enables either the 74hc138 (when low) or the 74hc238 (when high)
//
//		GPIO 4,17 and 27 are the 'xrows', a 3 bit address fed into both 74hc chips,
//		which then decode that 3 bit address to enable one of their 8 output pins
//
//		7 rows on the 74hc238 each provide power to a block of 18 LEDs
//			(a LED will then light up when its column pin is set to LOW!)
//		5 rows on the 74hc138 each sink the current for a block of 18 switches 
//			(a set switch pulls its column pin to GND, overruling its internal pull-up)
//
//		when xIO is set to output LOW, 
//		- the 74hc238 decodes these 3 address lines into 
//		  one of the 7 blocks of LEDs on the board
//		- to light up a LED, set its column GPIO pin LOW. 
//		  To keep it dark, set that GPIO pin HIGH
//
//		and when xIO is set to output HIGH,
//		- the 74hc138 decodes these 3 address lines into 
//		  one of the 5 blocks of switches on the board 
//		- the column pins needs to be set to input-with-pullup-high to read switches

u_int8_t xrows[3] = { 4, 17, 27 };
u_int8_t xIO = 22;			// GPIO 22: 0=drive LEDs, 1=read switches
u_int8_t cols[18] = { 21,20,16,12,7,8,25,24,23,  18,10,9,11,5,6,13,19,26 };


int
main(int argc, const char *const *argv)
{
    const char *argv0 = (argc > 0) ? argv[0] : "scansw10";
    char radix = 'u';
    int width = 0;
    int zerofill = 0;
    uint64_t bitmask = (1uL << 36) - 1;
    char format[16];
	int	col, row, i, switchscan, tmp;
    int num_gpios, ret;
	uint64_t result;
	
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
	
	// init GPIO ----------
	ret = gpiolib_init();
	if (ret < 0)
	{	printf("Failed to initialise gpiolib - %d\n", ret);
		return -1;
	}

	num_gpios = ret;
	if (!num_gpios)
	{	printf("No GPIO chips found\n");
		return -1;
	}

	ret = gpiolib_mmap();
	if (ret)
	{	if (ret == EACCES && geteuid())
			printf("Must be root\n");
		else
			printf("Failed to mmap gpiolib - %s\n", strerror(ret));
		return -1;
	}

	// initialise GPIO pins
	// - xIO pin
	gpio_set_fsel(xIO, GPIO_FSEL_OUTPUT);
	gpio_set_dir(xIO, DIR_OUTPUT);
	gpio_set_drive(xIO, DRIVE_HIGH);
	
	// - row address pins
	for (row = 0; row < 3; row++) {
		gpio_set_fsel(xrows[row], GPIO_FSEL_OUTPUT);
		gpio_set_dir(xrows[row], DIR_OUTPUT);
		gpio_set_drive(xrows[row], DRIVE_HIGH);

	}
	// - columns
	for (col = 0; col < 18; col++) { // Define ledrows as input
		gpio_set_fsel(cols[col], GPIO_FSEL_INPUT);
		gpio_set_pull(cols[col], PULL_UP);
	}
	
	// start the actual multiplexing
		
	gpio_set_drive(xIO, DRIVE_HIGH);
	nanosleep((struct timespec[]) {	{	0, intervl/2}}, NULL);

	// ---- set cols to 18 bits of input -----
	for (i = 0; i < 18; i++) 
		gpio_set_dir(cols[i], DIR_INPUT);


	for (row=0; row<5; row++)		// there are 5 rows of 18 switches each
	{
		// ----- select row from address
		for(i=0; i<3;i++)
		{	if((row & (1<<i)) == 0)
			gpio_set_drive(xrows[i], DRIVE_LOW);
			else	
				gpio_set_drive(xrows[i], DRIVE_HIGH);
		}

		nanosleep((struct timespec[]) { { 0, intervl / 10}}, NULL); 

		// ----- read switches 		
		switchscan = 0;
		for (i = 0; i < 18; i++) // 18 switches in each row
		{	tmp = gpio_get_level(cols[i]);
			if (tmp != 0)
				switchscan += 1 << i;
		}
		gpio_switchstatus[row] = switchscan;
	}

	result = ~(((uint64_t)gpio_switchstatus[1] << 18) |
				(gpio_switchstatus[0] & 0777777)) & bitmask;

	// print the switch register according to the format determined above
	printf(format, result);

	return result & 0177;
}
