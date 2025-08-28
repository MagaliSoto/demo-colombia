#!/bin/bash
set -e

# Get current working dir
DATA_DIR=`pwd`

# Check number of parameters
if [[ $# -eq 1 ]]; then
    DATA_DIR=$1
fi

echo 
echo -e "\e[33;1m[INFO]\e[0m Downloading models in \e[33;1m$DATA_DIR\e[0m"

# People dectector model
mkdir -p "$DATA_DIR/ultralytics"
cd "$DATA_DIR/ultralytics"

# yolov8m_cf_caja_person_cart_640x480_v3: 
wget "https://www.comet.com/api/asset/download?assetId=03cdc2bb292a43a7b93e6deedc7903a7&experimentKey=65be8d3e665a42729744eb8ff6a285ba" \
    -O yolov8m_1280x720.pt
