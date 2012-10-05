#include "output.h"
#include <stdlib.h>
#include "../zjni/jniUtils.h"
#ifdef ANDROID_BUILD
#include <android/log.h>
#include <android/bitmap.h>
#endif



extern "C" {
        
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
        
} // end of extern C




#define LOG_TAG "FFMpegMediaPlayer-native"
#define LOG_LEVEL 10


#ifdef ANDROID_BUILD
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#else
#define LOGI(level, ...) if (level <= LOG_LEVEL) {printf(__VA_ARGS__);}
#endif 


jmethodID Output::audio_write_cb;
jmethodID Output::get_audio_buff_cb;
jmethodID Output::audio_process_cb;

jmethodID Output::native_getNewBitMap;
jmethodID Output::native_updateVideoSurface;

jobject Output::mediaPlayerObject;

void * Output::pictureRGB;
ANativeWindow* Output::theNativeWindow;
ANativeWindow_Buffer * Output::pictureBuffer;


int Output::AudioDriver_register(JNIEnv * env, jobject thiz)
{

    //LOGI(9,"Output::AudioDriver_register"); 

    JNIEnv * env_ = env; 
    mediaPlayerObject = thiz; 

    jclass clazz;
    clazz = env_->FindClass("com/media/ffmpeg/FFMpegPlayer");

    //Setup audio buffer playback call back 

    get_audio_buff_cb = env->GetMethodID(clazz, "get_audio_buff","(I)[B");
    if (get_audio_buff_cb == 0)
    {   
        return -1;
    }   
    audio_process_cb = env->GetMethodID(clazz, "process_audio_buff_callback","(I)I");
     if (audio_process_cb == 0)
    {   
        return -1;
    }   


	//return AndroidAudioTrack_register();
	return 0 ; // not supported 
}

int Output::AudioDriver_unregister()
{
	//return AndroidAudioTrack_unregister();

    LOGI(9,"Output::AudioDriver_unregister"); 
    return 0; 
}

int Output::AudioDriver_start()
{
	//return AndroidAudioTrack_start();

    LOGI(9,"Output::AudioDriver_start"); 
    return 0;
}

int Output::AudioDriver_set(int streamType,
							uint32_t sampleRate,
							int format,
							int channels)
{
    LOGI(9,"Output::AudioDriver_set: str Type: %d, sampleRate: %d, format:%d, channels:%d",
            streamType, sampleRate,format,channels);

//	return AndroidAudioTrack_set(streamType,
//								 sampleRate,
//								 format,
//								 channels);
    return 0; 
}

int Output::AudioDriver_flush()
{
//	return AndroidAudioTrack_flush();

    LOGI(9,"Output::AudioDriver_flush"); 
    return 0; 
}

int Output::AudioDriver_stop()
{
    
    LOGI(9,"Output::AudioDriver_stop"); 
//	return AndroidAudioTrack_stop();
    return 0; 
}

int Output::AudioDriver_reload()
{

    LOGI(9,"Output::AudioDriver_reload"); 
//	return AndroidAudioTrack_reload();
    return 0; 
}

int Output::AudioDriver_write(void *buffer, int buffer_size)
{
    JNIEnv *env_ = getJNIEnv(); 
    //Throw the buffer back to java. 



#if 0
    jbyteArray bArray = env_->NewByteArray(buffer_size);
#else
    jbyteArray bArray = (jbyteArray) env_->CallObjectMethod(mediaPlayerObject,get_audio_buff_cb,buffer_size);

#endif

   // LOGI(9,"Output::audio_write 0555 "); 
#if 0
    jboolean isCopy;
    jbyte *data = env_->GetByteArrayElements(bArray, &isCopy);

    LOGI(9,"Output::audio_write 03 isCopy:%d ", isCopy); 
    if (data == NULL)
    {

        //Dont need to call release 
        LOGI(9,"Output::data == NULL out of memory?? "); 
        return 0; 
    }

    memcpy(data,(jbyte*) buffer, buffer_size);

    LOGI(9,"Output::audio_write 04"); 
    env_->ReleaseByteArrayElements( bArray, data, 0);
#else

    env_->SetByteArrayRegion(bArray, 0, buffer_size, (jbyte*)buffer); 

#endif 
    // VERY IMPORTANT----LiNE  
    //Fixed for  ReferenceTable overflow (max=512) ERROR --- DONT UNDERSTAND though
    env_->DeleteLocalRef(bArray); 

    //LOGI(9,"Output::audio_write 0666"); 

#if 0
    int ret = env_->CallIntMethod(mediaPlayerObject,audio_write_cb,bArray,buffer_size); 
#else
    int ret = env_->CallIntMethod(mediaPlayerObject,audio_process_cb,buffer_size); 
#endif 

    return ret ; 
}

//-------------------- Video driver --------------------

int Output::VideoDriver_register(JNIEnv* env, jobject jsurface)
{
    jclass clazz;
    clazz = env->FindClass("com/media/ffmpeg/FFMpegPlayer");

    //Setup audio buffer playback call back 
    native_getNewBitMap = env->GetMethodID(clazz, "native_getNewBitMap", "()Landroid/graphics/Bitmap;");
    if (native_getNewBitMap == 0)
    {   
        LOGI(9,"Output::VideoDriver_register 01"); 
        return -1;
    }   


    native_updateVideoSurface = env->GetMethodID(clazz, "native_updateVideoSurface","(Landroid/graphics/Bitmap;)V");
    if (native_updateVideoSurface == 0)
    {   

        LOGI(9,"Output::VideoDriver_register 02"); 
        return -1;
    }   


    // obtain a native window from a Java surface
    theNativeWindow = ANativeWindow_fromSurface(env, jsurface);

    return 0; 
}

int Output::VideoDriver_unregister()
{

    LOGI(9,"Output::VideoDriver_unregister"); 
    // make sure we don't leak native windows
    if (theNativeWindow != NULL) {
        ANativeWindow_release(theNativeWindow);
        theNativeWindow = NULL;
    }

#if 0
    if (pictureBuffer != NULL)
    {
        av_free(pictureBuffer->bits);
        free(pictureBuffer);
        pictureBuffer = NULL;
    }

    if (pictureRGB != NULL)
    {
        av_free(pictureRGB); 
        pictureRGB = NULL;
    }
#endif 

    return 0; 
}

int Output::VideoDriver_getPixels(int width, int height, void** pixels_out)
{

    LOGI(9,"Output::VideoDriver_getPixels: w:%d, h:%d", width, height); 
    
    //pictureRGB;
    int size;

    // Determine required buffer size and allocate buffer
    size = avpicture_get_size(PIX_FMT_RGB565, width, height);
    pictureRGB  = av_malloc(size);
    if (!pictureRGB)
        return -1;


    *pixels_out = pictureRGB; 
    
    pictureBuffer = (ANativeWindow_Buffer *) malloc( sizeof(ANativeWindow_Buffer)); 
    pictureBuffer->width =width; 
    pictureBuffer->height = height; 
    pictureBuffer->format = WINDOW_FORMAT_RGB_565;
    pictureBuffer->bits =  malloc(width*height*sizeof(uint16_t)); 

    if (theNativeWindow != NULL)
    {
        ANativeWindow_setBuffersGeometry(theNativeWindow, 
                pictureBuffer->width, pictureBuffer->height, 
                pictureBuffer->format); 
    }

    LOGI(9,"Output::VideoDriver_getPixels: picRGB size: %d bitsize:%d",size, width*height*sizeof(uint16_t)); 

    return 0; 
}

int Output::VideoDriver_updateSurface( )
{

#if 0 
    JNIEnv * env = getJNIEnv(); 
    if (env == NULL)
    {
        LOGI(9,"Output::VideoDriver_updateSurface env = NULL"); 
        return 0; 
    }

    jobject bitmap; 
    bitmap = env->CallObjectMethod(mediaPlayerObject, native_getNewBitMap) ; 

    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;

    
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {

        LOGI(9,"Output::VideoDriver_updateSurface getbitmap info <0"); 
        return 0 ;
    }   

    if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {

        LOGI(9,"Output::VideoDriver_updateSurface getbitmap formatp not RGB 565"); 
        return 0 ;
    }   

    if ((ret =AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0) 
    {
    }   

    memcpy(pixels, pictureRGB, 640*480*sizeof(uint16_t));

    AndroidBitmap_unlockPixels(env, bitmap);


    //Now draw it and cross my finger 
    env->CallVoidMethod(mediaPlayerObject, native_updateVideoSurface, bitmap ); 

    env->DeleteLocalRef(bitmap); 


#else

    //Another way: update pictureRGB directly to theNativeWindow


    if ( 0 > ANativeWindow_lock( theNativeWindow, pictureBuffer, NULL ) ) {
        return 0;
    }
    //Copy picture 
    memcpy(pictureBuffer->bits, pictureRGB, 640*480*sizeof(uint16_t));

    ANativeWindow_unlockAndPost( theNativeWindow );
#endif
    return 0; 
}
