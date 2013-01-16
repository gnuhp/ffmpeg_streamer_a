#include "decoder_video.h"
#include "../zjni/jniUtils.h"


#ifdef ANDROID_BUILD
#include <android/log.h>
#endif


#define TAG "FFMpegVideoDecoder"

#ifdef ANDROID_BUILD
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#else
#define __android_log_print(ANDROID_LOG_INFO, TAG, ...) fprintf(stdout, __VA_ARGS__)
#endif 
static uint64_t global_video_pkt_pts = AV_NOPTS_VALUE;
static int decoder_reorder_pts = -1; 

DecoderVideo::DecoderVideo(AVStream* stream) : IDecoder(stream)
{
	 mStream->codec->get_buffer = getBuffer;
	 mStream->codec->release_buffer = releaseBuffer;
}

DecoderVideo::~DecoderVideo()
{
}

bool DecoderVideo::prepare()
{
	mFrame = avcodec_alloc_frame();
	if (mFrame == NULL) {
		return false;
	}
	return true;
}

double DecoderVideo::synchronize(AVFrame *src_frame, double pts) {

	double frame_delay;

	if (pts != 0) {
		/* if we have pts, set video clock to it */
		mVideoClock = pts;
	} else {
		/* if we aren't given a pts, set it to the clock */
		pts = mVideoClock;
	}
	/* update the video clock */
	frame_delay = av_q2d(mStream->codec->time_base);
	/* if we are repeating a frame, adjust clock accordingly */
	frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
	mVideoClock += frame_delay;
	return pts;
}

bool DecoderVideo::process(AVPacket *packet)
{
    int	completed = 0;
    int64_t  pts = 0;

	// Decode video frame
    int status = avcodec_decode_video2(mStream->codec,
            mFrame,
            &completed,
            packet);

    if (completed) {
        int ret = 1; 

        if (decoder_reorder_pts == -1) {
            pts = (int64_t)av_opt_ptr(avcodec_get_frame_class(), mFrame, "best_effort_timestamp");
        } else if (decoder_reorder_pts) {
            pts = mFrame->pkt_pts;
        } else {
            pts = mFrame->pkt_dts;
        }    

        if (pts == AV_NOPTS_VALUE) {
            pts = 0; 
        }    

#if 0
        if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_st) || is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK) &&
                (framedrop>0 || (framedrop && is->audio_st))) {
            SDL_LockMutex(is->pictq_mutex);
            if (is->frame_last_pts != AV_NOPTS_VALUE && *pts) {
                double clockdiff = get_video_clock(is) - get_master_clock(is);
                double dpts = av_q2d(is->video_st->time_base) * *pts;
                double ptsdiff = dpts - is->frame_last_pts;
                if (fabs(clockdiff) < AV_NOSYNC_THRESHOLD &&
                        ptsdiff > 0 && ptsdiff < AV_NOSYNC_THRESHOLD &&
                        clockdiff + ptsdiff - is->frame_last_filter_delay < 0) { 
                    is->frame_last_dropped_pos = pkt->pos;
                    is->frame_last_dropped_pts = dpts;
                    is->frame_drops_early++;
                    ret = 0; 
                }    
            }    
            SDL_UnlockMutex(is->pictq_mutex);
        }    
#else

		onDecode(mFrame,(int) pts);

		return true;

#endif

    }    
    return false ;



#if 0
	//__android_log_print(ANDROID_LOG_INFO, TAG, "avcodec_decode_video2 return: %d completed: %d\n",
    //        status, completed);


	if (packet->dts == AV_NOPTS_VALUE && mFrame->opaque
			&& *(uint64_t*) mFrame->opaque != AV_NOPTS_VALUE) {
		pts = *(uint64_t *) mFrame->opaque;
	} else if (packet->dts != AV_NOPTS_VALUE) {
		pts = packet->dts;
	} else {
		pts = 0;
	}
	pts *= av_q2d(mStream->time_base);

	if (completed) {
		pts = synchronize(mFrame, pts);

		onDecode(mFrame, pts);

		return true;
	}
	return false;
#endif 
}

bool DecoderVideo::decode(void* ptr)
{
	AVPacket        pPacket;
	
	__android_log_print(ANDROID_LOG_INFO, TAG, "decoding video \n");
	    if(attachVmToThread () != JNI_OK)
    {
        __android_log_print(ANDROID_LOG_INFO, TAG, "decoding video -- Attach VM to thread Failed");
        mRunning = false; 
    }

    while(mRunning)
    {
        if(mQueue->get(&pPacket, true) < 0)
        {
            mRunning = false;
            continue;
        }
        if(!process(&pPacket))
        {
            __android_log_print(ANDROID_LOG_INFO,TAG,"DecoderVideo::decode proccess return false:ignore"); 
            //mRunning = false;
            //continue;
        }


        // Free the packet that was allocated by av_read_frame
        av_free_packet(&pPacket);
    }
	
    // Free the RGB image
    av_free(mFrame);

    __android_log_print(ANDROID_LOG_INFO, TAG, "decoding video ended -- Dettach VM fr Thread");
    dettachVmFromThread();

    return true;
}

/* These are called whenever we allocate a frame
 * buffer. We use this to store the global_pts in
 * a frame at the time it is allocated.
 */
int DecoderVideo::getBuffer(struct AVCodecContext *c, AVFrame *pic) {
	int ret = avcodec_default_get_buffer(c, pic);
	uint64_t *pts = (uint64_t *)av_malloc(sizeof(uint64_t));
	*pts = global_video_pkt_pts;
	pic->opaque = pts;
	return ret;
}
void DecoderVideo::releaseBuffer(struct AVCodecContext *c, AVFrame *pic) {
	if (pic)
		av_freep(&pic->opaque);
	avcodec_default_release_buffer(c, pic);
}
