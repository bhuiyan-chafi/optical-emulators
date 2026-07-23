######################################################################
# NETCONF Notification example
# (C) 2006-2009 Tail-f Systems
#
# See the README file for more information
######################################################################

usage:
	@echo "See README file for more instructions"
	@echo "make all      Build all example files"
	@echo "make clean    Remove all built and intermediary files"
	@echo "make start    Start ConfD daemon and example notifier app using the builtin"
	@echo "              replay store"
	@echo "make start2   Start ConfD daemon and example notifier app using an external"
	@echo "              replay store"
	@echo "make stop     Stop any ConfD daemon and example notifier app"
	@echo "make query    Run NETCONF query against ConfD"
	@echo "make cli     Start the CONFD Command Line Interface"
	@echo "make cli-c   Start the CONFD Command Line Interface, C-style"
	@echo "make cli-j   Start the CONFD Command Line Interface, J-style"

######################################################################
# Where is ConfD installed? Make sure CONFD_DIR points it out
CONFD_DIR ?= ../../..

# Include standard ConfD build definitions and rules
include $(CONFD_DIR)/src/confd/build/include.mk

# In case CONFD_DIR is not set (correctly), this rule will trigger
$(CONFD_DIR)/src/confd/build/include.mk:
	@echo 'Where is ConfD installed? Set $$CONFD_DIR to point it out!'
	@echo ''

######################################################################
# Example specific definitions and rules
CONFD_FLAGS = --addloadpath $(CONFD_DIR)/etc/confd
START_FLAGS ?=


CFLAGS	+= $(EXPAT_INC)
LIBS	+= $(EXPAT_LIB)

HFXS = flex-scale-mg-on.h

#SRC = transponder_driver.c

SRC =

OBJ = $(SRC:%.c=%.o)


all: notifier flex-scale-mg-on.fxs openconfig-wavelength-router.fxs  openconfig-types.fxs openconfig-transport-types.fxs openconfig-platform-types.fxs \
	openconfig-yang-types.fxs openconfig-platform.fxs openconfig-alarm-types.fxs openconfig-interfaces.fxs ietf-yang-types.fxs \
	ietf-interfaces.fxs openconfig-if-ethernet.fxs openconfig-telemetry.fxs openconfig-telemetry-types.fxs openconfig-inet-types.fxs \
       	openconfig-platform-transceiver.fxs openconfig-platform-port.fxs openconfig-platform-transceiver.fxs  $(CDB_DIR) ssh-keydir

notifier: notifier.c flex-scale-mg-on.h openconfig-wavelength-router.h openconfig-types.h openconfig-transport-types.h openconfig-platform-types.h \
	openconfig-platform-transceiver.h openconfig-yang-types.h openconfig-platform.h openconfig-alarm-types.h openconfig-interfaces.h \
	ietf-yang-types.h ietf-interfaces.h openconfig-if-ethernet.h openconfig-telemetry.h openconfig-telemetry-types.h openconfig-inet-types.h \
        openconfig-platform-port.h openconfig-platform-transceiver.h  $(OaBJ)
	$(CC) notifier.c flex-scale-mg-on.h openconfig-wavelength-router.h openconfig-types.h openconfig-transport-types.h openconfig-platform-types.h \
	openconfig-platform-transceiver.h openconfig-yang-types.h openconfig-platform.h openconfig-alarm-types.h openconfig-interfaces.h \
	ietf-yang-types.h ietf-interfaces.h openconfig-if-ethernet.h openconfig-telemetry.h openconfig-telemetry-types.h openconfig-inet-types.h \
        openconfig-platform-port.h openconfig-platform-transceiver.h  $(OBJ) $(LIBS) $(CFLAGS) -o $@


#all: notifier flex-scale-transceiver.fxs openconfig-terminal-device.fxs openconfig-telemetry.fxs openconfig-telemetry-types.fxs \
	openconfig-if-ethernet.fxs openconfig-interfaces.fxs ietf-interfaces.fxs openconfig-transport-types.fxs \
	openconfig-platform-types.fxs $(CDB_DIR) ssh-keydir 

#notifier: notifier.c flex-scale-transceiver.h openconfig-terminal-device.h openconfig-telemetry.h openconfig-telemetry-types.h \
	openconfig-if-ethernet.h openconfig-interfaces.h ietf-interfaces.h openconfig-transport-types.h \
	openconfig-platform-types.h $(aOBJ)
#	$(CC) notifier.c transponder_driver.c flex-scale-transceiver.h openconfig-terminal-device.h openconfig-telemetry.h openconfig-telemetry-types.h \
		openconfig-if-ethernet.h openconfig-interfaces.h ietf-interfaces.h openconfig-transport-types.h \
		openconfig-platform-types.h $(OBJ) $(LIBS) $(CFLAGS) -o $@


%.o: %.c
	@echo 'Building file: $<'
	$(CC) -c $< $(CFLAGS) -o $@
	@echo 'Finished building: $<'
	@echo ' '

%.h: %.fxs
	$(CONFDC) --emit-h $*.h $<

######################################################################
init:
	@echo 'Initialize database'
	cp init_openconfig-platform.xml confd-cdb/

clean:	iclean
	-rm -rf notifier notifier_builtin_replay_store $(HFXS) interface.*\
		confd_prim.conf 2> /dev/null || true

start:  stop 
	sed 's/[ \t]*<!-- enableBuiltinReplayStore -->/          <enabled>true<\/enabled>/' confd.conf > confd_prim.conf
	$(CONFD) -c ./confd_prim.conf $(CONFD_FLAGS)
	LD_LIBRARY_PATH=$(CONFD_SO) ./notifier_builtin_replay_store -t

startw:  stop 
	sed 's/[ \t]*<!-- enableBuiltinReplayStore -->/          <enabled>true<\/enabled>/' confd.conf > confd_prim.conf
	$(CONFD) -c ./confd_prim.conf $(CONFD_FLAGS) -w
	LD_LIBRARY_PATH=$(CONFD_SO) ./notifier_builtin_replay_store -t

start2:  stop
	sed 's/[ \t]*<!-- enableBuiltinReplayStore -->/          <enabled>false<\/enabled>/' confd.conf > confd_prim.conf
	$(CONFD) -c ./confd_prim.conf $(CONFD_FLAGS)
	LD_LIBRARY_PATH=$(CONFD_SO) ./notifier -t

######################################################################
stop:
	### Stopping any confd daemon
	$(CONFD) --stop || true

######################################################################
query:
	$(CONFD_DIR)/bin/netconf-console-tcp --get -x netconf

subscribe:
	$(CONFD_DIR)/bin/netconf-console-tcp -s all sub.xml

replay:
	$(CONFD_DIR)/bin/netconf-console-tcp -s all replay.xml

subscribe-netconf:
	$(CONFD_DIR)/bin/netconf-console-tcp --create-subscription=NETCONF

edit-config1:
	$(CONFD_DIR)/bin/netconf-console-tcp --edit-config=edit1.xml

edit-config2:
	$(CONFD_DIR)/bin/netconf-console-tcp --edit-config=edit2.xml

rpc-outage:
	$(CONFD_DIR)/bin/netconf-console-tcp rpc-outage2.xml

######################################################################

cli:
	$(CONFD_DIR)/bin/confd_cli --user=admin --groups=admin \
		--interactive || echo Exit

cli-c:
	$(CONFD_DIR)/bin/confd_cli -C --user=admin --groups=admin \
		--interactive || echo Exit

cli-j:
	$(CONFD_DIR)/bin/confd_cli -J --user=admin --groups=admin \
		--interactive || echo Exit
