#!/bin/bash

sdk_dir=../../sdk/rts39xx_sdk_v5.3

openssl genrsa -out verity_key0_fake.key 2048

#openssl rsa -in ../private_key/verity_key1.key -pubout > verity_key1.pem
#openssl rsa -in ../private_key/verity_key2.key -pubout > verity_key2.pem
#openssl rsa -in ../private_key/verity_key0.key -pubout > verity_key0.pem

cp rts3917_sec_boot.pem verity_key0.pem
cp rts3917_sec_boot.pem verity_key1.pem
cp rts3917_sec_boot.pem verity_key2.pem


openssl rsa -pubin -in verity_key2.pem -inform PEM -RSAPublicKey_out -outform DER > verity_key2.der
openssl rsa -pubin -in verity_key0.pem -inform PEM -outform DER > verity_key0.der


openssl dgst -sha256 -binary verity_key0.der  > verity_key0_pub.der.sha256.bin

cp verity_key2.der ../../fw/rootfs/rootfs_ipcrt//etc/keys/verity_key2.der
cp verity_key2.der ${sdk_dir}/out/rts3917n_base/target-mini/etc/keys/
cp verity_key2.der ${sdk_dir}/out/rts3918n_base/target-mini/etc/keys/


cp verity_key1.pem ${sdk_dir}/out/rts3917n_base/rtskey/


# Function to create mkimage wrapper script
create_mkimage_wrapper() {
    local platform_path=$1
    local mkimage_dir="${platform_path}/host/bin"
    local current_dir=$(pwd)


    # Create the mkimage wrapper script
    echo '#!/bin/bash' > "${mkimage_dir}/mkimage"
    echo "export LD_LIBRARY_PATH=${current_dir}/../host_libs" >> "${mkimage_dir}/mkimage"
    echo "${current_dir}/../tools/mkimage_rts \"\$@\"" >> "${mkimage_dir}/mkimage"

    # Make it executable
    chmod +x "${mkimage_dir}/mkimage"

    echo "Created mkimage wrapper at: ${mkimage_dir}/mkimage"
}

# Create mkimage wrapper scripts for both platforms
create_mkimage_wrapper "${sdk_dir}/out/rts3917n_base"
create_mkimage_wrapper "${sdk_dir}/out/rts3918n_base"
