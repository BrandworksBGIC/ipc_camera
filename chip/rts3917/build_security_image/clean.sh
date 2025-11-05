#!/bin/bash
set -e

BASE_DIR=$(pwd)
TEMPLATE_DIR=${BASE_DIR}/template
RTSKEY_DIR=${BASE_DIR}/rtskey
BUILD_DIR=${BASE_DIR}/build
TOOLS_DIR=${BASE_DIR}/tools
IMAGE_DIR=${BASE_DIR}/images
RELEASE_DIR=${BASE_DIR}/release
SHA_DIR=${BASE_DIR}/sha256
SIG_FILE_DIR=${BASE_DIR}/file_to_sign
SIG_DIR=${BASE_DIR}/signature

rm -rf ${BUILD_DIR}/
rm -rf ${RELEASE_DIR}/
rm -rf ${SHA_DIR}/
rm -rf ${SIG_FILE_DIR}/
rm -rf ${SIG_DIR}/
rm -rf ${IMAGE_DIR}/*.tree
rm -rf ${IMAGE_DIR}/*.info
rm -rf ${IMAGE_DIR}/*.table

echo "clean successfully!"
