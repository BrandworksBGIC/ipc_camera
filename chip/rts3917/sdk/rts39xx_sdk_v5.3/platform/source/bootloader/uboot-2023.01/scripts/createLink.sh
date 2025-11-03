#!/bin/sh
rm -rf ./board/realtek/rlxboard
rm -rf ./ram_init/*
rm -f ./include/configs/rlxboard.h
rm -f ./include/configs/bspchip.h

ln -s -r ./board/realtek/rts3917 ./board/realtek/rlxboard

ln -s -r ./board/realtek/rlxboard/rlxboard.h ./include/configs/rlxboard.h
ln -s -r ./board/realtek/rlxboard/bspchip.h ./include/configs/bspchip.h

for file in `ls board/realtek/rlxboard/ram_init/`
do
	echo $file
	ln -s -r ./board/realtek/rlxboard/ram_init/$file ./ram_init/$file
done
rm -rf ./ram_init/generate.sh
ln -s -r ./board/realtek/rlxboard/ram_init/generate.sh ./ram_init/generate.sh
