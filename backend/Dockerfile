# syntax=docker/dockerfile:1
FROM ubuntu:22.04 AS base
LABEL author="Magali Soto <magali@go2future.com>"
LABEL description="Backend alertas ROI"

# Zona horaria
ENV TZ=America/Argentina/Buenos_Aires
RUN rm -rf /etc/localtime && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime

# Dependencias del sistema mínimas
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv \
    git sudo netcat \
    && rm -rf /var/lib/apt/lists/*

# Crear usuario no-root
RUN useradd -G sudo --create-home --system --shell /bin/bash g2f && \
    echo "g2f ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/g2f && \
    chown -R g2f:g2f /home/g2f && \
    echo "export PATH=/home/g2f/.local/bin:$PATH" >> /home/g2f/.bashrc

RUN mkdir -p /app && chown g2f:g2f -R /app

# ------------------------------------------------------------------------------
# Stage: Deploy
# ------------------------------------------------------------------------------
FROM base AS deploy

WORKDIR /app
USER g2f

# Copiar la aplicación
COPY --chown=g2f:g2f . .

# Instalar dependencias de Python
RUN pip3 install --no-cache-dir -r requirements.txt

# Comando por defecto
CMD ["python3", "main.py"]