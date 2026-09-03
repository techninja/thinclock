ARG BUILD_FROM=ghcr.io/hassio-addons/base:16.3.2
FROM $BUILD_FROM

# Install Node.js 22
RUN apk add --no-cache nodejs npm

WORKDIR /app

# Copy package files and install production deps only
COPY package.json package-lock.json ./
RUN npm ci --omit=dev

# Copy server source
COPY src/ ./src/

# Persistent data dir (HA mounts /data)
RUN mkdir -p /data/preview-cache /data/custom-screens

# Symlink data dir into app so server finds it at ./data/
RUN ln -s /data ./data

COPY run.sh /run.sh
RUN chmod +x /run.sh

CMD ["/run.sh"]
