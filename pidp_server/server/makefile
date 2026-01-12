# call with MAKE_TARGET_ARCH=RPI
# if using libgpiod-dev v1, call with USE_LIBGPIOD_V1=1

#
# CC Command
#
CC_DBG_FLAGS = -ggdb3 -O0
OS_CCDEFS=-I/usr/include/tirpc
OBJDIR=../../../BIN
LDFLAGS=-lrt -pthread -lgpiod -ltirpc

ifneq (,$(USE_LIBGPIOD_V1))
	GPIOSRC=gpio_v1.c
else
	GPIOSRC=gpio.c
endif

CCDEFS= \
	-DBLINKENLIGHT_SERVER	\
	-I.	\
	-I$(BLINKENLIGHT_COMMON_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)/rpcgen_linux	\
	$(CC_DBG_FLAGS) $(OS_CCDEFS)


BLINKENLIGHT_COMMON_DIR=../../common
# sub dir for the API. is same level (= sibling) as server and client
BLINKENLIGHT_API_DIR=../../blinkenlight_api
# *_svc, *_xdr and api.h sources must be created with rpcgen
BLINKENLIGHT_API_SOURCES.c = \
	$(BLINKENLIGHT_API_DIR)/blinkenlight_panels.c	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_api_server_procs.c \
	$(BLINKENLIGHT_API_DIR)/rpcgen_linux/rpc_blinkenlight_api_svc.c \
	$(BLINKENLIGHT_API_DIR)/rpcgen_linux/rpc_blinkenlight_api_xdr.c	\
	$(BLINKENLIGHT_API_DIR)/historybuffer.c	\
	$(BLINKENLIGHT_COMMON_DIR)/bitcalc.c	\
	$(BLINKENLIGHT_COMMON_DIR)/radix.c	\
	$(BLINKENLIGHT_COMMON_DIR)/mcout.c	\
	$(BLINKENLIGHT_COMMON_DIR)/errno2txt.c

BLINKENLIGHT_API_SOURCES.h = \
	$(BLINKENLIGHT_API_DIR)/rpcgen_linux/rpc_blinkenlight_api.h	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_api_server_procs.h \
	$(BLINKENLIGHT_API_DIR)/historybuffer.h	\
	$(BLINKENLIGHT_COMMON_DIR)/bitcalc.h	\
	$(BLINKENLIGHT_COMMON_DIR)/radix.h	\
	$(BLINKENLIGHT_COMMON_DIR)/mcout.h	\
	$(BLINKENLIGHT_COMMON_DIR)/errno2txt.h

TARGETS = $(OBJDIR)/server11 $(OBJDIR)/server8
all:    $(TARGETS)

$(BLINKENLIGHT_API_SOURCES.c) $(BLINKENLIGHT_API_SOURCES.h):
	# rpcgen
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux sources


#########################################################

SOURCES.h = \
	globals.h	\
	gpiopattern.h	\
	print.h	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_panels.h

COMMON_SOURCES.c = \
	$(GPIOSRC) \
	gpiopattern.c	\
	print.c	\
	$(BLINKENLIGHT_API_SOURCES.c)

SERVER11_SOURCES.c = server11.c $(COMMON_SOURCES.c)

SERVER8_SOURCES.c = server8.c $(COMMON_SOURCES.c)

SOURCES.c = server11.c server8.c $(COMMON_SOURCES.c)

OBJECTS = $(SOURCES.c:%.c=%.o)

#
# Build everything (target 'all' is above)
#
clean:
	rm -f core $(OBJECTS) $(TARGETS)
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux clean

install:
	# no-op for now
	# eventually this should copy the binaries and set the capabilities for the panel server

$(OBJDIR)/server11:	$(SERVER11_SOURCES.c) $(SOURCES.h)
ifneq ($(MAKE_TARGET_ARCH),RPI)
	$(error PiDP11 server only builds for MAKE_TARGET_ARCH=RPI)
endif
	${CC} -o $@ $(CCDEFS) $^ ${LDFLAGS}
	file $@

$(OBJDIR)/server8:	$(SERVER8_SOURCES.c) $(SOURCES.h)
ifneq ($(MAKE_TARGET_ARCH),RPI)
	$(error PiDP9 server only builds for MAKE_TARGET_ARCH=RPI)
endif
	${CC} -o $@ $(CCDEFS) $^ ${LDFLAGS}
	file $@
