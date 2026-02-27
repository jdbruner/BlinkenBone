#
# Platform check - this only builds on Raspberry Pi
#
ifeq (,$(wildcard /usr/bin/raspi-config))
  $(error panel servers only build on Raspberry Pi)
endif

#
# Build scansw for PiDP10
#
OBJDIR=../../BIN
TARGETS = $(OBJDIR)/scansw10
all:    $(TARGETS)

#
# CC Command
#
ifeq ($(MAKE_CONFIGURATION),RELEASE)
CFLAGS = -O2 -Wno-unused-result
else
CFLAGS = -ggdb3 -O0 -Wall -Wextra
endif

LDFLAGS = -lgpiolib

clean:
	rm -f core *.o $(TARGETS)

install:
	# no-op for now

scansw10.o:	scansw10.c scansw10.h

$(OBJDIR)/scansw10:	scansw10.o
	${CC} -o $@ $^ ${LDFLAGS}
