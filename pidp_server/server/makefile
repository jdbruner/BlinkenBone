#
# Platform check - this only builds on Raspberry Pi
#
ifeq (,$(wildcard /usr/bin/raspi-config))
  $(error panel servers only build on Raspberry Pi)
endif

#
# Build panel servers for PiDP11, PiDP10, and PiDP8
#
OBJDIR=../../../BIN
TARGETS = $(OBJDIR)/server11 $(OBJDIR)/server10 $(OBJDIR)/server8
all:    $(TARGETS)

#
# CC Command
#
ifeq ($(MAKE_CONFIGURATION),RELEASE)
CC_DBG_FLAGS = -O2 -Wno-unused-result
else
CC_DBG_FLAGS = -ggdb3 -O0
endif
OS_CCDEFS = -I/usr/include/tirpc
LDFLAGS = -lrt -pthread -ltirpc

#
# Use the appropriate GPIO interface
# PiDP8 and PiDP11 use direct selection of rows and LED rows
# PiDP10 uses encoded selection
#
# If USE_LIBGPIOD is non-empty, use libgpiod; otherwise use pinctrl/gpiolib
# When using libgpiod, use the version-specific code (because unfortunately
# there were significant breaking changes between v1 and v2)
#
ifneq (,$(USE_LIBGPIOD))
	# determine version of libgpiod (breaking change between v1 and v2)
	ifneq (,$(shell apt-cache show libgpiod-dev | grep '^Version: 1\.'))
		GPIOSELSRC = gpio_sel_libgpiod_v1.c
		GPIOENCSRC = gpio_enc_libgpiod_v1.c
	else
		GPIOSELSRC = gpio_sel_libgpiod_v2.c
		GPIOENCSRC = gpio_enc_libgpiod_v2.c
	endif
	LDFLAGS += -lgpiod
else
	# use pinctrl/gpiolib
	GPIOSELSRC = gpio_sel.c
	GPIOENCSRC = gpio_enc.c
	LDFLAGS += -lgpiolib
endif

#
# C compiler flags
#
CCDEFS= \
	-DBLINKENLIGHT_SERVER	\
	-I.	\
	-I$(BLINKENLIGHT_COMMON_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)/rpcgen_linux	\
	$(CC_DBG_FLAGS) $(OS_CCDEFS)

#
# Blinkenlight interface sources
#
BLINKENLIGHT_COMMON_DIR=../../common
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

$(BLINKENLIGHT_API_SOURCES.c) $(BLINKENLIGHT_API_SOURCES.h):
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux sources


#########################################################

INCLUDES = \
	globals.h	\
	gpiopattern.h	\
	print.h	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_panels.h \
	$(BLINKENLIGHT_API_SOURCES.h)

COMMON_SOURCES.c = \
	gpiopattern.c	\
	print.c	\
	server_common.c \
	$(BLINKENLIGHT_API_SOURCES.c)

SERVER11_SOURCES.c = server11.c $(GPIOSELSRC) $(COMMON_SOURCES.c)

SERVER10_SOURCES.c = server10.c $(GPIOENCSRC) $(COMMON_SOURCES.c)

SERVER8_SOURCES.c = server8.c $(GPIOSELSRC) $(COMMON_SOURCES.c)

# default target "all" is above

clean:
	rm -f core *.o $(TARGETS)
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux clean

install:
	# no-op for now
	# eventually this should copy the binaries and set the capabilities for the panel server

$(OBJDIR)/server11:	$(SERVER11_SOURCES.c) $(INCLUDES)
	${CC} -o $@ $(CCDEFS) $(SERVER11_SOURCES.c) ${LDFLAGS}

$(OBJDIR)/server10:	$(SERVER10_SOURCES.c) $(INCLUDES)
	${CC} -o $@ $(CCDEFS) $(SERVER10_SOURCES.c) ${LDFLAGS}

$(OBJDIR)/server8:	$(SERVER8_SOURCES.c) $(INCLUDES)
	${CC} -o $@ $(CCDEFS) $(SERVER8_SOURCES.c) ${LDFLAGS}
