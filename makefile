MAKEFLAGS=-s

# output directory
OBJDIR = ../BIN

# Blinkenlight API
BLAPIDIR = blinkenlight_api/rpcgen_linux
BLAPI = $(BLAPIDIR)/rpc_blinkenlight_api_svc.c \
	$(BLAPIDIR)/rpc_blinkenlight_api_clnt.c \
	$(BLAPIDIR)/rpc_blinkenlight_api.x \
	$(BLAPIDIR)/rpc_blinkenlight_api.h \
	$(BLAPIDIR)/rpc_blinkenlight_api_xdr.c

# Simh targets for PDP11 and PDP10
# Vanilla and REALCONS are cross-platform
# PIPANEL are only meaningful on Raspberry Pi
SIMHDIR = ..
PDP11_TARGETS = pdp11 pdp11_realcons
PDP10_TARGETS = pdp10-ka pdp10-ki pdp10-kl pdp10-ks \
	pdp10-ka_realcons pdp10-ki_realcons pdp10-kl_realcons pdp10-ks_realcons
PDP10PANEL_TARGETS = pdp10-ka_pipanel pdp10-ki_pipanel \
	pdp10-kl_pipanel pdp10-ks_pipanel

# Java panel simulator
PANELSIMDIR = javapanelsim
PANELSIM = $(PANELSIMDIR)/panelsim_all.jar

# REALCONS servers
SERVERDIR = pidp_server/server
SERVER10 = $(OBJDIR)/server10
SERVER11 = $(OBJDIR)/server11

# REALCONS-based getcsw (get console switches)
GETCSWDIR = getcsw
GETCSW = $(OBJDIR)/getcsw

# Non-REALCONS scansw (get console switches)
SCANSWDIR = scansw
SCANSW00 = $(OBJDIR)/scansw00
SCANSW10 = $(OBJDIR)/scansw10

# Blinkenlight test app
BLTESTDIR = blinkenlight_test
BLTEST = $(OBJDIR)/blinkenlightapitst

COMMON = $(GETCSW) $(BLTEST) $(PANELSIM) $(SCANSW00)

PIDP11 = $(PDP11_TARGETS)
PIDP10 = $(PDP10_TARGETS)

ifneq (,$(wildcard /usr/bin/raspi-config))
  PIDP11 += $(SERVER11)
  PIDP10 += $(PDP10PANEL_TARGETS) $(SERVER10) $(SCANSW10)
  PDP10_TARGETS += $(PDP10PANEL_TARGETS)
endif

all:	pidp10 pidp11
	@echo All done!

pidp11:	$(OBJDIR) $(PIDP11) $(COMMON)

$(PDP10_TARGETS) $(PDP11_TARGETS): $(BLAPI)
	@echo $@ ...
	MAKELEVEL=0 $(MAKE) -C $(SIMHDIR) $@

pidp10:	$(OBJDIR) $(PIDP10) $(COMMON)

$(BLAPI):
	@echo Blinkenlight API
	$(MAKE) -C $(BLAPIDIR) sources
	
$(OBJDIR):
	-mkdir -p $(OBJDIR)

$(GETCSW): FRC
	@echo $@ ...
	$(MAKE) -C $(GETCSWDIR)

$(BLTEST): FRC
	@echo $@ ...
	$(MAKE) -C $(BLTESTDIR)

$(SERVER11) $(SERVER10):	FRC
	@echo $@ ...
	$(MAKE) -C $(SERVERDIR)

$(SCANSW00) $(SCANSW10):
	@echo $@
	$(MAKE) -C $(SCANSWDIR)

# this doesn't change often, so only build it if it is missing
$(PANELSIM):
	@echo $@ ...
	cd $(PANELSIMDIR) ; ant -f build.xml compile jar

# force all - because the dependencies are evaluated in the sub-makes
FRC:

clean:
	$(MAKE) -C $(BLAPIDIR) clean
	MAKELEVEL=0 $(MAKE) -C $(SIMHDIR) clean
	$(MAKE) -C $(GETCSWDIR) clean
	$(MAKE) -C $(BLTESTDIR) clean
	$(MAKE) -C $(SERVERDIR) clean
