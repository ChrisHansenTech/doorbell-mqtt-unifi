#!/bin/sh
set -eu

PUID="${PUID:-10001}"
PGID="${PGID:-10001}"

APP_USER="appuser"
APP_GROUP="appgroup"

# Create group/user if needed
if ! getent group "$APP_GROUP" >/dev/null 2>&1; then
  addgroup --gid "$PGID" "$APP_GROUP" >/dev/null 2>&1 || true
fi

if ! id "$APP_USER" >/dev/null 2>&1; then
  adduser --disabled-password --gecos "" --uid "$PUID" --gid "$PGID" "$APP_USER" >/dev/null 2>&1 || true
fi

# Ensure dirs exist
mkdir -p /config /profiles /tmp/doorbell-mqtt-unifi

# Fix ownership (only on dirs we expect to be writable)
chown -R "$PUID:$PGID" /config /profiles /tmp/doorbell-mqtt-unifi || true

if [ ! -f /config/config.json ]; then
  echo "[INFO] No config.json found, copying default"
  cp /defaults/config.json /config/config.json
  chown "$PUID:$PGID" /config/config.json || true
fi

# Drop privileges and exec
exec gosu "$PUID:$PGID" /app/doorbell-mqtt-unifi