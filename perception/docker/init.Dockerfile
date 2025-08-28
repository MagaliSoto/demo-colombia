# syntax=docker/dockerfile:1
FROM frgentile/g2f:ds64-base

LABEL mantainer="Francisco GENTILE <francisco@go2future.com>"
LABEL platform="Linux x86_64"
LABEL description="Deepstream 6.4 with TAO Apps and Ultralytics for deployment"

ARG DEBIAN_FRONTEND=nointeractive

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# To get video driver libraries at runtime (libnvidia-encode.so/libnvcuvid.so)
ENV NVIDIA_DRIVER_CAPABILITIES=$NVIDIA_DRIVER_CAPABILITIES,video

WORKDIR /app

# Copy Custom Parser
COPY --chown=developer:developer ./custom-parser /app/custom-parser

# Copy Scripts
COPY --chown=developer:developer ./scripts /app/scripts

