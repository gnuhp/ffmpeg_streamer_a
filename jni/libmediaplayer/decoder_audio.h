#ifndef FFMPEG_DECODER_AUDIO_H
#define FFMPEG_DECODER_AUDIO_H

#include "decoder.h"

//typedef void (*AudioDecodingHandler) (int16_t*,int);
typedef void (*AudioDecodingHandler) (uint8_t*,int);

class DecoderAudio : public IDecoder
{
public:
    DecoderAudio(AVStream* stream);

    ~DecoderAudio();

    AudioDecodingHandler		onDecode;

private:
    AVCodec *pcm_codec;
    AVCodecContext *pcm_c;
    struct SwrContext * swr; 

    int16_t*                    mSamples;
    int                         mSamplesSize;

    bool                        prepare();
    bool                        decode(void* ptr);
    bool                        process(AVPacket *packet);

    int                         encodeToPcm_init();
    int encode_audio_with_resampling( AVCodecContext *  enc, AVCodecContext * dec,  AVFrame *decoded_frame);
    //Obsolete funcs: 
    //void                        encodeAdpcmToPcm(int16_t * adpcm_buffer, int len);
    // int encodeAmrnbToPcm (uint8_t * buffer, int len);



};

#endif //FFMPEG_DECODER_AUDIO_H
