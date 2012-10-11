/*
 * mediaplayer.cpp
 */

//#define LOG_NDEBUG 0
#define TAG "FFMpegMediaPlayer-native"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

extern "C" {
	
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/log.h"
#include "libavfilter/avfilter.h"    
	
} // end of extern C


#include "mediaplayer.h"
#include "output.h"
#include "../zjni/jniUtils.h"

#ifdef ANDROID_BUILD
#include <android/log.h>
#endif

#define FPS_DEBUGGING false

#ifdef ANDROID_BUILD
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#else
#define __android_log_print(ANDROID_LOG_INFO, TAG, ...) printf(__VA_ARGS__)
#endif 



static MediaPlayer* sPlayer;

MediaPlayer::MediaPlayer()
{
    mListener = NULL;
    mCookie = NULL;
    mDuration = -1;
    //mStreamType = MUSIC;
    mCurrentPosition = -1;
    mSeekPosition = -1;
    mCurrentState = MEDIA_PLAYER_IDLE;
    mPrepareSync = false;
    mPrepareStatus = NO_ERROR;
    mLoop = false;
    pthread_mutex_init(&mLock, NULL);
    mLeftVolume = mRightVolume = 1.0;
    mVideoWidth = mVideoHeight = 0;
    sPlayer = this;
    ffmpegEngineInitialized = false; 
}

MediaPlayer::~MediaPlayer()
{
	if(mListener != NULL) {
		free(mListener);
	}
}

status_t MediaPlayer::initFFmpegEngine()
{
    //FFMPEG INIT code 

    avcodec_register_all(); 
    avfilter_register_all(); 
    av_register_all(); 
    avformat_network_init();

    ffmpegEngineInitialized = true; 

    return NO_ERROR; 
}

status_t MediaPlayer::prepareAudio(JNIEnv *env, jobject thiz)
{
	__android_log_print(ANDROID_LOG_INFO, TAG, "prepareAudio\n");
	mAudioStreamIndex = -1;
	for (int i = 0; i < mMovieFile->nb_streams; i++) {
		if (mMovieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			mAudioStreamIndex = i;
			break;
		}
	}
	
	if (mAudioStreamIndex == -1) {

        __android_log_print(ANDROID_LOG_INFO, TAG, "prepareAudio mAudioStreamIndex==-1\n");
        //mAudioStreamIndex -- could be -1 .. but prepare OK 

		return INVALID_OPERATION;
	}

	AVStream* stream = mMovieFile->streams[mAudioStreamIndex];
	// Get a pointer to the codec context for the video stream
	AVCodecContext* codec_ctx = stream->codec;
	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
	if (codec == NULL) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "prepareAudio Could not find audio codec. Maybe the stream has no audio\n");

		return INVALID_OPERATION;
	}


    //AVDictionary * opts1; --> Caused segmentation Fault // Same line in prepareVideo() is fine
    //av_dict_set(&opts1, "threads", "auto", 0);
	// Open codec
    if (avcodec_open(codec_ctx, codec) < 0) {
	//if (avcodec_open2(codec_ctx, codec, &opts1)< 0) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "prepareAudio 03\n");
		return INVALID_OPERATION;
	}

    int sampleRate = codec_ctx->sample_rate; 
    //  public static final int CHANNEL_CONFIGURATION_MONO = 2;
    //  public static final int CHANNEL_CONFIGURATION_STEREO = 3;
    int num_channels = (codec_ctx->channels ==1)?2:3; 

    //Start java pcm player here 
    jclass clazz;
    clazz = env->FindClass("com/media/ffmpeg/FFMpegPlayer");

    jmethodID native_startPcmPlayer = env->GetMethodID(clazz, "native_startPCMPlayer", "(II)V");
    if (native_startPcmPlayer == 0)
    {   
        __android_log_print(ANDROID_LOG_INFO, TAG,"Cant' find java void native_startPCMPlayer(int sampleRate, int channels)"); 
        return -1; 
    }

    env->CallVoidMethod(thiz, native_startPcmPlayer,sampleRate, num_channels); 


	__android_log_print(ANDROID_LOG_INFO, TAG, "prepareAudio DONE\n");
	return NO_ERROR;
}

status_t MediaPlayer::prepareVideo()
{
	__android_log_print(ANDROID_LOG_INFO, TAG, "prepareVideo\n");
	// Find the first video stream
	mVideoStreamIndex = -1;
	for (int i = 0; i < mMovieFile->nb_streams; i++) {
		if (mMovieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			mVideoStreamIndex = i;
			break;
		}
	}
	
	if (mVideoStreamIndex == -1) {
		return INVALID_OPERATION;
	}
	
	AVStream* stream = mMovieFile->streams[mVideoStreamIndex];
	// Get a pointer to the codec context for the video stream
	AVCodecContext* codec_ctx = stream->codec;
	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
	if (codec == NULL) {
		return INVALID_OPERATION;
	}
	
    AVDictionary * opts = NULL;
    av_dict_set(&opts, "threads", "auto", 0);
	// Open codec
	//if (avcodec_open(codec_ctx, codec) < 0) {
	if (avcodec_open2(codec_ctx, codec, &opts) < 0) {
		return INVALID_OPERATION;
	}
	
	mVideoWidth = codec_ctx->width;
	mVideoHeight = codec_ctx->height;
	mDuration =  mMovieFile->duration;
	
	mConvertCtx = sws_getContext(stream->codec->width,
								 stream->codec->height,
								 stream->codec->pix_fmt,
								 stream->codec->width,
								 stream->codec->height,
								 PIX_FMT_RGB565,
								 SWS_POINT,
								 NULL,
								 NULL,
								 NULL);

	if (mConvertCtx == NULL) {
		return INVALID_OPERATION;
	}

	void*		pixels;
    int         size; 
#if 1

    ///// &pixels is pointed to a Native bitmap. 
	if (Output::VideoDriver_getPixels(stream->codec->width,
									  stream->codec->height,
									  &pixels) != 0) {
		return INVALID_OPERATION;
	}
#else

    /* create temporary picture */
    size = avpicture_get_size(PIX_FMT_RGB565, stream->codec->width, stream->codec->height);
    pixels  = av_malloc(size);
    if (!pixels)
        return INVALID_OPERATION;

#endif

	mFrame = avcodec_alloc_frame();
	if (mFrame == NULL) {
		return INVALID_OPERATION;
	}
	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *) mFrame,
				   (uint8_t *) pixels,
				   PIX_FMT_RGB565,
				   stream->codec->width,
				   stream->codec->height);



	__android_log_print(ANDROID_LOG_INFO, TAG, "prepareVideo  DONE \n ");

	return NO_ERROR;
}

status_t MediaPlayer::prepare(JNIEnv *env, jobject thiz)
{
	status_t ret;
	mCurrentState = MEDIA_PLAYER_PREPARING;
	av_log_set_callback(ffmpegNotify);
	if ((ret = prepareVideo()) != NO_ERROR) {
		mCurrentState = MEDIA_PLAYER_STATE_ERROR;
		return ret;
	}

    //__android_log_print(ANDROID_LOG_INFO, TAG, "skip prepare Audio for now \n ");

	if ((ret = prepareAudio(env, thiz)) != NO_ERROR) {
		mCurrentState = MEDIA_PLAYER_STATE_ERROR;
		return ret;
	}
	mCurrentState = MEDIA_PLAYER_PREPARED;
	return NO_ERROR;
}

status_t MediaPlayer::setListener(MediaPlayerListener* listener)
{
    __android_log_print(ANDROID_LOG_INFO, TAG, "setListener");
    mListener = listener;
    return NO_ERROR;
}

status_t MediaPlayer::setDataSource(const char *url)
{
    __android_log_print(ANDROID_LOG_INFO, TAG, "setDataSource(%s) with av_open_input_file", url);
    status_t err = BAD_VALUE;

    //* try to init the ffMpeg engine if not done 
    if (ffmpegEngineInitialized == false)
    {
        initFFmpegEngine(); 
    }

	// Open video file
	if(av_open_input_file(&mMovieFile, url, NULL, 0, NULL) != 0) {
	//if(avformat_open_input(&mMovieFile, url, NULL,  NULL) != 0) {

        __android_log_print(ANDROID_LOG_INFO, TAG, "av_open error ");
		return INVALID_OPERATION;
	}

	// Retrieve stream information
	//if(av_find_stream_info(mMovieFile) < 0) {
    if(avformat_find_stream_info(mMovieFile, NULL) < 0) {

        __android_log_print(ANDROID_LOG_INFO, TAG, "av_find_stream_info error ");
		return INVALID_OPERATION;
	}

    av_dump_format(mMovieFile, 0, url, 0);

	mCurrentState = MEDIA_PLAYER_INITIALIZED;
    return NO_ERROR;
}

status_t MediaPlayer::suspend() {
	__android_log_print(ANDROID_LOG_INFO, TAG, "suspend");
	
	mCurrentState = MEDIA_PLAYER_STOPPED;
	if(mDecoderAudio != NULL) {
		mDecoderAudio->stop();
	}
	if(mDecoderVideo != NULL) {
		mDecoderVideo->stop();
	}
	
	if(pthread_join(mPlayerThread, NULL) != 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Couldn't cancel player thread");
	}
	
	// Close the codec
	free(mDecoderAudio);
	free(mDecoderVideo);
	
	// Close the video file
	//av_close_input_file(mMovieFile);
    avformat_close_input(&mMovieFile); 

	//close OS drivers
	Output::AudioDriver_unregister();
	Output::VideoDriver_unregister();

	__android_log_print(ANDROID_LOG_ERROR, TAG, "suspended");

    return NO_ERROR;
}

status_t MediaPlayer::resume() {
	//pthread_mutex_lock(&mLock);
	mCurrentState = MEDIA_PLAYER_STARTED;
	//pthread_mutex_unlock(&mLock);
    return NO_ERROR;
}

status_t MediaPlayer::setVideoSurface(JNIEnv* env, jobject jsurface)
{ 
	if(Output::VideoDriver_register(env, jsurface) != 0) {
		return INVALID_OPERATION;
	}
    return NO_ERROR;
}

status_t MediaPlayer::setAudioBufferCallBack(JNIEnv* env, jobject mediaPlayerClass)
{
	if(Output::AudioDriver_register(env, mediaPlayerClass) != 0) {
		return INVALID_OPERATION;
	}	
    return NO_ERROR;
}

bool MediaPlayer::shouldCancel(PacketQueue* queue)
{
	return (mCurrentState == MEDIA_PLAYER_STATE_ERROR || mCurrentState == MEDIA_PLAYER_STOPPED ||
			 ((mCurrentState == MEDIA_PLAYER_DECODED || mCurrentState == MEDIA_PLAYER_STARTED) 
			  && queue->size() == 0));
}

void MediaPlayer::decode(AVFrame* frame, double pts)
{

	if(FPS_DEBUGGING) {
		timeval pTime;
		static int frames = 0;
		static double t1 = -1;
		static double t2 = -1;

		gettimeofday(&pTime, NULL);
		t2 = pTime.tv_sec + (pTime.tv_usec / 1000000.0);
		if (t1 == -1 || t2 > t1 + 1) {
			__android_log_print(ANDROID_LOG_INFO, TAG, "Video fps:%i", frames);
			//sPlayer->notify(MEDIA_INFO_FRAMERATE_VIDEO, frames, -1);
			t1 = t2;
			frames = 0;
		}
		frames++;
	}


	// Convert the image from its native format to RGB
	sws_scale(sPlayer->mConvertCtx,
		      frame->data,
		      frame->linesize,
			  0,
			  sPlayer->mVideoHeight,
			  sPlayer->mFrame->data,
			  sPlayer->mFrame->linesize);

    // now sPlayer->mFrame has the picture data -- 
    //   start processing it .. 
    // (AVPicture *) mFrame


	Output::VideoDriver_updateSurface();
}

void MediaPlayer::decode(int16_t* buffer, int buffer_size)
{
	if(FPS_DEBUGGING) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "onDecodeA FPS debug");
		timeval pTime;
		static int frames = 0;
		static double t1 = -1;
		static double t2 = -1;

		gettimeofday(&pTime, NULL);
		t2 = pTime.tv_sec + (pTime.tv_usec / 1000000.0);
		if (t1 == -1 || t2 > t1 + 1) {
			__android_log_print(ANDROID_LOG_INFO, TAG, "Audio fps:%i", frames);
			//sPlayer->notify(MEDIA_INFO_FRAMERATE_AUDIO, frames, -1);
			t1 = t2;
			frames = 0;
		}
		frames++;
	}

    int byte_written = Output::AudioDriver_write(buffer, buffer_size);
	if(byte_written <= 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Couldn't write samples to audio track");
	}
}

void MediaPlayer::decodeMovie(void* ptr)
{
    int status; 
	AVPacket pPacket;
	
	AVStream* stream_audio = mMovieFile->streams[mAudioStreamIndex];
	mDecoderAudio = new DecoderAudio(stream_audio);
	mDecoderAudio->onDecode = decode;
	mDecoderAudio->startAsync();
	
	AVStream* stream_video = mMovieFile->streams[mVideoStreamIndex];
	mDecoderVideo = new DecoderVideo(stream_video);
	mDecoderVideo->onDecode = decode;
	mDecoderVideo->startAsync();
	
	mCurrentState = MEDIA_PLAYER_STARTED;
	__android_log_print(ANDROID_LOG_INFO, TAG, "MediaPlayer::decodeMovie %ix%i\n", mVideoWidth, mVideoHeight);
	while (mCurrentState != MEDIA_PLAYER_DECODED && mCurrentState != MEDIA_PLAYER_STOPPED &&
		   mCurrentState != MEDIA_PLAYER_STATE_ERROR)
	{
		if (mDecoderVideo->packets() > FFMPEG_PLAYER_MAX_QUEUE_SIZE &&
				mDecoderAudio->packets() > FFMPEG_PLAYER_MAX_QUEUE_SIZE) {
			usleep(200);
			continue;
		}
		
        status = av_read_frame(mMovieFile, &pPacket); 

		if(status < 0) {
			mCurrentState = MEDIA_PLAYER_DECODED;
			continue;
		}
		
		// Is this a packet from the video stream?
		if (pPacket.stream_index == mVideoStreamIndex)
        {
			mDecoderVideo->enqueue(&pPacket);
		} 
		else if (pPacket.stream_index == mAudioStreamIndex) 
        {
			mDecoderAudio->enqueue(&pPacket);
		}
		else 
        {
			// Free the packet that was allocated by av_read_frame
			av_free_packet(&pPacket);
		}
	}
	
	//waits on end of video thread
	__android_log_print(ANDROID_LOG_ERROR, TAG, "waiting on video thread\n");
	int ret = -1;
	if((ret = mDecoderVideo->wait()) != 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Couldn't cancel video thread: %i\n", ret);
	}
	
	__android_log_print(ANDROID_LOG_ERROR, TAG, "waiting on audio thread");
	if((ret = mDecoderAudio->wait()) != 0) {
		__android_log_print(ANDROID_LOG_ERROR, TAG, "Couldn't cancel audio thread: %i\n", ret);
	}
    
	if(mCurrentState == MEDIA_PLAYER_STATE_ERROR) {
		__android_log_print(ANDROID_LOG_INFO, TAG, "playing err\n");
	}
	mCurrentState = MEDIA_PLAYER_PLAYBACK_COMPLETE;
	__android_log_print(ANDROID_LOG_INFO, TAG, "end of playing\n");
}

void* MediaPlayer::startPlayer(void* ptr)
{
    __android_log_print(ANDROID_LOG_INFO, TAG, "starting main player thread\n");
    sPlayer->decodeMovie(ptr);

    return NULL;
}

status_t MediaPlayer::start()
{
	if (mCurrentState != MEDIA_PLAYER_PREPARED) {
		return INVALID_OPERATION;
	}
    int status; 
	status = pthread_create(&mPlayerThread, NULL, startPlayer, NULL);

    __android_log_print(ANDROID_LOG_INFO, TAG, "pthreadcreate return:%d\n", status);

	return NO_ERROR;
}

status_t MediaPlayer::stop()
{
	//pthread_mutex_lock(&mLock);
	mCurrentState = MEDIA_PLAYER_STOPPED;
	//pthread_mutex_unlock(&mLock);
    return NO_ERROR;
}

status_t MediaPlayer::pause()
{
	//pthread_mutex_lock(&mLock);
	mCurrentState = MEDIA_PLAYER_PAUSED;
	//pthread_mutex_unlock(&mLock);
	return NO_ERROR;
}

bool MediaPlayer::isPlaying()
{
    return mCurrentState == MEDIA_PLAYER_STARTED || 
		mCurrentState == MEDIA_PLAYER_DECODED;
}

status_t MediaPlayer::getVideoWidth(int *w)
{
	if (mCurrentState < MEDIA_PLAYER_PREPARED) {
		return INVALID_OPERATION;
	}
	*w = mVideoWidth;
    return NO_ERROR;
}

status_t MediaPlayer::getVideoHeight(int *h)
{
	if (mCurrentState < MEDIA_PLAYER_PREPARED) {
		return INVALID_OPERATION;
	}
	*h = mVideoHeight;
    return NO_ERROR;
}

status_t MediaPlayer::getCurrentPosition(int *msec)
{
	if (mCurrentState < MEDIA_PLAYER_PREPARED) {
		return INVALID_OPERATION;
	}
	*msec = 0/*av_gettime()*/;
	//__android_log_print(ANDROID_LOG_INFO, TAG, "position %i", *msec);
	return NO_ERROR;
}

status_t MediaPlayer::getDuration(int *msec)
{
	if (mCurrentState < MEDIA_PLAYER_PREPARED) {
		return INVALID_OPERATION;
	}
	*msec = mDuration / 1000;
    return NO_ERROR;
}

status_t MediaPlayer::seekTo(int msec)
{
    return INVALID_OPERATION;
}

status_t MediaPlayer::reset()
{
    return INVALID_OPERATION;
}

status_t MediaPlayer::setAudioStreamType(int type)
{
	return NO_ERROR;
}

void MediaPlayer::ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl) {
	
	switch(level) {
			/**
			 * Something went really wrong and we will crash now.
			 */
		case AV_LOG_PANIC:
			__android_log_print(ANDROID_LOG_ERROR, TAG, "AV_LOG_PANIC: %s", fmt);
			//sPlayer->mCurrentState = MEDIA_PLAYER_STATE_ERROR;
			break;
			
			/**
			 * Something went wrong and recovery is not possible.
			 * For example, no header was found for a format which depends
			 * on headers or an illegal combination of parameters is used.
			 */
		case AV_LOG_FATAL:
			__android_log_print(ANDROID_LOG_ERROR, TAG, "AV_LOG_FATAL: %s", fmt);
			//sPlayer->mCurrentState = MEDIA_PLAYER_STATE_ERROR;
			break;
			
			/**
			 * Something went wrong and cannot losslessly be recovered.
			 * However, not all future data is affected.
			 */
		case AV_LOG_ERROR:
			__android_log_print(ANDROID_LOG_ERROR, TAG, "AV_LOG_ERROR: %s", fmt);
			//sPlayer->mCurrentState = MEDIA_PLAYER_STATE_ERROR;
			break;
			
			/**
			 * Something somehow does not look correct. This may or may not
			 * lead to problems. An example would be the use of '-vstrict -2'.
			 */
		case AV_LOG_WARNING:
			__android_log_print(ANDROID_LOG_ERROR, TAG, "AV_LOG_WARNING: %s", fmt);
			break;
			
		case AV_LOG_INFO:
			__android_log_print(ANDROID_LOG_INFO, TAG, "%s", fmt);
			break;
			
		case AV_LOG_DEBUG:
			__android_log_print(ANDROID_LOG_DEBUG, TAG, "%s", fmt);
			break;
			
	}
}

void MediaPlayer::notify(int msg, int ext1, int ext2)
{
    //__android_log_print(ANDROID_LOG_INFO, TAG, "message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);
    bool send = true;
    bool locked = false;

    if ((mListener != 0) && send) {
       //__android_log_print(ANDROID_LOG_INFO, TAG, "callback application");
       mListener->notify(msg, ext1, ext2);
       //__android_log_print(ANDROID_LOG_INFO, TAG, "back from callback");
	}
}
