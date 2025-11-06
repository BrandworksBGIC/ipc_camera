#!/bin/bash

cur_dir=$(pwd)

echo $@

cd ../build_security_image/

python3 ./sign_partition.py ${cur_dir}/$2  $1 eddsa ${cur_dir}/$3