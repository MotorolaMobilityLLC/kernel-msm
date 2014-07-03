#!/bin/bash

echo "HDCP Force Ri Mismatch Helper Script"
echo "Pushing helper module..."
adb push hdcp.ko /data/
echo "Running module"
adb shell insmod /data/hdcp.ko option="mismatch"
echo "Removing Module"
adb shell rmmod hdcp.ko
echo "Done. Look for HDCP messages in dmesg"
echo "Use: adb shell dmesg | grep \"hdcp\""
