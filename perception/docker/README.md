Steps to build g2f-perception:

1. docker buildx build -t frgentile/g2f:g2f-perception -f docker/Dockerfile .

docker run -it --rm --gpus all -v tracking-dev-vol:/opt/storage -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY --net=host frgentile/g2f:g2f-perception /bin/bash
