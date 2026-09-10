ARG BUILD_FROM=ghcr.io/hassio-addons/base:16.3.2
FROM $BUILD_FROM

LABEL org.opencontainers.image.title="ThinClock"
LABEL org.opencontainers.image.description="Config server for ThinClock ESP32 LED matrix displays"
LABEL org.opencontainers.image.source="https://github.com/techninja/thinclock"
LABEL org.opencontainers.image.licenses="MIT"

RUN apk add --no-cache nodejs npm

WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci --omit=dev

COPY src/ ./src/

RUN ln -s /data ./data

COPY run.sh /run.sh
RUN chmod +x /run.sh

CMD ["/run.sh"]
