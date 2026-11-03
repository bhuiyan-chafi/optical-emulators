FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    openssh-client \
    libssl-dev \
    python3 \
    python3-pip \
    build-essential \
    make \
    iputils-ping \
    net-tools \
    nano \
    vim \
    unzip \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install libssl1.1 (required for ConfD 8.0 bundle)
RUN wget http://nz2.archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    dpkg -i libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    rm libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb

# Copy ConfD installer package into build context
COPY confd-basic-8.0.20.linux.x86_64.signed.zip /tmp/confd.zip

# Unzip and install ConfD into /opt/confd, then rebuild libconfd & tools with MAXDEPTH=40
# (OpenROADM v18.1.0 YANG hierarchy exceeds ConfD's prebuilt default MAXDEPTH=20)
RUN cd /tmp && \
    unzip confd.zip && \
    chmod +x confd-basic-8.0.20.linux.x86_64.signed.bin && \
    ./confd-basic-8.0.20.linux.x86_64.signed.bin && \
    chmod +x confd-basic-8.0.20.linux.x86_64.installer.bin && \
    ./confd-basic-8.0.20.linux.x86_64.installer.bin /opt/confd && \
    tar -xzf confd-basic-8.0.20.libconfd.tar.gz && \
    cd /tmp/confd-basic-8.0.20/libconfd && \
    make -j"$(nproc)" CONFD_DIR=/opt/confd EXTRA_CFLAGS="-DMAXDEPTH=40 -DMAXKEYLEN=16" all install && \
    cd /opt/confd/src/confd/tools && \
    make -f Makefile.confd_cmd CONFD_DIR=/opt/confd CFLAGS="-Wall -O2 -DMAXDEPTH=40 -DMAXKEYLEN=16 -I../ipc_drv -I/opt/confd/include" clean all && \
    cp confd_load confd_cmd /opt/confd/bin/ && \
    rm -rf /tmp/*

ENV CONFD_DIR=/opt/confd
ENV PATH=$CONFD_DIR/bin:$PATH
ENV LD_LIBRARY_PATH=$CONFD_DIR/lib

# Create project directory structure
RUN mkdir -p /my_project/confd-cdb /my_project/etc/confd

# Bake in all 80 pre-compiled OpenROADM v18.1.0 FXS binaries, confd.conf, initial XML, and scripts
COPY build_context/confd-cdb/*.fxs /my_project/confd-cdb/
COPY my_project/etc/confd/confd.conf /my_project/etc/confd/confd.conf
COPY my_project/init_pss32_config.xml /my_project/init_pss32_config.xml
COPY my_project/load_config.sh /my_project/load_config.sh
COPY entrypoint.sh /entrypoint.sh

# Make scripts executable and create startNetconfAgent.sh symlinks for TFS drop-in compatibility
RUN chmod +x /entrypoint.sh /my_project/load_config.sh && \
    ln -s /entrypoint.sh /startNetconfAgent.sh && \
    ln -s /entrypoint.sh /my_project/startNetconfAgent.sh

WORKDIR /my_project

ENTRYPOINT ["/entrypoint.sh"]
