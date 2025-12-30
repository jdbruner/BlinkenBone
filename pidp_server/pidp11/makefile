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

# Reference also the standard blinkenbone server in BLINKENLIGHT_SERVER_DIR

CCDEFS= \
	-DBLINKENLIGHT_SERVER	\
	-I.	\
	-I$(BLINKENLIGHT_COMMON_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)/rpcgen_linux	\
	-I$(BLINKENLIGHT_SERVER_DIR)	\
	$(CC_DBG_FLAGS) $(OS_CCDEFS)


BLINKENLIGHT_COMMON_DIR=../../common
# sub dir for the API. is same level (= sibling) as server and client
BLINKENLIGHT_API_DIR=../../blinkenlight_api
# dir of standard server
BLINKENLIGHT_SERVER_DIR=../../blinkenlight_server
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

all:    $(OBJDIR)/server11

$(BLINKENLIGHT_API_SOURCES.c) $(BLINKENLIGHT_API_SOURCES.h):
	# rpcgen
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux sources


#########################################################

SOURCES.h = \
	main.h	\
	gpiopattern.h	\
	$(BLINKENLIGHT_SERVER_DIR)/print.h	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_panels.h

SOURCES.c = \
	main.c	\
	$(GPIOSRC) \
	gpiopattern.c	\
	$(BLINKENLIGHT_SERVER_DIR)/print.c	\
	$(BLINKENLIGHT_API_SOURCES.c)


OBJECTS = $(SOURCES.c:%.c=%.o)

#
# Build everything (target 'all' is above)
#
clean:
	echo "CLEAN"
	rm -f core $(OBJECTS) $(OBJDIR)/server11 $(OBJDIR)/scansw
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux clean

install:
	# no-op for now
	# eventually this should copy the binaries and set the capabilities for the panel server


$(OBJDIR)/server11:	$(SOURCES.c) $(SOURCES.h)
ifneq ($(MAKE_TARGET_ARCH),RPI)
	$(error PiDP11 server only builds for MAKE_TARGET_ARCH=RPI)
endif
	${CC} -o $@ $(CCDEFS) $^ ${LDFLAGS}
	file $@
