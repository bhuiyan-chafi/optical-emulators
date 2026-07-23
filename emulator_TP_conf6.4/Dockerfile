FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies including python3-pip
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
    libncurses5 \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install libssl1.1 and libssl1.0.0 manually (required for older ConfD)
RUN wget http://nz2.archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    dpkg -i libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    rm libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    wget http://security.ubuntu.com/ubuntu/pool/main/o/openssl1.0/libssl1.0.0_1.0.2n-1ubuntu5.13_amd64.deb && \
    dpkg -i libssl1.0.0_1.0.2n-1ubuntu5.13_amd64.deb && \
    rm libssl1.0.0_1.0.2n-1ubuntu5.13_amd64.deb

# Copy installer (assuming it is in the build context)
COPY ../repos/confd-basic-6.4.linux.x86_64/confd-basic-6.4.linux.x86_64.installer.bin /tmp/installer.bin

# Install ConfD
# The installer requires the destination directory to be provided
RUN chmod +x /tmp/installer.bin && \
    /tmp/installer.bin /opt/confd

# Setup Environment Variables
ENV CONFD_DIR=/opt/confd
ENV PATH=$CONFD_DIR/bin:$PATH
ENV LD_LIBRARY_PATH=$CONFD_DIR/lib:$LD_LIBRARY_PATH

# Cleanup
RUN rm /tmp/installer.bin

WORKDIR /my_project

# Expose NETCONF (2024 for our usage, standard is 830) and IPC (4565)
EXPOSE 2024 4565

CMD ["confd", "--foreground", "--verbose"]
