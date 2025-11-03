#!/bin/bash

RTSVER=0

function calc_ver() {
	ver_a=`echo $@ |awk -F '[.]' '{printf $1}'`
	ver_b=`echo $@ |awk -F '[.]' '{printf $2}'`

	RTSVER=$(($ver_a * 100 + $ver_b))
}

function get_ver_from_str() {
	vstr=$1

	#echo $vstr

	if [ "$vstr" == "master" ]; then
		RTSVER=9527
		return 1
	fi

	vst=`echo $vstr|grep -o '[sS][dD][kK]_[vV][0-9]\+.[0-9]\+'`
	#echo $vst
	if [ $vst"mxq" != "mxq" ]; then
		vst=`echo $vstr|grep -o '[0-9]\+.[0-9]\+'`
		calc_ver $vst
		return 1
	fi

	return 0
}


function get_branch_ver() {
	ver=0

	git status > /dev/null 2>&1

	ret=$?

	#echo $ret

	if [ $ret -eq 0 ]; then
		vstr=`git status|head -1|awk '{print $NF}'`
		get_ver_from_str $vstr
		if [ $? -le 1 ]; then
			vstr=`git branch --remote|head -1|awk '{print $1}'|
				awk -F '[/]' '{print $NF}'`
			get_ver_from_str $vstr
		fi
	fi

	echo $RTSVER
}

function get_buildnum() {
	buildnum=0

	git status > /dev/null 2>&1

	if [ $? -eq 0 ]; then
		buildnum=`git log --pretty=oneline|head -1|awk '{print $1}'|
				awk -F, '{print substr($1, 1, 6)}'`
	fi

	echo $buildnum
}

if [ $# -ge 1 ]; then
	if [ $1 == "version" ]; then
		get_branch_ver
	elif [ $1 == "number" ]; then
		get_buildnum
	fi
fi
