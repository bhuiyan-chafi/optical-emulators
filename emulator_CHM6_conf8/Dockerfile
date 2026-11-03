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

# Unzip and install ConfD into /opt/confd
RUN cd /tmp && \
    unzip confd.zip && \
    chmod +x confd-basic-8.0.20.linux.x86_64.signed.bin && \
    ./confd-basic-8.0.20.linux.x86_64.signed.bin && \
    chmod +x confd-basic-8.0.20.linux.x86_64.installer.bin && \
    ./confd-basic-8.0.20.linux.x86_64.installer.bin /opt/confd && \
    rm -rf /tmp/*

ENV CONFD_DIR=/opt/confd
ENV PATH=$CONFD_DIR/bin:$PATH
ENV LD_LIBRARY_PATH=$CONFD_DIR/lib

# Create project directory structure
RUN mkdir -p /my_project/confd-cdb /my_project/etc/confd

# Bake in all 21 compiled FXS binaries, confd.conf, Day-0 XML, and scripts
COPY build_context/confd-cdb/*.fxs /my_project/confd-cdb/
COPY my_project/etc/confd/confd.conf /my_project/etc/confd/confd.conf
COPY my_project/init_chm6_config.xml /my_project/init_chm6_config.xml
COPY my_project/load_config.sh /my_project/load_config.sh
COPY entrypoint.sh /entrypoint.sh

# Make scripts executable and create startNetconfAgent.sh symlinks for TFS drop-in compatibility
RUN chmod +x /entrypoint.sh /my_project/load_config.sh && \
    ln -s /entrypoint.sh /startNetconfAgent.sh && \
    ln -s /entrypoint.sh /my_project/startNetconfAgent.sh

WORKDIR /my_project

ENTRYPOINT ["/entrypoint.sh"]
