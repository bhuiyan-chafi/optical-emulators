
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies (standard modern Ubuntu libraries)
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

# Install libssl1.1 manually (required for ConfD 8.0 bundle)
RUN wget http://nz2.archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    dpkg -i libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb && \
    rm libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb

# Copy the zip package
COPY confd-basic-8.0.20.linux.x86_64.signed.zip /tmp/confd.zip

# Unzip and install
# The signed bin extracts the installer to the current directory
RUN cd /tmp && \
    unzip confd.zip && \
    chmod +x confd-basic-8.0.20.linux.x86_64.signed.bin && \
    ./confd-basic-8.0.20.linux.x86_64.signed.bin && \
    chmod +x confd-basic-8.0.20.linux.x86_64.installer.bin && \
    ./confd-basic-8.0.20.linux.x86_64.installer.bin /opt/confd && \
    rm -rf /tmp/*

ENV CONFD_DIR=/opt/confd
ENV PATH=$CONFD_DIR/bin:$PATH
ENV LD_LIBRARY_PATH=$CONFD_DIR/lib:$LD_LIBRARY_PATH

WORKDIR /my_project

EXPOSE 2024 4565

CMD ["confd", "--foreground", "--verbose"]
