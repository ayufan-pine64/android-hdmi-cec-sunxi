export NDK_PROJECT_PATH := .

build:
	ndk-build APP_BUILD_SCRIPT=./Android.mk
