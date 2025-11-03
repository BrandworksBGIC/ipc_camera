#!/usr/bin/env bash

. ${BR2_EXTERNAL_platform_PATH}/board/common/inc.sh

if [ -d "$1" ]; then
	OUT_DIR=$1
else
	showInfo "no $1 directory, use pwd"
	OUT_DIR=./rtskey
fi

[ -d ${OUT_DIR} ] || mkdir ${OUT_DIR}

showInfo "===================================================================="
showInfo "generate rsa keys in path: ${OUT_DIR}"
showInfo "===================================================================="

#
# generate rsa private and pub keys, this will result following:
#
# rsa
# ├── *.key--------------------private key in pem format
# ├── *.crt--------------------self-signed certificate
# └── *_pub.der.sha256.bin-----hash of public key for efuse programming
#

VERIRY_KEY0=$2
VERIRY_KEY1=$3
VERIRY_KEY2=$4
VERIRY_KEYS="${VERIRY_KEY0} ${VERIRY_KEY1} ${VERIRY_KEY2}"


showInfo "using openssl in path: `which openssl`"

for K in ${VERIRY_KEYS}
do
	if [ -f ${OUT_DIR}/${K}.key ]; then
		showInfo "use previous generated private key: ${K}.key"
	else
		showInfo "generate private key: ${K}.key"
		openssl genrsa -out ${OUT_DIR}/${K}.key 2048
	fi
done

showInfo "generate ${VERIRY_KEY1} ${VERIRY_KEY2} self-signed certificate."
openssl req -batch -new -x509 -key ${OUT_DIR}/${VERIRY_KEY1}.key -out ${OUT_DIR}/${VERIRY_KEY1}.crt
openssl req -batch -new -x509 -key ${OUT_DIR}/${VERIRY_KEY2}.key -out ${OUT_DIR}/${VERIRY_KEY2}.crt

# gen hash for efuse prog
showInfo "generate ${VERIRY_KEY0} public key hash for efuse prog."
openssl rsa -in ${OUT_DIR}/${VERIRY_KEY0}.key -inform PEM -pubout -outform DER -out /tmp/${VERIRY_KEY0}_pub_$$.der
openssl dgst -sha256 -binary /tmp/${VERIRY_KEY0}_pub_$$.der > ${OUT_DIR}/${VERIRY_KEY0}_pub.der.sha256.bin
rm -rf /tmp/${VERIRY_KEY0}_pub_$$.der

