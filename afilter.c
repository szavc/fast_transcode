#include "transcode_test.h"

int32_t do_audio_swr_convert(ff_swr_factory_t* pFFSwrFactory,AVFrame* pFrame)
{
    if (NULL == pFFSwrFactory) {return -1;}
     if(NULL == pFFSwrFactory->pSwrCtx) {return -1;}
     int32_t ret;

     /* compute destination number of samples */
      pFFSwrFactory->dstSamples = av_rescale_rnd(swr_get_delay(pFFSwrFactory->pSwrCtx, pFFSwrFactory->srcSampleRate) + pFFSwrFactory->srcSamples,\
                                                 pFFSwrFactory->dstSampleRate, pFFSwrFactory->srcSampleRate, AV_ROUND_UP);

     if (pFFSwrFactory->dstSamples > pFFSwrFactory->maxDstSamples)
     {
         av_freep(&(pFFSwrFactory->ppDstData[0]));
         int32_t dstLinesize;
         ret = av_samples_alloc(pFFSwrFactory->ppDstData, &dstLinesize, pFFSwrFactory->dstChannels,
                                pFFSwrFactory->dstSamples, pFFSwrFactory->dstSampleFmt, 1);
         if (ret < 0) {return -1;}
         pFFSwrFactory->dstLinesize = dstLinesize;
         pFFSwrFactory->maxDstSamples = pFFSwrFactory->dstSamples;
     }

     uint8_t** ppData = pFFSwrFactory->ppDstData;    //short cut
    /* convert to destination format */
    ret = swr_convert(pFFSwrFactory->pSwrCtx, ppData,pFFSwrFactory->dstSamples, \
                      (const uint8_t **)pFrame->data, pFFSwrFactory->srcSamples);
    if (ret < 0)
    {
        err("Error while swr_convert");
        return -1;
    }

    uint32_t dstDataLen = av_samples_get_buffer_size(&(pFFSwrFactory->dstLinesize), pFFSwrFactory->dstChannels,
                                             ret, pFFSwrFactory->dstSampleFmt, 1);

    if(!fifo_is_full(pFFSwrFactory->sampleFifo))
    {
        fifo_in_batch(pFFSwrFactory->sampleFifo,ppData[0],dstDataLen);
    }
    else
    {
        info("sampleFifo is full\n");
    }

    return dstDataLen;
}
