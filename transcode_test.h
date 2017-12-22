#ifndef TRANSCODE_TEST_H
#define TRANSCODE_TEST_H

#ifdef  __cplusplus
extern "C" {
#define __STDC_FORMAT_MACROS
#endif

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/mathematics.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libavutil/error.h"
#include "libavutil/time.h"

#include "pthread.h"
#include "aupera/aup_lock.h"
#include "aupera/aup_fifo.h"
#include "aupera/aup_net.h"
#include "aupera/aup_common.h"
#include "aupera/aup_vcodec.h"
#include "aupera/aup_acodec.h"
#include "aupera/aup_vcodec_h264.h"
#include "libyuv.h"

#define TRANSCODE_VIDEO_FF_CODEC_ID    AV_CODEC_ID_H264
#define TRANSCODE_AUDIO_FF_CODEC_ID    AV_CODEC_ID_AAC

#define TRANSCODE_OUTPUT_DEFAULT_FRAMERATE    15
#define TRANSCODE_OUTPUT_DEFAULT_BITRATE      1024000
#define TRANSCODE_OUTPUT_DEFAULT_GOP          15
#define TRANSCODE_OUTPUT_AFRAME_SIZE          1024    //frame size for output audio
#define TRANSCODE_OUTPUT_ASAMPLE_RATE         44100
#define TRANSCODE_OUTPUT_ABITRATE             64000
#define TRANSCODE_OUTPUT_ACHANNEL             1
#define TRANSCODE_OUTPUT_ASTREAM_INDEX        1    //stream index for audio,set -1 means output no audio

#define TRANSCODE_OUTPUT_VSTREAM_INDEX        0    //stream index for video,set -1 means output no video

#define TRANSCODE_MAX_AFRAME_SIZE             2048  //max audio frame size(such as aac-he)
#define TRANSCODE_MAX_ACHANNEL                16

#define TRANSCODE_INPUT_MAX_STREAMS           10  //input max streams
#define TRANSCODE_OUTPUT_MAX_STREAMS          2   //output max streams
#define TRANSCODE_MAX_INPUT                   1
#define TRANSCODE_MAX_OUTPUT                  3
#define TRANSCODE_MAX_PARAMETERS              20

#define TRANSCODE_MAX_AVFORMATCTX_OPEN_WAIT  5000000  //us,max waiting time when open avFormatCtx

typedef struct ff_swr_factory_s
{
    SwrContext* pSwrCtx;
    int32_t srcSampleBytes;
    int32_t dstSampleBytes;
    int32_t srcSampleRate;
    int32_t dstSampleRate;
    enum AVSampleFormat srcSampleFmt;
    enum AVSampleFormat dstSampleFmt;
//    uint8_t **ppSrcData; //no need for now
    uint8_t **ppDstData;
    int32_t srcSamples;
    int32_t dstSamples;
    int32_t maxDstSamples;
    int32_t srcChannels;
    int32_t dstChannels;
    int32_t dstLinesize; //if dstChannels == 1,then dstLinesize is equal to bytes of an audio frame
    int64_t srcFrameDuration;
    int64_t dstFrameDuration;
    FIFO_T  sampleFifo;
    uint8_t* p;         //data point to store data from samplefifo
}ff_swr_factory_t;

typedef struct input_ff_s
{
    int32_t         running;
    int32_t         id;
    char*           url;
    aup_vcodec_t*   pVcodec;
    aup_acodec_t*   pAcodec;
    aup_vpic_t      inPIC;   //tmp picture ,for picture 16 alignment
    aup_vpic_t      lastVpic;
    AVFormatContext *pCtx;   //input context of avformat
    AVCodecContext* pCodecCtx[TRANSCODE_INPUT_MAX_STREAMS];
    int32_t         videoIndex;
    int32_t         audioIndex;
    int32_t         audioSampleBytes;
    int32_t         nbStreams;     //number of streams
}input_ff_t;

typedef struct output_ff_s
{
    int32_t         running;
    int32_t         id;
    char*           url;
    aup_vcodec_t*   pVcodec;
    aup_acodec_t*   pAcodec;
    aup_vpic_t      outPIC;  //tmp picture ,for picture scaling
    AVFormatContext *pCtx;   //input context of avformat
    int32_t         videoIndex;
    int32_t         audioIndex;
    int32_t         audioSampleBytes;
    int32_t         nbStreams;     //number of streams
    AVCodecContext* pCodecCtx[TRANSCODE_OUTPUT_MAX_STREAMS];
}output_ff_t;

typedef struct transcode_factory_s
{
    int32_t         running;
    input_ff_t      input[TRANSCODE_MAX_INPUT];
    output_ff_t     output[TRANSCODE_MAX_OUTPUT];
    ff_swr_factory_t* pFFSwrFactory[TRANSCODE_MAX_OUTPUT];
    int32_t         numOfInput;
    int32_t         numOfOutput;
}transcode_factory_t;


transcode_factory_t* transcode_factory_open(char* inputUrl,char** pOutputUrl,int32_t numOfOutput,int32_t outWidth,int32_t outFramerate,int32_t outBitrate,int32_t outGop);
int32_t transcode_factory_launch(transcode_factory_t* pTranscodeFactory);
int32_t transcode_factory_close(transcode_factory_t* pTranscodeFactory);


//vfilter api
int32_t vfilter_yuv420p_simple_crop(aup_vpic_t* pDstPic,aup_vpic_t* pSrcPic);
int32_t vfilter_yuv420p_scale(aup_vpic_t* pDstPic,aup_vpic_t* pSrcPic, LIBYUV_BOOL scaleMode);

//afilter api
int32_t do_audio_swr_convert(ff_swr_factory_t* pFFSwrFactory,AVFrame* pFrame);

#ifdef  __cplusplus
}
#endif

#endif // TRANSCODE_TEST_H

