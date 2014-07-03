#!/bin/bash

usage()
{
	echo "Usage : edid_print.sh [log_level=0 or 1 or 2 or 3 or 4]"
	echo "		log_level = 0 - prints error messages"
	echo "		log_level = 1 - prints error and high priority messages"
	echo "		log_level = 2 - prints error, high and low priority messages"
	echo "		log_level = 3 - prints error, high, low and VBLANK messages"
	echo "		log_level = 4 - prints all messages"
	echo "for example:"
	echo "     edid_print.sh log_level=3"
}

modulepath=./
module=edidprint.ko
if [ ! -f "$modulepath$module" ]; then
	echo "cannot find edid_print module $module"
	exit 1
fi

log_level=""

if [ $# -ne 1 ]; then
	usage
	exit 0
fi

#parse input
input=`echo $1 | awk -F '=' ' { print $1 } '`
data=`echo $1 | awk -F '=' ' { print $2 } '`
#echo "$input $data"

if [[ $input == "log_level" ]]; then
    log_level="log_level=$data"
else
	usage
	exit 2
fi

echo "adb push $modulepath$module /data/"
adb push $modulepath$module /data/

echo "adb shell insmod /data/$module $log_level"
adb shell insmod /data/$module $log_level

adb shell rmmod $module

