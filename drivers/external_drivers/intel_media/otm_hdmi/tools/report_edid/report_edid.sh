#!/bin/sh

echo "EDID report helper script"
echo "Pushing helper module..."
adb push report_edid.ko /data/
echo "Running module..."
adb shell insmod /data/report_edid.ko
echo "Cleaning up.."
adb shell rmmod report_edid.ko
echo "DONE. Look for EDID report in dmesg"

