## Pine A64 HDMI experimental driver

This is experimental HDMI-CEC driver for Pine A64 for Android builds.

Sometimes it does work, sometimes it does not.

### How to run it?

1. Connect Ethernet cable.

1. Connect to ADB: `adb connect IP_ADDRESS_OF_PINE`.

1. Do: `make deploy` to install kernel module, android HW driver and needed features.

1. Restart.

1. Reconnect with ADB: `adb disconnect; adb connect IP_ADDRESS_OF_PINE`.

1. Run `make configure` and wait to check if HDMI CEC do work.

### Troubleshooting

1. Kernel log messages: `adb shell dmesg | grep -i hdmi`

1. Android log messages: `adb logcat | grep -i hdmi`

### Author

Kamil Trzciński <ayufan@ayufan.eu>
