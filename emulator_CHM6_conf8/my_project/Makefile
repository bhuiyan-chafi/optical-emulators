
######################################################################
# Where is ConfD installed? Make sure CONFD_DIR points it out
CONFD_DIR ?= /opt/confd

# Include standard ConfD build definitions and rules
include $(CONFD_DIR)/src/confd/build/include.mk

# In case CONFD_DIR is not set (correctly), this rule will trigger
$(CONFD_DIR)/src/confd/build/include.mk:
	@echo "CONFD_DIR must be set to the location of the ConfD installation"
	@exit 1

######################################################################
# Example specific definitions and rules

CONFD_FLAGS = --addloadpath $(CONFD_DIR)/etc/confd
START_FLAGS ?=

# Only compiling files in yang/ directory
FXS_DIR = ./confd-cdb
YANG_DIR = ./yang
# Filter out submodules (files not starting with 'module')
YANG_SOURCES = $(shell grep -l "^\s*module" $(YANG_DIR)/*.yang)
FXS_FILES = $(patsubst $(YANG_DIR)/%.yang, $(FXS_DIR)/%.fxs, $(YANG_SOURCES))

all:	$(FXS_FILES) ssh-keydir
	@echo "Build complete"

$(FXS_DIR)/%.fxs: $(YANG_DIR)/%.yang
	$(CONFDC) -c -o $@ --yangpath $(YANG_DIR) -- $(YANG_DIR)/$*.yang

ssh-keydir:
	@mkdir -p $(CONFD_DIR)/var/confd/homes/admin/.ssh
	@if [ ! -f $(CONFD_DIR)/var/confd/homes/admin/.ssh/id_rsa ]; then \
		ssh-keygen -t rsa -f $(CONFD_DIR)/var/confd/homes/admin/.ssh/id_rsa -N "" -q; \
		cat $(CONFD_DIR)/var/confd/homes/admin/.ssh/id_rsa.pub > $(CONFD_DIR)/var/confd/homes/admin/.ssh/authorized_keys; \
	fi

clean:
	rm -rf $(FXS_DIR)/*.fxs 2>/dev/null || true

start:  stop
	$(CONFD) -c etc/confd/confd.conf $(START_FLAGS)

stop:
	$(CONFD) --stop || true

######################################################################
