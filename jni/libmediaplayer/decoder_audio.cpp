#include "decoder_audio.h"
#include "../zjni/jniUtils.h"

#ifdef ANDROID_BUILD
#include <android/log.h>
#endif

#define TAG "FFMpegAudioDecoder"

#ifdef ANDROID_BUILD
#define LOGI(level, ...) if (level <= LOG_LEVEL) {__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__);}
#else
#define __android_log_print(ANDROID_LOG_INFO, TAG, ...) printf(__VA_ARGS__)
#endif 

DecoderAudio::DecoderAudio(AVStream* stream) : IDecoder(stream)
{
}

DecoderAudio::~DecoderAudio()
{
}

bool DecoderAudio::prepare()
{
    mSamplesSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    mSamples = (int16_t *) av_malloc(mSamplesSize);
    if(mSamples == NULL) {
    	return false;
    }

    if(encodeToPcm_init() <0)
    {
        return false; 
    }

    return true;
}

bool DecoderAudio::process(AVPacket *packet)
{
    int size = 0;

    int got_frame; 
    AVFrame mFrame; 
    int ret = avcodec_decode_audio4(mStream->codec,&mFrame, &got_frame, packet); 
    __android_log_print(ANDROID_LOG_INFO, TAG,"ret : %d\n", ret);

    AVCodecContext *avctx = mStream->codec;
    
    if (avctx->codec->id == CODEC_ID_AMR_NB )
    {
        //encodeAmrnbToPcm(&mFrame); 
        //encodeAmrnbToPcm((uint8_t*) mSamples,size); 

        encode_audio_with_resampling(pcm_c, avctx,  &mFrame); 
    }
    else 
    {

        if (ret >= 0 && got_frame) {
            int ch, plane_size;
            int planar = av_sample_fmt_is_planar(avctx->sample_fmt);
            int data_size = av_samples_get_buffer_size(&plane_size, avctx->channels,
                    mFrame.nb_samples,
                    avctx->sample_fmt, 1);
            if (mSamplesSize < data_size) {
                __android_log_print(ANDROID_LOG_INFO, TAG,"output buffer size is too small for "
                        "the current frame (%d < %d)\n", mSamplesSize, data_size);
                return AVERROR(EINVAL);
            }

            memcpy(mSamples, mFrame.extended_data[0], plane_size);

            if (planar && avctx->channels > 1) {
                uint8_t *out = ((uint8_t *)mSamples) + plane_size;
                for (ch = 1; ch < avctx->channels; ch++) {
                    memcpy(out, mFrame.extended_data[ch], plane_size);
                    out += plane_size;
                }
            }
            size = data_size;

            //__android_log_print(ANDROID_LOG_INFO, TAG, "codecid = %d size: %d", avctx->codec->id, size );
            //call handler for posting buffer to os audio driver
            if (avctx->codec->id == CODEC_ID_PCM_S16LE)
            {
                onDecode(mSamples, size);
            }
            else if (avctx->codec->id == CODEC_ID_ADPCM_SWF)
            {
                encodeAdpcmToPcm(mSamples, size); 
            }
            //else if (avctx->codec->id == CODEC_ID_AMR_NB )
            //{
            //    //encodeAmrnbToPcm(&mFrame); 
            //    //encodeAmrnbToPcm((uint8_t*) mSamples,size); 

            //    encode_audio_with_resampling(pcm_c, avctx,  &mFrame); 
            //}

            else
            {
                __android_log_print(ANDROID_LOG_INFO, TAG, "unhandle codec id:%d", avctx->codec->id);
            }

        }
        else
        {
            size = 0;
        }

    }

    return true;
}

bool DecoderAudio::decode(void* ptr)
{
    AVPacket        pPacket;

    __android_log_print(ANDROID_LOG_INFO, TAG, ">>>>> decoding audio -- Attach VM to thread");


    if(attachVmToThread () != JNI_OK)
    {
        __android_log_print(ANDROID_LOG_INFO, TAG, "decoding audio -- Attach VM to thread Failed");
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
            mRunning = false;
            continue;
        }
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&pPacket);

    }


    // Free audio samples buffer
    av_free(mSamples);


    __android_log_print(ANDROID_LOG_INFO, TAG, "decoding audio ended -- Dettach VM fr Thread");
    dettachVmFromThread();

    return true;
}



//Prepare the context & codec for PCM encoding
int DecoderAudio::encodeToPcm_init()
{
    AVCodecContext *avctx = mStream->codec;

    pcm_codec = NULL; 
    pcm_c = NULL; 

    /* find the PCM_ encoder */
    pcm_codec = avcodec_find_encoder(CODEC_ID_PCM_S16LE);
    if (!pcm_codec) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Encode to pcm init: cant'find pcm codec ");
        return -1 ; 
    }   

    pcm_c = avcodec_alloc_context3(pcm_codec);

    /* put sample parameters */
    pcm_c->sample_rate =  avctx->sample_rate;
    pcm_c->channels = avctx->channels ;
    pcm_c->sample_fmt = AV_SAMPLE_FMT_S16;

    /* open it */
    if (avcodec_open(pcm_c, pcm_codec) < 0) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Encode error open context ");
        return -2; 

    }


    swr = NULL; 

    return 0; 

}

void DecoderAudio::encodeAdpcmToPcm(int16_t * adpcm_buffer, int len)
{
    int out_size = -1; 

    int outbuf_size = len;
    uint8_t *  outbuf = (uint8_t *) malloc(outbuf_size);



    out_size = avcodec_encode_audio(pcm_c, outbuf, outbuf_size, adpcm_buffer); 

    __android_log_print(ANDROID_LOG_INFO, TAG, "encodeAdpcmToPcm size: %d\n", out_size );
    if (outbuf_size >0)
    {
        onDecode( (int16_t*)outbuf, out_size);
    }


    free(outbuf); 
}



//TODO: move to .h 
struct SwrContext * DecoderAudio::swr; 
static uint8_t *audio_buf;
static unsigned int allocated_audio_buf_size;

// SRC: ffmpeg.c ---- do_audio_out()..
int DecoderAudio::encode_audio_with_resampling( AVCodecContext *  enc, AVCodecContext * dec,  AVFrame *decoded_frame)
{
    
    uint8_t *buftmp;
    int64_t audio_buf_size,  size_out; 

    int osize = av_get_bytes_per_sample(enc->sample_fmt);
    int isize = av_get_bytes_per_sample(dec->sample_fmt);
    uint8_t *buf = decoded_frame->data[0];
    int size     = decoded_frame->nb_samples * dec->channels * isize;


    int audio_resample = 0; 
    int audio_sync_method  = 0; 

    audio_buf_size  = (0 /*allocated_for_size*/ + isize * dec->channels - 1) / (isize * dec->channels);
    audio_buf_size  = (audio_buf_size * enc->sample_rate + dec->sample_rate) / dec->sample_rate;
    audio_buf_size  = audio_buf_size * 2 + 10000; // safety factors for the deprecated resampling API
    audio_buf_size  = FFMAX(audio_buf_size, AVCODEC_MAX_AUDIO_FRAME_SIZE);
    audio_buf_size *= osize * enc->channels;


    if (audio_buf_size > INT_MAX) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                "Buffer sizes too large\n");
        return -1; 
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
            " audio_buf_size:%d   \n", audio_buf_size);


    av_fast_malloc(&audio_buf, &allocated_audio_buf_size, audio_buf_size);
    if (!audio_buf) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                "Out of memory in do_audio_out\n");
        return -1; 
    }   

    if (enc->channels != dec->channels
            || enc->sample_fmt != dec->sample_fmt
            || enc->sample_rate!= dec->sample_rate
       )    
    {    
        audio_resample = 1; 
        //__android_log_print(ANDROID_LOG_INFO, TAG,
        // "NEED resample.. Why? channels:%d/%d, samplefmt:%d/%d, samplerate:%d/%d audioSyncmethod:%d\n",
        //        enc->channels , dec->channels,
        //        enc->sample_fmt , dec->sample_fmt,
        //        enc->sample_rate, dec->sample_rate , 
        //        audio_sync_method);
    }                


    if ( audio_resample && (swr == NULL))
    {

        //Set some swr 
        swr = swr_alloc_set_opts(NULL,
                enc->channel_layout, enc->sample_fmt, enc->sample_rate,
                dec->channel_layout, dec->sample_fmt, dec->sample_rate,
                0, NULL);
        //if (0 /*ost->audio_channels_mapped*/)
        //{
        //    swr_set_channel_mapping(swr, 0 );
        //}
       
        av_opt_set_double(swr, "rmvol", 1.0 /*ost->rematrix_volume*/, 0);

        //if (0/*ost->audio_channels_mapped*/) {
        //    av_opt_set_int(swr, "icl", av_get_default_channel_layout(0), 0);
        //    av_opt_set_int(swr, "uch", 0, 0);
        //}

      
        if (av_opt_set_int(swr, "ich", dec->channels, 0) < 0) {
            __android_log_print(ANDROID_LOG_INFO, TAG,
                    "Unsupported number of input channels\n");
            return -1; 
        }
     
        if (av_opt_set_int(swr, "och", enc->channels, 0) < 0) {

            __android_log_print(ANDROID_LOG_INFO, TAG,
                    "Unsupported number of output channels\n");
            return -1;
        }

        
        if(audio_sync_method>1) av_opt_set_int(swr, "flags", SWR_FLAG_RESAMPLE, 0);

        if(swr && swr_init(swr) < 0){
            __android_log_print(ANDROID_LOG_INFO, TAG,
                    "swr_init() failed\n");

            swr_free(&swr);
        }


       if (!swr) {
            __android_log_print(ANDROID_LOG_INFO, TAG,
                    "Can not resample %d channels @ %d Hz to %d channels @ %d Hz\n",
                    dec->channels, dec->sample_rate,
                    enc->channels, enc->sample_rate);
            return -1; 
        }


    }

    if (audio_resample) 
    {
        buftmp = audio_buf;
        size_out = swr_convert(swr, ( uint8_t*[]){buftmp}, audio_buf_size / (enc->channels * osize),
                (const uint8_t*[]){buf   }, size / (dec->channels * isize));

        if (size_out == audio_buf_size / (enc->channels * osize))
        {
            __android_log_print(ANDROID_LOG_INFO, TAG,
                                        "Warning: audio buff is probably too small");
        }



        size_out = size_out * enc->channels * osize;
    } 
    else
    {
        buftmp = buf;
        size_out = size;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
            " size:%d  isize:%d osize:%d size_out:%d \n",
             size, isize, osize, size_out);





    //encode_audio_frame(s, ost, buftmp, size_out);

    // encode PCM & play 
    //encodeAmrnbToPcm(buftmp,size_out); 

    onDecode((int16_t*)buftmp, size_out/2);


    return 0; 
}

/* SRC: ffmpeg.c ---  int encode_audio_frame(....)
    TODO:  clean up 
 */ 

int DecoderAudio::encodeAmrnbToPcm(uint8_t * buf, int buf_size)
{
    AVCodecContext *enc = pcm_c; //  ost->st->codec;
    AVFrame *frame = NULL;
    AVPacket pkt;
    int ret, got_packet;

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    if (buf) {
        frame = avcodec_alloc_frame(); 
        if (frame->extended_data != frame->data)
            av_freep(&frame->extended_data);
        avcodec_get_frame_defaults(frame);

        frame->nb_samples  = buf_size /
            (enc->channels * av_get_bytes_per_sample(enc->sample_fmt));

        if ((ret = avcodec_fill_audio_frame(frame, enc->channels, enc->sample_fmt,
                        buf, buf_size, 1)) < 0) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "Audio encoding failed\n" );
            return -1; 
        }
    }

    got_packet = 0;
    if (avcodec_encode_audio2(enc, &pkt, frame, &got_packet) < 0) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Audio encoding failed\n" );
        return -1; 
    }

    ret = pkt.size;

    if (got_packet) {
        //pkt.stream_index = ost->index;
        if (pkt.pts != AV_NOPTS_VALUE)
            pkt.pts      = av_rescale_q(pkt.pts, enc->time_base, mStream->time_base);
        if (pkt.duration > 0)
        {

            pkt.duration = av_rescale_q(pkt.duration, enc->time_base, mStream->time_base);

            __android_log_print(ANDROID_LOG_INFO, TAG, "pkt.duration =%d pkt.size=%d\n", pkt.duration, pkt.size );
        }
       
        // 
        //write_frame(s, &pkt, ost);

        onDecode((int16_t*)pkt.data, pkt.size); 

        av_free_packet(&pkt);
    }

    return ret;
}

#if 0
void DecoderAudio::encodeAmrnbToPcm(AVFrame  *mFrame)
{
    AVPacket pkt = { 0 };
    av_init_packet(&pkt); 
    int got_packet, ret; 

    ret = avcodec_encode_audio2(pcm_c, &pkt, mFrame, &got_packet);
    if (ret < 0)
    {
        __android_log_print(ANDROID_LOG_INFO, TAG, "encodeAmrnbToPcm  error encoding frame\n" );
    }
    else
    {

        __android_log_print(ANDROID_LOG_INFO, TAG, "encodeAmrnbToPcm  ret: %d\n",ret );

        if (!got_packet)
        {
            return;
        }
        else
        {
            __android_log_print(ANDROID_LOG_INFO, TAG, "encodeAmrnbToPcm  datasize: %d\n", pkt.size );

            //Copy PCM data now.. 
            memcpy(mSamples, pkt.data, pkt.size);
            onDecode(mSamples, pkt.size); 
        }



    }

    av_free_packet(&pkt); 

}
#endif 


