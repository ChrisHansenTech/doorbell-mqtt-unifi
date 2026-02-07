# Build stage
FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    pkg-config \
    libssl-dev \
    libssh2-1-dev \
    libpaho-mqtt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY src/ ./src/
COPY include/ ./include/
COPY Makefile ./

RUN make clean && make

# Final stage
FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    libssl3 \
    libssh2-1 \
    libpaho-mqtt1.3 \
    ca-certificates \
    gosu \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/bin/doorbell-mqtt-unifi /app/doorbell-mqtt-unifi

COPY config.example.json /defaults/config.json

COPY test-profile/ /app/test-profile/

COPY docker-entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

VOLUME ["/config", "/profiles"]

ENTRYPOINT ["/entrypoint.sh"]