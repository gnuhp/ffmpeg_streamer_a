LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := -D__STDC_CONSTANT_MACROS -DANDROID_BUILD

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../ffmpeg-0.10.3 \
    $(LOCAL_PATH)/include

LOCAL_SRC_FILES += \
    packetqueue.cpp \
    output.cpp \
    mediaplayer.cpp \
    decoder.cpp \
    decoder_audio.cpp \
    decoder_video.cpp \
    thread.cpp \
	../zjni/onLoad.cpp  #some JNI utils 

#LOCAL_LDLIBS := -L$(LOCAL_PATH)/../ffmpeg-0.10.3/android/armeabi/lib \
#				-lavcodec  -lswscale -lavutil  -lavformat -lpostproc \
#				-lz -lm -llog 
LOCAL_LDLIBS := -L$(LOCAL_PATH)/../ffmpeg-0.10.3/android/armeabi \
				-lffmpeg \
				-lz -lm -llog  -ljnigraphics

#LOCAL_STATIC_LIBRARIES := libavcodec libavformat libavutil libpostproc libswscale
LOCAL_MODULE := libmediaplayer

#include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)
