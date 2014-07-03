#!/bin/bash

usage()
{
	echo "Usage 1: hdmicmd.sh [resolution=resolutionstring]"
	echo "Usage 2: hdmicmd.sh [vic=vic_number]"
	echo "for example:"
	echo "     hdmicmd.sh resolution=1280x720@60"
	echo "     hdmicmd.sh vic=4"
	echo "If no parameters, dmesg will print out the current configuration"
}

modulepath=./
module=hdmicmd.ko
if [ ! -f "$modulepath$module" ]; then
	echo "cannot find hdmicmd module $module"
	exit 1
fi

resolution=""
vic=""

if [ $# -ne 1 ]; then
	usage
	exit 0
fi

#parse input
input=`echo $1 | awk -F '=' ' { print $1 } '`
data=`echo $1 | awk -F '=' ' { print $2 } '`
#echo "$input $data"

if [[ $input == "resolution" ]]; then
    resolution="resolution=$data"
elif [[ $input == "vic" ]]; then
    vic="vic=$data"
else
	usage
	exit 2
fi

echo "adb push $modulepath$module /data/"
adb push $modulepath$module /data/

echo "adb shell insmod /data/$module $resolution $vic"
adb shell insmod /data/$module $resolution $vic

adb shell rmmod $module

