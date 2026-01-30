MAKEFLAGS=-s

# output directory
OBJDIR = ../BIN

PDP11RC = pdp11_realcons

CLIENT11DIR = ..
SERVER11DIR = pidp_server/server
GETCSWDIR = blinkenlight_getcsw
BLTESTDIR = blinkenlight_test
BLAPIDIR = blinkenlight_api/rpcgen_linux
PANELSIMDIR = javapanelsim

CLIENT11 = $(OBJDIR)/client11
SERVER11 = $(OBJDIR)/server11
GETCSW = $(OBJDIR)/getcsw
BLTEST = $(OBJDIR)/blinkenlightapitst
PANELSIM = $(PANELSIMDIR)/panelsim_all.jar

ALL = $(CLIENT11) $(GETCSW) $(BLTEST) $(PANELSIM)
ifneq (,$(wildcard /usr/bin/raspi-config))
  ALL += $(SERVER11)
endif

all:	$(OBJDIR) $(ALL)
	@echo All done!

$(OBJDIR):
	-mkdir -p $(OBJDIR)

$(CLIENT11): FRC
	@echo $@ ...
	$(MAKE) -C $(BLAPIDIR) sources
	MAKELEVEL=0 $(MAKE) -C $(CLIENT11DIR) $(PDP11RC)
	cp $(OBJDIR)/$(PDP11RC) $(CLIENT11)

$(GETCSW): FRC
	@echo $@ ...
	$(MAKE) -C $(GETCSWDIR)

$(BLTEST): FRC
	@echo $@ ...
	$(MAKE) -C $(BLTESTDIR)

$(SERVER11): FRC
	@echo $@ ...
	$(MAKE) -C $(SERVER11DIR)

# this doesn't change often, so only build it if it is missing
$(PANELSIM):
	@echo $@ ...
	cd $PANELSIMDIR ; ant -f build.xml compile jar

# force all - because the dependencies are evaluated in the sub-makes
FRC:

clean:
	$(MAKE) -C $(BLAPIDIR) clean
	MAKELEVEL=0 $(MAKE) -C $(CLIENT11DIR) clean
	$(MAKE) -C $(GETCSWDIR) clean
	$(MAKE) -C $(BLTESTDIR) clean
	$(MAKE) -C $(SERVER11DIR) clean
