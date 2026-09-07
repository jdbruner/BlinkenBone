#
# Build scansw
#
OBJDIR=../../BIN
TARGETS = $(OBJDIR)/scansw00
ifneq (,$(wildcard /usr/bin/raspi-config))
  TARGETS += $(OBJDIR)/scansw10 $(OBJDIR)/scansw11
endif

all:    $(TARGETS)

#
# CC Command
#
ifeq ($(MAKE_CONFIGURATION),RELEASE)
CFLAGS = -O2 -Wno-unused-result
else
CFLAGS = -ggdb3 -O0 -Wall -Wextra
endif

LDFLAGS =
GPIOLIB = -lgpiolib

clean:
	rm -f core *.o $(TARGETS)

install:
	# no-op for now

$(OBJDIR)/scansw00:	scansw00.c
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS}

$(OBJDIR)/scansw10:	scansw10.c
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS} ${GPIOLIB}

$(OBJDIR)/scansw11:	scansw11.c
	${CC} ${CFLAGS} -o $@ $^ ${LDFLAGS} ${GPIOLIB}
