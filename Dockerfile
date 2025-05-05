FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip \
    git \
    build-essential \
    libudev1 \
    libusb-1.0-0 \
    wget curl \
    && apt-get install -y python3-venv \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/* \
    && npm install -g npm@latest \
    && pip3 install platformio
    
ARG USERNAME=vscode
RUN usermod -aG dialout ${USERNAME} || true