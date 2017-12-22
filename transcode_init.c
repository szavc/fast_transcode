#include "transcode_test.h"

static int32_t get_ff_layout_from_channel(int32_t channels)
{
    int32_t channelLayout;
    if(channels == 1)
    {
        channelLayout =  AV_CH_LAYOUT_MONO;
    }
    else if(channels == 2)
    {
        channelLayout =  AV_CH_LAYOUT_STEREO;
    }
    else
    {
        err("not support channels larger than 2,change to 2 channels");
        channelLayout =  AV_CH_LAYOUT_STEREO;;
    }
    return channelLayout;
}

static int32_t get_ff_frame_size(int32_t profile)
{
    /*get audio frame size of each channel*/
    int32_t samples = 0;
    switch (profile)
    {
    case FF_PROFILE_AAC_MAIN:
    case FF_PROFILE_AAC_LOW:
    case FF_PROFILE_AAC_SSR:
    case FF_PROFILE_AAC_LTP:
        samples = 1024;
        break;
    case FF_PROFILE_AAC_HE:
    case FF_PROFILE_AAC_HE_V2:
        samples = 2048;
        break;
    case FF_PROFILE_AAC_LD:
        samples = 480;
        break;
    case FF_PROFILE_AAC_ELD:
        samples = 512;
        break;
    default:
        break;
    }
    return samples;
}

static int32_t get_ff_sample_bytes(enum AVSampleFormat sampleFmt)
{
    int32_t sampleBytes = -1;
    switch (sampleFmt)
    {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_U8P:
        sampleBytes = 1;
        break;
    case AV_SAMPLE_FMT_S16:
    case AV_SAMPLE_FMT_S16P:
        sampleBytes = 2;
        break;
    case AV_SAMPLE_FMT_S32:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_FLT:
        sampleBytes = 4;
        break;
    case AV_SAMPLE_FMT_DBL:
    case AV_SAMPLE_FMT_DBLP:
    case AV_SAMPLE_FMT_S64:
    case AV_SAMPLE_FMT_S64P:
        sampleBytes = 8;
        break;
    default:
        av_log(NULL,AV_LOG_ERROR,"get_sample_bytes error sample format,input sampleFmt id:%d\n",sampleFmt);
        break;
    }
    return sampleBytes;
}


int32_t transcode_input_ff_get_vdec_config(input_ff_t *pInFF, aup_vcodec_config_t* pConfig)
{
    int32_t ret = -1;
    if (NULL == pInFF) {return ret;}
    AVFormatContext* pInputCtx = pInFF->pCtx;
    if (pInputCtx && pConfig)
    {
        memset(pConfig,0,sizeof(*pConfig));
        struct AVStream *pStream = (pInputCtx->streams[pInFF->videoIndex]);
        //get extradata
        pConfig->h264.extraDataLen = pStream->codecpar->extradata_size;
        memcpy(pConfig->h264.extraData,pStream->codecpar->extradata,pConfig->h264.extraDataLen);
        dbg("extraDataLen = %d\n",pConfig->h264.extraDataLen);

        //get width,height,framerate
        pConfig->inputWidth = pStream->codecpar->width;

        pConfig->inputHeight = pStream->codecpar->height;
        pConfig->outputWidth = pConfig->inputWidth;
        pConfig->outputHeight = pConfig->inputHeight;

        float framerate = pStream->r_frame_rate.num/pStream->r_frame_rate.den;
        if(framerate == 0) {framerate = 15.0;}
        pConfig->inputFrameRate = round(framerate);
        pConfig->outputFrameRate = pConfig->inputFrameRate;

        info("w=%d,h=%d, framerate=%d(%f)\n",pConfig->inputWidth, pConfig->inputHeight,pConfig->inputFrameRate, framerate);

        //open decoder
        pConfig->codecType = AUP_VCODEC_H264_V108;
        pConfig->outputPixelFormat = AUP_VCODEC_PIXFMT_YUV420P;
        pConfig->flag = 0; //AUP_VCODEC_FLAG_NONBLOCK|AUP_VCODEC_FLAG_ZEROCPY
        /*********************************************************************/
        pConfig->debugEnable = 0; //CAUTION: set 1 to dump the YUV data to /tmp/aup_dec*
        /*********************************************************************/
        ret = 0;
    }
    return ret;
}

int32_t ff_swr_factory_close(ff_swr_factory_t* pFFSwrFactory)
{
    if(pFFSwrFactory)
    {
        if(pFFSwrFactory->pSwrCtx){swr_free(&(pFFSwrFactory->pSwrCtx));}

        if(pFFSwrFactory->ppDstData)
        {
            if (pFFSwrFactory->ppDstData[0]) {av_freep(&(pFFSwrFactory->ppDstData[0]));}
            av_freep(&(pFFSwrFactory->ppDstData));
        }

        if(pFFSwrFactory->sampleFifo){fifo_close(pFFSwrFactory->sampleFifo);}
        if(pFFSwrFactory->p) {free(pFFSwrFactory->p);}

        memset(pFFSwrFactory,0,sizeof(ff_swr_factory_t));
        free(pFFSwrFactory);
    }
    return 0;
}

ff_swr_factory_t* ff_swr_factory_open(uint64_t srcChannelLayout,uint64_t dstChannelLayout,int32_t srcSampleRate,int32_t dstSampleRate, \
                            enum AVSampleFormat srcSampleFmt,enum AVSampleFormat dstSampleFmt,AVRational srcTimeBase,AVRational dstTimeBase,int32_t srcProfile)
{
    ff_swr_factory_t* pFFSwrFactory = (ff_swr_factory_t*)calloc(sizeof(ff_swr_factory_t),1);
    if (NULL == pFFSwrFactory) {return NULL;}
    memset(pFFSwrFactory,0,sizeof(ff_swr_factory_t));

    pFFSwrFactory->srcSampleBytes = get_ff_sample_bytes(srcSampleFmt);
    pFFSwrFactory->dstSampleBytes = get_ff_sample_bytes(dstSampleFmt);
    pFFSwrFactory->srcSampleRate = srcSampleRate;
    pFFSwrFactory->dstSampleRate = dstSampleRate;
    pFFSwrFactory->srcSampleFmt = srcSampleFmt;
    pFFSwrFactory->dstSampleFmt = dstSampleFmt;
    pFFSwrFactory->srcSamples = get_ff_frame_size(srcProfile);
    if(pFFSwrFactory->srcSamples == 0)
    {
        av_log(NULL,AV_LOG_ERROR,"invalid profile:%d,not support now",srcProfile);
        ff_swr_factory_close(pFFSwrFactory);
        return NULL;
    }
    pFFSwrFactory->maxDstSamples = pFFSwrFactory->dstSamples = av_rescale_rnd(pFFSwrFactory->srcSamples, dstSampleRate, srcSampleRate, AV_ROUND_UP);
    pFFSwrFactory->srcChannels = av_get_channel_layout_nb_channels(srcChannelLayout);
    pFFSwrFactory->dstChannels = av_get_channel_layout_nb_channels(dstChannelLayout);
    if(pFFSwrFactory->srcChannels == 0 || pFFSwrFactory->dstChannels == 0)
    {
        av_log(NULL,AV_LOG_ERROR,"audio swr_convert channels error\n");
        ff_swr_factory_close(pFFSwrFactory);
        return NULL;
    }
    pFFSwrFactory->srcFrameDuration = pFFSwrFactory->srcSamples * srcTimeBase.den/pFFSwrFactory->srcSampleRate;
    pFFSwrFactory->dstFrameDuration = pFFSwrFactory->dstSamples * dstTimeBase.den/pFFSwrFactory->dstSampleRate;

    /* create resampler context */
    int32_t ret = -1;
    pFFSwrFactory->pSwrCtx = swr_alloc();
    if (NULL == pFFSwrFactory->pSwrCtx)
    {
        av_log(NULL,AV_LOG_ERROR,"Could not allocate resampler context\n");
        ff_swr_factory_close(pFFSwrFactory);
        return NULL;
    }

    /*set options for swr context*/
    av_opt_set_int(pFFSwrFactory->pSwrCtx, "in_channel_layout",    srcChannelLayout, 0);
    av_opt_set_int(pFFSwrFactory->pSwrCtx, "in_sample_rate",       srcSampleRate, 0);
    av_opt_set_sample_fmt(pFFSwrFactory->pSwrCtx, "in_sample_fmt", srcSampleFmt, 0);

    av_opt_set_int(pFFSwrFactory->pSwrCtx, "out_channel_layout",    dstChannelLayout, 0);
    av_opt_set_int(pFFSwrFactory->pSwrCtx, "out_sample_rate",       dstSampleRate, 0);
    av_opt_set_sample_fmt(pFFSwrFactory->pSwrCtx, "out_sample_fmt", dstSampleFmt, 0);

    /* initialize the resampling context */
    ret = swr_init(pFFSwrFactory->pSwrCtx);
    if(ret < 0)
    {
        av_log(NULL,AV_LOG_ERROR,"Failed to initialize the resampling context\n");
        ff_swr_factory_close(pFFSwrFactory);
        return NULL;
    }
    ret = av_samples_alloc_array_and_samples(&(pFFSwrFactory->ppDstData), &(pFFSwrFactory->dstLinesize), pFFSwrFactory->dstChannels,\
                                             pFFSwrFactory->dstSamples, dstSampleFmt, 0);
    if (ret < 0)
    {
        av_log(NULL,AV_LOG_ERROR,"Could not allocate destination samples\n");
        ff_swr_factory_close(pFFSwrFactory);
        return NULL;
    }

    int32_t  fifoSize = TRANSCODE_MAX_AFRAME_SIZE * TRANSCODE_MAX_ACHANNEL * pFFSwrFactory->dstSampleBytes;
    pFFSwrFactory->sampleFifo = fifo_open(fifoSize,1);
    if (NULL == pFFSwrFactory->sampleFifo) {ff_swr_factory_close(pFFSwrFactory);}
    pFFSwrFactory->p = (uint8_t*)malloc(fifoSize);
    if (NULL == pFFSwrFactory->p) {ff_swr_factory_close(pFFSwrFactory);}

    return pFFSwrFactory;
}

int32_t transcode_input_ff_close(input_ff_t* pInFF)
{
    uint32_t i;
    if (NULL == pInFF) {return -1;}

    if(pInFF->pVcodec) {aup_vdec_close(pInFF->pVcodec);}
    if(pInFF->pAcodec) {aup_adec_close(pInFF->pAcodec);}
    if(pInFF->pCtx)
    {
        for(i = 0;i < pInFF->pCtx->nb_streams;i++)
        {
            if(pInFF->pCodecCtx[i])
            {
                avcodec_free_context(&(pInFF->pCodecCtx[i]));
                pInFF->pCodecCtx[i] = NULL;
            }
        }

        info("close all pCodecCtx done\n");

        avformat_close_input(&(pInFF->pCtx));
        pInFF->pCtx = NULL;
        info("close all avformatCtx done\n");
    }
    return 0;
}

int32_t transcode_output_ff_close(output_ff_t* pOutFF)
{
    uint32_t i;
    if (NULL == pOutFF) {return -1;}

    if(pOutFF->pVcodec) {aup_venc_close(pOutFF->pVcodec);}
    if(pOutFF->pAcodec) {aup_aenc_close(pOutFF->pAcodec);}
    if(pOutFF->pCtx)
    {
        for(i = 0;i < pOutFF->pCtx->nb_streams;i++)
        {
            if(pOutFF->pCodecCtx[i])
            {
                avcodec_free_context(&(pOutFF->pCodecCtx[i]));
                pOutFF->pCodecCtx[i] = NULL;
            }
        }
        info("close all pCodecCtx done\n");
        if(pOutFF->pCtx->oformat)
        {
            if(!(pOutFF->pCtx->oformat->flags & AVFMT_NOFILE))
            {
                info("close output context io\n");
                avio_close(pOutFF->pCtx->pb);
                info("free output avFormat\n");
                avformat_free_context(pOutFF->pCtx);
            }
        }
        pOutFF->pCtx = NULL;
        info("close all avformatCtx done\n");
    }
    return 0;
}

static int interrupt_cb(void *opaque)
{
    input_ff_t* pInFF = (input_ff_t*)opaque;
    if (pInFF->running)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int32_t transcode_input_ff_context_open(input_ff_t* pInFF)
{
    if (NULL == pInFF) {return -1;}
    int32_t i;
    AVCodecContext* pCodecCtx = NULL;

    pInFF->pCtx = avformat_alloc_context();
    if (NULL == pInFF->pCtx)
    {
         err("avformat_alloc_context failed");
         return -1;
    }

    pInFF->running = 1;

    info("[%d]pInFF->pCtx addr = %p\n",pInFF->id,pInFF->pCtx);

    AVFormatContext *pCtx = pInFF->pCtx;

    pCtx->interrupt_callback.callback = interrupt_cb;
    pCtx->interrupt_callback.opaque = pInFF;

    AVDictionary* options = NULL;
    char timeOutChar[36];
    snprintf(timeOutChar,sizeof(timeOutChar),"%d",TRANSCODE_MAX_AVFORMATCTX_OPEN_WAIT);
    av_dict_set(&options, "stimeout", timeOutChar, 0); // in us,set timeout for opening input url

    if (avformat_open_input(&pCtx, pInFF->url, NULL, &options))
    {
        err( "Can not open input file %s\n",pInFF->url);
        avformat_free_context(pCtx);
        return -1;
    }

    info("[%d]open input file = %s\n",pInFF->id,pInFF->url);

    if (avformat_find_stream_info(pCtx, NULL) < 0)
    {
        err( "Failed to get input stream information\n");
        transcode_input_ff_close(pInFF);
        return -1;
    }

    info("[%d]find stream info done,nb_streams = %d\n",pInFF->id,pCtx->nb_streams);

    pInFF->nbStreams = pCtx->nb_streams;
    if(pInFF->nbStreams < 1 || pInFF->nbStreams > TRANSCODE_INPUT_MAX_STREAMS)
    {
        err("invalid input stream:%d,exit!\n",pInFF->nbStreams);
        transcode_input_ff_close(pInFF);
        return -1;
    }

    pInFF->videoIndex = -1;
    pInFF->audioIndex = -1;
    for(i = 0;i < pInFF->nbStreams;i++)
    {
        if(NULL == pCtx->streams[i])
        {
            transcode_input_ff_close(pInFF);
            return -1;
        }

        if(NULL == pCtx->streams[i]->codecpar)
        {
            transcode_input_ff_close(pInFF);
            return -1;
        }

        if((pCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)&&(pInFF->videoIndex < 0))
        {
            pInFF->videoIndex = i;
        }
        else if((pCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)&&(pInFF->audioIndex < 0))
        {
            pInFF->audioIndex = i;
        }
    }

    dbg("[%d]init videoIndex and audioIndex\n",pInFF->id);

    for(i = 0; i < pInFF->nbStreams; i++)
    {
        AVStream *stream = pCtx->streams[i];
        if(stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO||stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            pInFF->pCodecCtx[i] = avcodec_alloc_context3(NULL);
            if (NULL == pInFF->pCodecCtx[i])
            {
                transcode_input_ff_close(pInFF);
                return -1;
            }
            pCodecCtx = pInFF->pCodecCtx[i];
            avcodec_parameters_to_context(pCodecCtx, stream->codecpar);
            AVCodec *codec =  avcodec_find_decoder(pCodecCtx->codec_id);
            av_codec_set_pkt_timebase(pCodecCtx, stream->time_base);
            if(avcodec_open2(pCodecCtx, codec, NULL))
            {
                err("Could not open input decoder for '%s'\n",avcodec_get_name(pCodecCtx->codec_id));
                transcode_input_ff_close(pInFF);
                return -1;
            }
        }
    }

    if(pInFF->audioIndex == -1)
    {
        pInFF->audioSampleBytes = 0;
    }
    else
    {
        pCodecCtx = pInFF->pCodecCtx[pInFF->audioIndex];
        pInFF->audioSampleBytes = get_ff_sample_bytes(pCodecCtx->sample_fmt);
    }

    //dump input format
    info("dump input[%d] format\n",pInFF->id);
    av_dump_format(pCtx, 0, pInFF->url, 0);


    //malloc buffer for inPIC
    if(pInFF->videoIndex >= 0)
    {
        AVStream* pVstream = pCtx->streams[pInFF->videoIndex];
        uint32_t lumaSize = pVstream->codecpar->width*pVstream->codecpar->height;
        uint32_t chromaSize = lumaSize/2;
        pInFF->inPIC.Y = (uint8_t*)valloc(lumaSize + chromaSize);
        if (pInFF->inPIC.Y)
        {
            pInFF->inPIC.U = pInFF->inPIC.Y + lumaSize;
            pInFF->inPIC.V = pInFF->inPIC.U + chromaSize/2;
            pInFF->inPIC.w = pVstream->codecpar->width;
            pInFF->inPIC.h = pVstream->codecpar->height;
        }
        else
        {
            err("Fail to valloc %d bytes!",lumaSize+chromaSize);
            transcode_input_ff_close(pInFF);
            return -1;
        }
    }

    aup_vcodec_config_t config;
    int32_t err = transcode_input_ff_get_vdec_config(pInFF,&config);
    if (err) {transcode_input_ff_close(pInFF);return -1;}

    pInFF->pVcodec = aup_vdec_open(&config);
    if (NULL == pInFF->pVcodec) {transcode_input_ff_close(pInFF);return -1; }

    return 0;
}

int32_t transcode_output_ff_context_open(output_ff_t* pOutFF,int32_t width,int32_t height,int32_t outFrameRate,int32_t vbitrate,int32_t gop)
{
    if (NULL == pOutFF) {return -1;}

    int32_t i;
    int32_t ret = -1;
    AVCodec* encoder = NULL;
    AVCodecContext* pOutCodecCtx = NULL;
    AVStream* pOutputStream = NULL;
    AVOutputFormat* ofmt = NULL;

    pOutFF->running = 1;

    if(!strncmp(pOutFF->url, "rtmp", sizeof("rtmp") - 1))
    {
        avformat_alloc_output_context2(&(pOutFF->pCtx), NULL, "flv", pOutFF->url); //RTMP
    }
    else if(!strncmp(pOutFF->url, "udp", sizeof("udp") - 1))
    {
        avformat_alloc_output_context2(&(pOutFF->pCtx), NULL, "mpegts", pOutFF->url); //udp
    }
    else if(!strncmp(pOutFF->url, "/", 1))
    {
        avformat_alloc_output_context2(&(pOutFF->pCtx), NULL, "flv", pOutFF->url); //local file
    }
    else
    {
        //support more...
        err("not support this protocol,exit\n");
        transcode_output_ff_close(pOutFF);;
        return -1;
    }

    AVFormatContext* pOutputCtx = pOutFF->pCtx;
    info("output filename: %s\n",pOutputCtx->filename );
    if (!pOutputCtx) {
        err( "Could not create output context\n");
        transcode_output_ff_close(pOutFF);;
        return -1;
    }


    //open v108 encoder
    //Configuration - general
    aup_vcodec_config_t config;
    memset(&config,0,sizeof(config));
    config.codecType = AUP_VCODEC_H264_V108;
    config.outputPixelFormat = AUP_VCODEC_PIXFMT_YUV420P;
    config.inputWidth = width;//(width%16 == 0) ? width:((width/16 + 1)*16);
    config.inputHeight = height;
//    config.inputLineSize = (width%16 == 0) ? width:((width/16 + 1)*16);
    config.inputFrameRate = outFrameRate;
    config.outputWidth =  width;//(width%16 == 0) ? width:((width/16 + 1)*16);  //make it aligned to 16
    config.outputHeight = height;
    info("config.in[w,h] = [%d,%d],config.out[w,h] = [%d,%d]\n",config.inputWidth,config.inputHeight,config.outputWidth,config.outputHeight);
    config.outputFrameRate = outFrameRate;
    config.outputBitRate = vbitrate;

    //h264
    if (aup_vcodec_is_h264(config.codecType))
    {
        aup_venc_h264_config_default(&config);
        if (gop > 0) {config.h264.gopSize = gop;}
    }
    else
    {
        err("Not support yet!");
    }

    pOutFF->pVcodec = aup_venc_open(&config);
    if (NULL == pOutFF->pVcodec) {err("vencoder open failed\n");transcode_output_ff_close(pOutFF);return -1;}


    ofmt = pOutputCtx->oformat;
    ofmt->video_codec = TRANSCODE_VIDEO_FF_CODEC_ID;
    ofmt->audio_codec = TRANSCODE_AUDIO_FF_CODEC_ID;

    //get encoder extradata and put into video stream codecCtx
    uint32_t extraDataLen;
    uint8_t* pExtraData = NULL;
    while((pOutFF->running) && (pOutFF->pVcodec))
    {
        if (pOutFF->pVcodec)
        {
            extraDataLen = pOutFF->pVcodec->config.h264.extraDataLen;
            pExtraData = pOutFF->pVcodec->config.h264.extraData;
            if (extraDataLen > 0)
            {
                break;
            }
        }
        usleep(1000);
    }

    if (extraDataLen <= 0)
    {
        err("Fail to get the video extra data, thread quit!");
        transcode_output_ff_close(pOutFF);;
        return -1;
    }

    //new streams for output context
    int32_t vStreamIdx = TRANSCODE_OUTPUT_VSTREAM_INDEX;
    int32_t aStreamIdx = TRANSCODE_OUTPUT_ASTREAM_INDEX;
    int32_t numOfStreams = 0;
    for (i = 0; i < TRANSCODE_OUTPUT_MAX_STREAMS; i++)
    {
        if(i == vStreamIdx)
        {
            pOutputStream = avformat_new_stream(pOutputCtx, NULL);
            if (!pOutputStream)
            {
                err( "Failed allocating output stream\n");
                transcode_output_ff_close(pOutFF);;
                return -1;
            }
            encoder = avcodec_find_encoder(ofmt->video_codec);
            if (!encoder)
            {
                err("Necessary encoder not found\n");
                transcode_output_ff_close(pOutFF);
                return -1;
            }

            pOutFF->pCodecCtx[i] = avcodec_alloc_context3(encoder);
            pOutCodecCtx = pOutFF->pCodecCtx[i];
            if (!pOutCodecCtx)
            {
                err("Failed to allocate the encoder context\n");
                transcode_output_ff_close(pOutFF);
                return -1;
            }

            pOutputStream->start_time = 0;
            pOutputStream->id = i;
            pOutCodecCtx->extradata = (uint8_t*)av_malloc(extraDataLen);
            if (NULL == pOutCodecCtx->extradata)
            {
                transcode_output_ff_close(pOutFF);
                return -1;
            }
            pOutCodecCtx->extradata_size = extraDataLen;
            memcpy(pOutCodecCtx->extradata,pExtraData,extraDataLen);

            pOutFF->videoIndex = i;
            pOutCodecCtx->codec_id = ofmt->video_codec;
            pOutCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            pOutCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P; //FIXME
            pOutCodecCtx->frame_number = 1;
            //following five parameters need to input from cmd line
            pOutCodecCtx->time_base = AVRational{1,outFrameRate};   //FIXME
            pOutCodecCtx->framerate = AVRational{outFrameRate,1};           //outFrameRate
            pOutputStream->avg_frame_rate = AVRational{outFrameRate,1};  //need to set for video average framerate
            pOutputStream->r_frame_rate = AVRational{outFrameRate,1};
            pOutCodecCtx->width = width; //outWidth
            pOutCodecCtx->height = height;   //outHeight
            pOutCodecCtx->bit_rate = vbitrate;            //outBitRate
            pOutCodecCtx->gop_size = gop; //outGopSize

            pOutputStream->time_base = AVRational{1,1000};
            if (ofmt->flags & AVFMT_GLOBALHEADER) {pOutCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;}
            av_codec_set_pkt_timebase(pOutCodecCtx, pOutputStream->time_base);
            if (avcodec_open2(pOutCodecCtx, encoder, NULL))
            {
                err( "Cannot open encoder for stream #%u\n", i);
                transcode_output_ff_close(pOutFF);
                return -1;
            }
            ret = avcodec_parameters_from_context(pOutputStream->codecpar, pOutCodecCtx);
            if (ret < 0)
            {
                err( "Failed to copy encoder parameters to output stream #%u\n", i);
                transcode_output_ff_close(pOutFF);
                return -1;
            }
            numOfStreams++;
        }
        else if(i == aStreamIdx)
        {
            pOutputStream = avformat_new_stream(pOutputCtx, NULL);
            if (!pOutputStream)
            {
                err( "Failed allocating output stream\n");
                transcode_output_ff_close(pOutFF);;
                return -1;
            }

            if(ofmt->audio_codec == AV_CODEC_ID_AAC)
            {
                encoder = avcodec_find_encoder_by_name("libfdk_aac");
            }
            else
            {
                encoder = avcodec_find_encoder(ofmt->audio_codec);
            }

            if (!encoder)
            {
                err("Necessary encoder not found\n");
                transcode_output_ff_close(pOutFF);
                return -1;
            }

            pOutFF->pCodecCtx[i] = avcodec_alloc_context3(encoder);
            pOutCodecCtx = pOutFF->pCodecCtx[i];
            if (!pOutCodecCtx)
            {
                err("Failed to allocate the encoder context\n");
                transcode_output_ff_close(pOutFF);
                return -1;
            }

            pOutputStream->start_time = 0;
            pOutputStream->id = i;
            pOutFF->audioIndex = i;
            pOutCodecCtx->codec_id = ofmt->audio_codec;
            pOutCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
            pOutCodecCtx->sample_rate = TRANSCODE_OUTPUT_ASAMPLE_RATE;
            pOutCodecCtx->frame_number = 1;
            pOutCodecCtx->time_base = AVRational{1,pOutCodecCtx->sample_rate};
            pOutCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
            pOutFF->audioSampleBytes = get_ff_sample_bytes(pOutCodecCtx->sample_fmt);
            pOutCodecCtx->bit_rate = TRANSCODE_OUTPUT_ABITRATE;
            pOutCodecCtx->channels = TRANSCODE_OUTPUT_ACHANNEL;
            pOutCodecCtx->channel_layout = get_ff_layout_from_channel(pOutCodecCtx->channels);
            pOutCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
            pOutCodecCtx->profile  = FF_PROFILE_AAC_LOW;
            pOutCodecCtx->frame_size = TRANSCODE_OUTPUT_AFRAME_SIZE;

            pOutputStream->time_base = AVRational{1,1000};
            if (ofmt->flags & AVFMT_GLOBALHEADER) {pOutCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;}
            av_codec_set_pkt_timebase(pOutCodecCtx, pOutputStream->time_base);
            if (avcodec_open2(pOutCodecCtx, encoder, NULL))
            {
                err( "Cannot open encoder for stream #%u\n", i);
                transcode_output_ff_close(pOutFF);
                return -1;
            }
            ret = avcodec_parameters_from_context(pOutputStream->codecpar, pOutCodecCtx);
            if (ret < 0)
            {
                err( "Failed to copy encoder parameters to output stream #%u\n", i);
                transcode_output_ff_close(pOutFF);
                return -1;
            }
            numOfStreams++;
        }
    }

    pOutputCtx->nb_streams = numOfStreams;
    info("dump output[%d] format\n",pOutFF->id);
    av_dump_format(pOutputCtx, 0, pOutFF->url, 1);
    if (!(ofmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&pOutputCtx->pb, pOutFF->url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            err( "Could not open output file '%s'", pOutFF->url);
            transcode_output_ff_close(pOutFF);;
            return -1;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(pOutputCtx, NULL);
    if (ret < 0)
    {
        err( "Error occurred when opening output file\n");
        transcode_output_ff_close(pOutFF);
        return -1;
    }

    //malloc buffer for outPIC
    if(vStreamIdx >= 0)
    {
        uint32_t lumaSize = width*height;
        uint32_t chromaSize = lumaSize/2;
        pOutFF->outPIC.Y = (uint8_t*)valloc(lumaSize + chromaSize);
        if (pOutFF->outPIC.Y)
        {
            pOutFF->outPIC.U = pOutFF->outPIC.Y + lumaSize;
            pOutFF->outPIC.V = pOutFF->outPIC.U + chromaSize/2;
            pOutFF->outPIC.w = width;
            pOutFF->outPIC.h = height;
        }
        else
        {
            err("Fail to valloc %d bytes!",lumaSize+chromaSize);
            transcode_output_ff_close(pOutFF);
            return -1;
        }
    }

    return 0;
}

int32_t transcode_factory_close(transcode_factory_t* pTranscodeFactory)
{
    if (pTranscodeFactory)
    {
        int32_t i;
        pTranscodeFactory->running = 0;
        input_ff_t* pInFF = NULL;
        output_ff_t* pOutFF = NULL;
        ff_swr_factory_t* pFFSwrFactory = NULL;
        for(i = 0;i < TRANSCODE_MAX_INPUT;i++)
        {
            pInFF = &(pTranscodeFactory->input[i]);
            transcode_input_ff_close(pInFF);
        }

        for(i = 0;i < TRANSCODE_MAX_OUTPUT;i++)
        {
            pOutFF = &(pTranscodeFactory->output[i]);
            transcode_output_ff_close(pOutFF);
        }
        for(i = 0;i < TRANSCODE_MAX_OUTPUT;i++)
        {
            pFFSwrFactory = pTranscodeFactory->pFFSwrFactory[i];
            if(pFFSwrFactory)
            {
                ff_swr_factory_close(pFFSwrFactory);
                pFFSwrFactory = NULL;
            }
            info("close ff swr factory done\n");
        }
    }
    return 0;
}

transcode_factory_t* transcode_factory_open(char* inputUrl,char** pOutputUrl,int32_t numOfOutput,int32_t outWidth,int32_t outFramerate,int32_t outBitrate,int32_t outGop)
{
    transcode_factory_t* pTranscodeFactory = (transcode_factory_t*)calloc(1,sizeof(transcode_factory_t));
    if (NULL == pTranscodeFactory) {return NULL;}

    pTranscodeFactory->running = 1;
    pTranscodeFactory->numOfInput = 1;
    pTranscodeFactory->numOfOutput = 1;   //for test,now only support one output
    int32_t err = -1;

    input_ff_t* pInFF = &(pTranscodeFactory->input[0]);
    pInFF->id = 0;
    pInFF->url = inputUrl;
    err = transcode_input_ff_context_open(pInFF);
    if(err)
    {
        err("transcode input ff context open failed\n");
        return NULL;
    }

    if (outWidth == 0) {outWidth = pInFF->inPIC.w;}
    if (outFramerate == 0) {outFramerate = TRANSCODE_OUTPUT_DEFAULT_FRAMERATE;}
    if (outBitrate == 0) {outBitrate = TRANSCODE_OUTPUT_DEFAULT_BITRATE;}
    if (outGop == 0) {outGop = TRANSCODE_OUTPUT_DEFAULT_GOP;}

    output_ff_t* pOutFF = &(pTranscodeFactory->output[0]);
    pOutFF->url = pOutputUrl[0];
    int32_t outHeight = pInFF->inPIC.h * outWidth/pInFF->inPIC.w;
    err = transcode_output_ff_context_open(pOutFF,outWidth,outHeight,outFramerate,outBitrate,outGop);
    if(err)
    {
        err("transcode output ff context open failed\n");
        return NULL;
    }

    if((pInFF->audioIndex >= 0) && (pOutFF->audioIndex >= 0))
    {
        AVCodecContext* pInCodecCtx = pInFF->pCodecCtx[pInFF->audioIndex];
        AVCodecContext* pOutCodecCtx = pOutFF->pCodecCtx[pOutFF->audioIndex];
        AVStream* pInAStream = pInFF->pCtx->streams[pInFF->audioIndex];
        AVStream* pOutAStream = pOutFF->pCtx->streams[pOutFF->audioIndex];
        pTranscodeFactory->pFFSwrFactory[0] = ff_swr_factory_open(pInCodecCtx->channel_layout,pOutCodecCtx->channel_layout,pInCodecCtx->sample_rate,\
                    pOutCodecCtx->sample_rate,pInCodecCtx->sample_fmt,pOutCodecCtx->sample_fmt,pInAStream->time_base,pOutAStream->time_base,pInCodecCtx->profile); //open sws context for audio resampling
        if (NULL == pTranscodeFactory->pFFSwrFactory[0]) {transcode_input_ff_close(pInFF);return NULL;}
    }
    return pTranscodeFactory;
}
