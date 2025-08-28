#!/bin/bash
set -e

# Print banner
echo -e "\r\n****************************************************"
echo -e "*** This script must be run inside dev container ***"
echo -e "****************************************************\r\n"

root_dir="/app"

# Check if the model name was passed as argument
if [[ $# -eq 0 ]]; then
    echo -e "\e[31;1mERROR: You must specify model name to convert, without extension .pt and batch size.\e[0m"
    exit -1
fi

# Define variables
MODEL_NAME=$1
BATCH_SIZE=$2
HEIGHT=$3
WIDTH=$4
OPSET=$5
DEVICE=0
if [[ $# -eq 6 ]]; then
DEVICE=$6
fi

echo "        Root Dir: $root_dir"
echo "Model to process: $MODEL_NAME.pt"
echo "      Batch Size: $BATCH_SIZE"
echo "          Height: $HEIGHT"
echo "           Width: $WIDTH"
echo "           Opset: $OPSET"
echo "          Device: $DEVICE"

# Convert PT to ONNX
python3 $root_dir/custom-parser/deepstream-yolo/utils/export_yoloV8.py -w ${MODEL_NAME}.pt -s ${HEIGHT} ${WIDTH} --dynamic --opset ${OPSET}
# Converto ONNX to TRT
/usr/src/tensorrt/bin/trtexec --device=$DEVICE --onnx=$MODEL_NAME.onnx --saveEngine=${MODEL_NAME}_b${BATCH_SIZE}_fp16_op${OPSET}_dev${DEVICE}.engine --fp16 --dumpOutput --skipInference --shapes=input:${BATCH_SIZE}x3x${HEIGHT}x${WIDTH} --minShapes=input:1x3x${HEIGHT}x${WIDTH} --maxShapes=input:${BATCH_SIZE}x3x${HEIGHT}x${WIDTH}
