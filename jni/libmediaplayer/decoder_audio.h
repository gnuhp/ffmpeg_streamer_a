#ifndef FFMPEG_DECODER_AUDIO_H
#define FFMPEG_DECODER_AUDIO_H

#include "decoder.h"

typedef void (*AudioDecodingHandler) (int16_t*,int);
//typedef void (*AudioDecodingHandler) (AVFrame*,int);

class DecoderAudio : public IDecoder
{
public:
    DecoderAudio(AVStream* stream);

    ~DecoderAudio();

    AudioDecodingHandler		onDecode;

private:
    AVCodec *pcm_codec;
    AVCodecContext *pcm_c;
    static SwrContext * swr; 

    //	AVFrame*					mFrame;
    int16_t*                    mSamples;
    int                         mSamplesSize;

    bool                        prepare();
    bool                        decode(void* ptr);
    bool                        process(AVPacket *packet);

    int                         encodeToPcm_init();
    void                        encodeAdpcmToPcm(int16_t * adpcm_buffer, int len);
    
    //void encodeAmrnbToPcm(AVFrame * mFrame);
    int encodeAmrnbToPcm (uint8_t * buffer, int len);
    int encode_audio_with_resampling( AVCodecContext *  enc, AVCodecContext * dec,  AVFrame *decoded_frame);



};

#endif //FFMPEG_DECODER_AUDIO_H
