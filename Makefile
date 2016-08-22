build: jni/sunxi.c
	ndk-build

clean:
	ndk-build clean

deploy:
	adb remount
	adb push features/android.hardware.hdmi.cec.xml /system/etc/permissions/android.hardware.hdmi.cec.xml
	adb push features/android.software.live_tv.xml /system/etc/permissions/android.software.live_tv.xml
	adb push libs/arm64-v8a/libhdmi_cec.tulip.so /system/lib64/hw/hdmi_cec.tulip.so
	adb push kernel/hdmi.ko /system/vendor/modules/hdmi.ko
	adb push kernel/hdmi_cec.ko /system/vendor/modules/hdmi_cec.ko

restart:
	adb shell stop
	adb shell start

configure:
	adb shell insmod /system/vendor/modules/hdmi_cec.ko
	sleep 1s
	adb shell chown system:system /dev/sunxi_hdmi_cec
	adb shell setprop ro.hdmi.device_type 4
