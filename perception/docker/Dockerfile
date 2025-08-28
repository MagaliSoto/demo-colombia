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

# Copy App
COPY --chown=developer:developer . /app

# Move setup script to entrypoint.d
RUN mv /app/scripts/80-g2f-check.sh /opt/nvidia/entrypoint.d/80-g2f-check.sh

ENTRYPOINT [ "/opt/nvidia/nvidia_entrypoint.sh" ]
CMD ["$@"]
