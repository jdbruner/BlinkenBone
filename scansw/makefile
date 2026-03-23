#
# Build scansw
#
OBJDIR=../../BIN
TARGETS = $(OBJDIR)/scansw00
ifneq (,$(wildcard /usr/bin/raspi-config))
  TARGETS += $(OBJDIR)/scansw10
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

$(OBJDIR)/scansw00:	scansw00.o
	${CC} -o $@ $^ ${LDFLAGS}

$(OBJDIR)/scansw10:	scansw10.o
	${CC} -o $@ $^ ${LDFLAGS} ${GPIOLIB}
