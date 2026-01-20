#
# CC Command
#

ifeq ($(MAKE_CONFIGURATION),RELEASE)
CC_DBG_FLAGS = -O2 -Wno-unused-result
else
CC_DBG_FLAGS = -ggdb3 -O0
endif

OS_CCDEFS=$(CC_DBG_FLAGS) -I/usr/include/tirpc
LDFLAGS += -ltirpc

CCDEFS= \
	-DBLINKENLIGHT_CLIENT	\
	-I.	\
	-I$(BLINKENLIGHT_COMMON_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)	\
	-I$(BLINKENLIGHT_API_DIR)/rpcgen_linux	\
	$(CC_DBG_FLAGS) $(OS_CCDEFS)

OBJDIR = ../../BIN

#
# Build everything
#
all:    $(OBJDIR)/blinkenlightapitst

#########################################################
# sub dir for the API. is same level (= sibling) as server and client
BLINKENLIGHT_COMMON_DIR=../common
BLINKENLIGHT_API_DIR=../blinkenlight_api

# *_svc, *_xdr and api.h sources must be created with rpcgen
BLINKENLIGHT_API_SOURCES.c = \
	$(BLINKENLIGHT_API_DIR)/blinkenlight_panels.c	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_api_client.c	\
	$(BLINKENLIGHT_API_DIR)/rpcgen_linux/rpc_blinkenlight_api_clnt.c \
	$(BLINKENLIGHT_API_DIR)/rpcgen_linux/rpc_blinkenlight_api_xdr.c	\
	$(BLINKENLIGHT_COMMON_DIR)/radix.c	\
	$(BLINKENLIGHT_COMMON_DIR)/bitcalc.c \
	$(BLINKENLIGHT_COMMON_DIR)/getopt2.c \
	$(BLINKENLIGHT_COMMON_DIR)/kbhit.c	\
	$(BLINKENLIGHT_COMMON_DIR)/mcout.c	\
	$(BLINKENLIGHT_COMMON_DIR)/inputline.c


BLINKENLIGHT_API_SOURCES.h = \
	$(BLINKENLIGHT_API_DIR)/rpcgen_linux/rpc_blinkenlight_api.h	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_api_client.h	\
	$(BLINKENLIGHT_COMMON_DIR)/radix.h	\
	$(BLINKENLIGHT_COMMON_DIR)/bitcalc.h \
	$(BLINKENLIGHT_COMMON_DIR)/getopt2.h \
	$(BLINKENLIGHT_COMMON_DIR)/kbhit.h	\
	$(BLINKENLIGHT_COMMON_DIR)/mcout.h	\
	$(BLINKENLIGHT_COMMON_DIR)/inputline.h


$(BLINKENLIGHT_API_SOURCES.c) $(BLINKENLIGHT_API_SOURCES.h):
	# rpcgen
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux sources

#########################################################
SOURCES.h = \
	main.h	\
	menus.h	\
	actions.h	\
	$(BLINKENLIGHT_API_DIR)/blinkenlight_panels.h

SOURCES.c = \
	main.c	\
	menus.c	\
	actions.c	\
	$(BLINKENLIGHT_API_SOURCES.c)

OBJECTS = $(SOURCES.c:%.c=%.o)

clean:
	rm -f core $(OBJDIR)/blinkenlighttst $(OBJECTS)
	make --directory=$(BLINKENLIGHT_API_DIR)/rpcgen_linux clean


$(OBJDIR)/blinkenlightapitst:	$(SOURCES.c) $(SOURCES.h)
	${CC} -o $@ $(CCDEFS) $(SOURCES.c) ${LDFLAGS}
