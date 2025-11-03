#/usr/bin/env bash

function showInfo ()
{
	echo $(tput smso 2>/dev/null)">>>   "$1$(tput rmso 2>/dev/null)
}
