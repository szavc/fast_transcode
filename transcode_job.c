#include "transcode_test.h"


//FILE* outyuv1  = fopen("/a/out1.yuv","wb");
//FILE* outyuv2  = fopen("/a/out2.yuv","wb");


int32_t copy_avpacket_to_aupvpkt(aup_vpkt_t *inputVpkt,AVPacket *pkt)
{
    if (NULL == pkt || NULL == inputVpkt) {return -1;}
    if (inputVpkt->size > (uint32_t)pkt->size)
    {
        inputVpkt->id = 0;
        inputVpkt->len = pkt->size;
        memcpy(inputVpkt->p,pkt->data,inputVpkt->len);
        return 0;
    }
    return -1;
}

int32_t copy_aupvpkt_to_avpacket(AVPacket *pkt,aup_vpkt_t *inputVpkt)
{
    if (NULL == pkt || NULL == inputVpkt) {return -1;}
    pkt->size = inputVpkt->len;
    pkt->data = inputVpkt->p;
    return 0;
}

int32_t transcode_process_video(transcode_factory_t* pTranscodeFactory,aup_vpkt_t* pInAupPkt)
{
    if (pTranscodeFactory && pInAupPkt)
    {
        int32_t err = -1;
        input_ff_t* pInFF = &(pTranscodeFactory->input[0]);
        output_ff_t* pOutFF = &(pTranscodeFactory->output[0]);
        aup_vpic_t tmpPIC;

        if(pOutFF->videoIndex < 0) {return 0;} //no need to  process video

        AVStream* vStream = pInFF->pCtx->streams[pInFF->videoIndex];
        if (NULL == vStream) {return 0;}
        //do decode job
        if(pInFF->running)
        {
            err = aup_vdec_frame(pInFF->pVcodec,pInAupPkt,&tmpPIC);//tmpPIC memory is borrowed from decoder
            if (err)
            {
                err("v108h264 decode a video packet failed\n");
                return -1;
            }
        }
        chkp();

//        int32_t refcnt = aup_codec_ref_count(pInFF->lastVpic.pRef);
//        if (refcnt > 0)
//        {
//            aup_vpic_unref(&(pInFF->lastVpic));       //last picture is used, unref it
//        }

//        aup_vpic_ref(&(pInFF->lastVpic),&tmpPIC);     //ref and store current picture

//        aup_vpic_unref(&tmpPIC);                        //current picture copy is used, unref it
//        aup_vpic_t* pDstPIC = &(pInFF->lastVpic);

//        fwrite(tmpPIC.Y,1,tmpPIC.w*tmpPIC.h,outyuv1);
//        fwrite(tmpPIC.U,1,tmpPIC.w*tmpPIC.h/4,outyuv1);
//        fwrite(tmpPIC.V,1,tmpPIC.w*tmpPIC.h/4,outyuv1);

        //do video filter here
        aup_vpic_t* pDstPIC = &(tmpPIC);
        if(tmpPIC.Y)//decode succeed and return YUV420P data
        {
            //if src picture width or height is not aligned to 16,need to crop
            if((tmpPIC.crop.x >= 0) && (tmpPIC.crop.y >= 0) && (tmpPIC.crop.w > 0) && (tmpPIC.crop.h > 0) \
                    && ((tmpPIC.w > (uint32_t)tmpPIC.crop.w) || (tmpPIC.h > (uint32_t)tmpPIC.crop.h)))
            {
                vfilter_yuv420p_simple_crop(&(pInFF->inPIC),&tmpPIC);
                pDstPIC = &(pInFF->inPIC);
            }
//            fwrite(pDstPIC->Y,1,pDstPIC->w*pDstPIC->h,outyuv2);
//            fwrite(pDstPIC->U,1,pDstPIC->w*pDstPIC->h/4,outyuv2);
//            fwrite(pDstPIC->V,1,pDstPIC->w*pDstPIC->h/4,outyuv2);

            if((pOutFF->outPIC.w != pInFF->inPIC.w) || (pOutFF->outPIC.h != pInFF->inPIC.h))
            {
//                info("begin to scale\n");
                err = vfilter_yuv420p_scale(&(pOutFF->outPIC),&(pInFF->inPIC),libyuv::kFilterNone);
                if (err){err("Window scaler failed!");}
                else    {pDstPIC = &(pOutFF->outPIC);}
            }
        }
        chkp();
        pDstPIC->crop.x = 0;
        pDstPIC->crop.y = 0;
        pDstPIC->crop.w = pDstPIC->w;
        pDstPIC->crop.h = pDstPIC->h;

//        info("pDstPIC:[w,h] = [%d,%d]\n",pDstPIC->w,pDstPIC->h);

        //do encode job and write avpacket to output
        AVPacket avPkt;
        aup_vpkt_t outAupPkt;
        av_init_packet(&avPkt);
        AVPacketSideDataType sideDataType = AV_PKT_DATA_NEW_EXTRADATA;
        uint32_t extraDataLen = pOutFF->pVcodec->config.h264.extraDataLen;
        uint8_t* pExtraData = pOutFF->pVcodec->config.h264.extraData;

        int64_t  vframeDuration = AV_TIME_BASE/pOutFF->pCodecCtx[pOutFF->videoIndex]->framerate.num;
        uint8_t *sideData = av_packet_new_side_data(&avPkt, sideDataType, extraDataLen);
        if(sideData)
        {
            memcpy(sideData,pExtraData,extraDataLen);

            if(pOutFF->running)
            {
                chkp();
                err = aup_venc_frame(pOutFF->pVcodec,pDstPIC,&outAupPkt);
                if(!err)
                {
                    chkp();
                    copy_aupvpkt_to_avpacket(&avPkt,&outAupPkt);
                    avPkt.stream_index = pOutFF->videoIndex;
                    chkp();
                    avPkt.duration = av_rescale_q(vframeDuration,AVRational{1, AV_TIME_BASE},vStream->time_base);
                    avPkt.pts = av_rescale_q((int64_t)pInAupPkt->pts,AVRational{1, AV_TIME_BASE},vStream->time_base);
                    avPkt.dts = avPkt.pts;
                    chkp();
                    info("avPkt.dts = %lld\n",avPkt.pts);
//                    avPkt.dts = 0;
                    avPkt.pos = -1;
                    avPkt.flags = (NAL_SLICE_IDR == aup_vcodec_h264_nal_type(outAupPkt.p,outAupPkt.len)) ? 1:0;

                    if(avPkt.size > 0)
                    {
                        /* mux encoded frame */
                        err = av_write_frame(pOutFF->pCtx, &avPkt);
                        if(err)
                        {
                            av_log(NULL,AV_LOG_ERROR, "Error muxing video packet\n");
                        }
                        av_packet_unref(&avPkt);
                    }
                    aup_venc_outbuf_return(pOutFF->pVcodec, &outAupPkt);
                }
            }
        }
        aup_vdec_outbuf_return(pInFF->pVcodec,&tmpPIC);
        av_packet_free_side_data(&avPkt);
    }
    return 0;
}

int32_t transcode_process_audio(transcode_factory_t* pTranscodeFactory,AVPacket* pAVPkt)
{
    if (pTranscodeFactory && pAVPkt)
    {
        int32_t ret = -1;
        input_ff_t* pInFF = &(pTranscodeFactory->input[0]);
        output_ff_t* pOutFF = &(pTranscodeFactory->output[0]);

        if(pOutFF->audioIndex < 0) {return 0;} //no need to  process audio

        AVCodecContext* pCodecCtx = NULL;
        AVPacket outAvPkt;
        AVFrame* pSrcFrame = av_frame_alloc();
        AVFrame* pDstFrame = av_frame_alloc();
        pCodecCtx = pOutFF->pCodecCtx[pOutFF->audioIndex];
        if (NULL == pCodecCtx) {return -1;}
        ff_swr_factory_t* pFFSwrFactory = pTranscodeFactory->pFFSwrFactory[pOutFF->id];
        if (NULL == pFFSwrFactory) {return -1;}
        int32_t outDataSize = pCodecCtx->frame_size*pCodecCtx->channels*pOutFF->audioSampleBytes;
        pDstFrame->data[0] = (uint8_t*)malloc(outDataSize);
        if (NULL == pDstFrame->data[0]) {return -1;}

        chkp();
        AVRational ctxOutTimeBase = pOutFF->pCtx->streams[pOutFF->audioIndex]->time_base;
        if(pInFF->running)
        {
            //decode audio packet
            pCodecCtx = pInFF->pCodecCtx[pInFF->audioIndex];
            if(pCodecCtx)
            {
                //decoded to a frame
                if (!avcodec_send_packet(pCodecCtx, pAVPkt))
                {
                    if(!avcodec_receive_frame(pCodecCtx, pSrcFrame))
                    {
                        chkp();
                        do_audio_swr_convert(pFFSwrFactory,pSrcFrame);
                        int32_t pcmUnitNum = fifo_count(pFFSwrFactory->sampleFifo)/outDataSize;
                        int32_t sampleSize = (pFFSwrFactory->dstChannels*pFFSwrFactory->dstSampleBytes);
                        int64_t afifoBufDuration = (fifo_count(pFFSwrFactory->sampleFifo)/sampleSize)*ctxOutTimeBase.den/pFFSwrFactory->dstSampleRate;
                        int64_t bottomPts = pAVPkt->pts - afifoBufDuration;
                        info("pcmUnitNum = %d,outDataSize = %d,pAVPkt->pts = %lld\n",pcmUnitNum,outDataSize,pAVPkt->pts);
                        for(int32_t i = 0;i < pcmUnitNum;i++)
                        {
                            chkp();
                            pCodecCtx = pOutFF->pCodecCtx[pOutFF->audioIndex];
                            pDstFrame->channels = pCodecCtx->channels;
                            pDstFrame->format = pCodecCtx->sample_fmt;
                            pDstFrame->nb_samples = pCodecCtx->frame_size; //pDstFrame->nb_samples must equal to pFFSwrFactory.dstSamples
                            memset(pDstFrame->data[0],0,outDataSize);

                            if(av_sample_fmt_is_planar(pCodecCtx->sample_fmt))
                            {
                                //here may need to fix,for now,afilter mix limit sample format to only non-planar
                            }
                            else
                            {
                                ret = fifo_out_batch(pFFSwrFactory->sampleFifo,pFFSwrFactory->p,outDataSize);
                                if(ret == outDataSize)
                                {
                                    memcpy(pDstFrame->data[0],pFFSwrFactory->p,outDataSize);
                                }
                            }
                            chkp();

                            //encode audio frame and write to output
                            if(!avcodec_send_frame(pCodecCtx,pDstFrame))
                            {
                                av_init_packet(&outAvPkt);
                                if(!avcodec_receive_packet(pCodecCtx,&outAvPkt))
                                {
                                    chkp();
                                    outAvPkt.stream_index = pOutFF->audioIndex;
                                    bottomPts += pFFSwrFactory->dstFrameDuration;
                                    outAvPkt.pts = bottomPts;
                                    outAvPkt.duration = pFFSwrFactory->dstFrameDuration;
                                    outAvPkt.dts = outAvPkt.pts;
                                    outAvPkt.pos = -1;
                                    /* mux encoded frame */
                                    if(outAvPkt.size > 0)
                                    {
                                        ret = av_write_frame(pOutFF->pCtx, &outAvPkt);
                                        if(ret)
                                        {
                                            av_log(NULL,AV_LOG_ERROR, "Error muxing audio packet\n");
                                        }
                                    }
                                }
                                chkp();
                                av_packet_unref(&outAvPkt);
                            }
                        }
                        av_frame_unref(pSrcFrame);
                    }
                    av_packet_unref(pAVPkt);
                }
                else
                {
                    err("adecoder send one packet failed\n");
                }
            }
        }
        av_frame_free(&pSrcFrame);
        av_frame_free(&pDstFrame);
    }

    return 0;
}

int32_t transcode_factory_launch(transcode_factory_t* pTranscodeFactory)
{
    if (NULL == pTranscodeFactory) {return -1;}

    AVPacket avPkt;
    input_ff_t* pInFF = &(pTranscodeFactory->input[0]);
    AVFormatContext *pFmtCtx = pInFF->pCtx;
    int32_t ret = -1;
    int32_t pkt_index = 0;
    int32_t videoIndex = pInFF->videoIndex;
    int32_t audioIndex = pInFF->audioIndex;

    aup_vpkt_t inputVpkt;
    inputVpkt.pts = 0;
    inputVpkt.size = 1024*1024;  //1M
    inputVpkt.p = (uint8_t *)aup_vdec_malloc(pInFF->pVcodec,inputVpkt.size);
    if (NULL == inputVpkt.p)
    {
        transcode_factory_close(pTranscodeFactory);
        return -1;
    }

    memset(inputVpkt.p,0,inputVpkt.size);

    AVFrame* pFrame = av_frame_alloc();
    if (NULL == pFrame)
    {
        transcode_factory_close(pTranscodeFactory);
        return -1;
    }

    while(pTranscodeFactory->running)
    {
        ret = av_read_frame(pFmtCtx,&avPkt);
        if (ret < 0)
        {
            //err("[%d]Error reading input packet,ret=%08x\n",pIn->id,ret);
            av_packet_unref(&avPkt);
            usleep(5000);
            continue;
        }

        //if packet do not contain PTS infomation,then write pts
        if((avPkt.pts == AV_NOPTS_VALUE) && (videoIndex >= 0))
        {
            dbg("[%d]pkt.pts == AV_NOPTS_VALUE\n",pIn->id);
            AVRational time_base1 = pFmtCtx->streams[videoIndex]->time_base;
            int64_t calc_duration = (double)AV_TIME_BASE/av_q2d(pFmtCtx->streams[videoIndex]->r_frame_rate);
            avPkt.pts = (double)(pkt_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            avPkt.dts = avPkt.pts;
            pkt_index++;
            avPkt.duration = (double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
            info("input pkt did not contain pts,write pts manual\n");
        }

        if(avPkt.stream_index == videoIndex)
        {
            copy_avpacket_to_aupvpkt(&inputVpkt,&avPkt);
            inputVpkt.dts = 0; //make decoder decode frame immediately
            inputVpkt.pts = av_rescale_q(avPkt.pts,pFmtCtx->streams[videoIndex]->time_base,AVRational{1, AV_TIME_BASE});
            info("avPkt.src.pts = %lld\n",avPkt.pts);
            transcode_process_video(pTranscodeFactory,&inputVpkt);
        }
        else if(avPkt.stream_index == audioIndex)
        {
            transcode_process_audio(pTranscodeFactory,&avPkt);
        }
        av_packet_unref(&avPkt);
        usleep(1000);
    }
    aup_vdec_free(pInFF->pVcodec,inputVpkt.p);
    av_frame_free(&pFrame);
    return 0;
}
