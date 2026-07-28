#!/system/bin/sh
#echo "Starting CAN setup" > /dev/kmsg
ip link set can0 down
ip link set can0 up type can bitrate 250000
#echo "CAN setup completed" > /dev/kmsg
