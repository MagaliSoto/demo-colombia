#!/bin/bash

set -e

# Check if PERCEPTION_MODELS_DIR is defined
if [ -z "$PERCEPTION_MODELS_DIR" ]; then
    echo "PERCEPTION_MODELS_DIR is not defined. Please set the environment variable."
    exit 1
fi

echo "Building Yolov8 Custom Library."
pushd "/app/custom-parser/deepstream-yolo"
cd nvdsinfer_custom_impl_Yolo
CUDA_VER=12.2 make clean
CUDA_VER=12.2 make -j $(nproc)
CUDA_VER=12.2 PREFIX=/opt/storage/custom-parser/deepstream-yolo/nvdsinfer_custom_impl_Yolo make install
cd ..
cp -r utils /opt/storage/custom-parser/deepstream-yolo/
popd

# Check if detector model exists, if not, proceed. Else exit.
if [ -f "${PERCEPTION_MODELS_DIR}/ultralytics/yolov8m_1280x720_b1_fp16_op17_dev0.engine" ]; then
    echo -e "YoloV8 TRT model already exists. Skipping model download."
    echo -e "All done!"
    exit 0
fi

# Download models
/app/scripts/get_models.bash ${PERCEPTION_MODELS_DIR}
pushd ${PERCEPTION_MODELS_DIR}
# Convert yolov8 models
cd ${PERCEPTION_MODELS_DIR}/ultralytics
# Generate TRT model using device 0
/app/scripts/yolov8_pt2trt.bash yolov8m_1280x720 1 736 1280 17 0
popd

echo "All done!"
exit 0
