cd The main directory where your code is located.
cd chip/rts3917/sdk/rts39xx_sdk_v5.3/
./launch.sh 
3    #Enter
cd out/rts3917n_base
make secure_boot M=1

cd ../../../../build_security_image/public_key
./get_hash.sh

cd ../../sdk/rts39xx_sdk_v5.3/out/rts3917n_base
make

cd ../../../../fw
jmake
