
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip \
    nodejs npm \
    git \
    build-essential \
    wget curl \
    && rm -rf /var/lib/apt/lists/*

ddd