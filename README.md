# setup build env

1. First, unzip openhsm, then execute ./run.sh in the terminal.
2. Enter [http://localhost:8080/](http://localhost:8080/) in the browser to open the HSM server, and click "Token Management" to manage tokens.
3. Open the code and modify the token in setup.sh, for example:
   export HSM_TOKEN='dc2bb143a28832603da0f176aba237f94698d656f3c43525cff186b2de947166'
   ![1782978723881](image/README/1782978723881.png)

```shell
source setup.sh
```

#The following operations are performed only during the first compilation:

#First, add the following files to the specified locations:
chip/rts3917/fw/build.js

- `jmake_git_all.sh` in the `third` folder
- `jmake_git_all.sh` in the `tools` folder
  ![1782978829419](image/README/1782978829419.png)

# build rts3917 sdk

```shell
# Trigger git to download third-party libraries, and check that the src directory in the third-party library contains content.

cd third
./jmake_git_all.sh
cd -

cd tools
./prepare_jmake_dirs.sh
cd -
# The above script only needs to be executed for the first time.

cd chip/rts3917/sdk/rts39xx_sdk_v5.3/

./launch.sh 
# select rts3917n_base_defconfig and default name

cd -
cd chip/rts3917/sdk/rts39xx_sdk_v5.3/out/rts3917n_base

#if can`t link ram_init try  mkdir build/uboot-custom/ram_init and run again# if can`t link ram_init try  mkdir build/uboot-custom/ram_init and run again
make secure_boot M=1  # build once, every time run this , need rerun get_hash.sh
cd -

cd chip/rts3917/build_security_image/public_key

./get_hash.sh # this will copy public key for kernel
cd -

cd chip/rts3917/sdk/rts39xx_sdk_v5.3/out/rts3917n_base

make
cd -

# When compiling for the first time, you need to recompile u-boot and the kernel twice as previously done.
```

# now will finish uboot and kernel build

# build firmware

```shell
cd chip/rts3917/fw

jmake
```

Then after following the above steps, manually run the contents of make_rts3917n.sh again in the terminal, and afterwards you only need to run make_rts3917n.sh for compilation.
