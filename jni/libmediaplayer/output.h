#ifndef FFMPEG_OUTPUT_H
#define FFMPEG_OUTPUT_H
#include <sys/types.h>
#include <jni.h>

#include <android/native_window_jni.h>

class Output
{
    private:
    static jmethodID audio_write_cb; 
    static jobject mediaPlayerObject;
    static jmethodID get_audio_buff_cb;
    static jmethodID audio_process_cb;

    static jmethodID native_getNewBitMap;
    static jmethodID native_updateVideoSurface;

    static void * pictureRGB; 

    static ANativeWindow* theNativeWindow;
    static ANativeWindow_Buffer * pictureBuffer; 

    public:	

    static int AudioDriver_register(JNIEnv * env, jobject thiz);

    static int					AudioDriver_set(int streamType,
            uint32_t sampleRate,
            int format,
            int channels);
    static int					AudioDriver_start();
    static int					AudioDriver_flush();
    static int					AudioDriver_stop();
    static int					AudioDriver_reload();
    static int					AudioDriver_write(void *buffer, int buffer_size);
    static int					AudioDriver_unregister();

    static int					VideoDriver_register(JNIEnv* env, jobject jsurface);
    static int					VideoDriver_getPixels(int width, int height, void** pixels);
    static int					VideoDriver_updateSurface();
    static int					VideoDriver_unregister();
};

#endif //FFMPEG_DECODER_H
