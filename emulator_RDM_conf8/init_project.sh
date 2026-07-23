#!/bin/bash
# Initialize the project directory structure if it doesn't exist
# This mimics 'confd --create' but manually

PROJECT_DIR="emulator_RDM_conf8/my_project"

mkdir -p "$PROJECT_DIR/yang"
mkdir -p "$PROJECT_DIR/etc/confd"
mkdir -p "$PROJECT_DIR/confd-cdb"

# Create a Makefile (Generic for ConfD)
cat > "$PROJECT_DIR/Makefile" << 'EOF'
all:
	@echo "Compiling all YANG modules in 'yang' directory..."
	@# Only compile files that start with 'module' to avoid compiling submodules directly
	@for f in yang/*.yang; do \
		if grep -q "^module " "$$f"; then \
			echo "Compiling $$f"; \
			confdc -c -o confd-cdb/$$(basename $$f .yang).fxs --yangpath yang $$f; \
		else \
			echo "Skipping submodule $$f"; \
		fi \
	done
	@echo "Generating SSH keys..."
	@ssh-keygen -m PEM -t rsa -f etc/confd/ssh/ssh_host_rsa_key -N "" || true

clean:
	rm -f confd-cdb/*.fxs
EOF

# Create confd.conf (Patched for ConfD 8.0 Basic)
cat > "$PROJECT_DIR/etc/confd/confd.conf" << 'EOF'
<confdConfig xmlns="http://tail-f.com/ns/confd_cfg/1.0">
  <loadPath>
    <dir>/opt/confd/etc/confd</dir>
    <dir>/my_project/confd-cdb</dir>
  </loadPath>

  <stateDir>/opt/confd/var/confd</stateDir>

  <cdb>
    <enabled>true</enabled>
    <dbDir>/opt/confd/var/confd/cdb</dbDir>
    <operational>
      <enabled>true</enabled>
    </operational>
  </cdb>

  <logs>
    <confdLog>
      <enabled>true</enabled>
      <file>
        <enabled>false</enabled>
        <name>/opt/confd/var/confd/log/confd.log</name>
      </file>
      <syslog>
        <enabled>false</enabled>
      </syslog>
    </confdLog>
    <developerLog>
      <enabled>true</enabled>
      <file>
          <enabled>false</enabled>
           <name>/opt/confd/var/confd/log/devel.log</name>
      </file>
    </developerLog>
  </logs>

  <datastores>
    <startup>
      <enabled>false</enabled>
    </startup>
    <candidate>
      <enabled>true</enabled>
      <implementation>confd</implementation>
      <storage>auto</storage>
      <filename>/opt/confd/var/confd/candidate/candidate.db</filename>
    </candidate>
    <running>
      <access>read-write</access>
    </running>
  </datastores>

  <netconf>
    <enabled>true</enabled>
    <transport>
      <ssh>
        <enabled>true</enabled>
        <ip>0.0.0.0</ip>
        <port>2022</port>
      </ssh>
      <tcp>
        <enabled>true</enabled>
        <ip>127.0.0.1</ip>
        <port>2023</port>
      </tcp>
    </transport>
    <capabilities>
      <startup>
        <enabled>false</enabled>
      </startup>
      <candidate>
        <enabled>true</enabled>
      </candidate>
      <confirmed-commit>
        <enabled>true</enabled>
      </confirmed-commit>
      <writable-running>
        <enabled>true</enabled>
      </writable-running>
      <rollback-on-error>
        <enabled>true</enabled>
      </rollback-on-error>
    </capabilities>
  </netconf>

  <cli>
    <enabled>true</enabled>
    <ssh>
        <enabled>true</enabled>
        <port>2024</port>
    </ssh>
  </cli>

  <webui>
    <enabled>false</enabled>
  </webui>

  <snmpAgent>
    <enabled>false</enabled>
  </snmpAgent>

  <aaa>
    <sshServerKeyDir>/opt/confd/etc/confd/ssh</sshServerKeyDir>

    <aaaBridge>
      <enabled>false</enabled>
      <file>/opt/confd/etc/confd/aaa.conf</file>
    </aaaBridge>

    <pam>
      <enabled>false</enabled>
      <service>system-auth</service>
    </pam>

    <localAuthentication>
      <enabled>true</enabled>
    </localAuthentication>
  </aaa>

  <rollback>
    <enabled>true</enabled>
    <directory>/opt/confd/var/confd/rollback</directory>
    <historySize>50</historySize>
  </rollback>
</confdConfig>
EOF
