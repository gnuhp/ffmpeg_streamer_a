LOCAL_PATH := $(call my-dir)
#CPU := armeabi 
#armeabi armv7-a


#declare the prebuilt library
include $(CLEAR_VARS)
LOCAL_MODULE := ffmpeg-prebuilt
LOCAL_SRC_FILES := android/$(TARGET_ARCH_ABI)/libffmpeg.so
LOCAL_EXPORT_C_INCLUDES := android/$(TARGET_ARCH_ABI)/include
LOCAL_EXPORT_LDLIBS := android/$(TARGET_ARCH_ABI)/libffmpeg.so
LOCAL_PRELINK_MODULE := true
include $(PREBUILT_SHARED_LIBRARY)

