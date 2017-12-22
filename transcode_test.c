#include "transcode_test.h"
/****************************************************************************
* NAME:         transcode_test.c
*----------------------------------------------------------------------------
* Copyright (c) auperastor.com
*----------------------------------------------------------------------------
* RELEASE:
* REVISION:
*----------------------------------------------------------------------------
* PURPOSE:
*  transcode test code - input module and output module with aupcodec api and ffmpeg api
*----------------------------------------------------------------------------
* HISTORY:
*        Date               Author      Modifications
*        ----               -----       -----------
*        2017-Dec-20        xuejin      Birth
*****************************************************************************/

static void _help(void)
{
    printf("**************************************************************************************************\n");
    printf("transcode_test [input url] [output url] [outw] [fps] [bitrate] [gop]\n");
    printf("Arguments:\n");
    printf("    input url:  \"url\"\n");
    printf("    output url: \"url0[|url1[|url2...]]\"\n");
    printf("    outw:       output width\n");
    printf("    fps:        output framerate\n");
    printf("    bitrate:    output bitrate, set 0 tell mixer to use fix qp mode\n");
    printf("    gop:        output gop size, set 0 tell mixer to use default gop size\n");
    printf("\n");
    printf("Example:\n");
    printf("transcode_test \"rtmp://10.53.170.99:1935/live/001\" \"rtmp://10.53.170.99:1935/live/out\" 576 15 1024000 15\\\n");
    printf("\n******************************************************************************************************\n");

}

int32_t parse_output_url(char *argv, char **ppUrl,int32_t maxEntries)
{
    int32_t cnt = 0;
    char* pTmp = NULL;
    if ((argv)&&(ppUrl))
    {
        //remove quote symbol
        char* pStart = strchr(argv,'"');
        if (pStart)
        {
            pStart++;
            pTmp = strchr(pStart,'"');
            if (pTmp) {*pTmp = 0;}
        }
        else
        {
            pStart = argv;
        }
        //store every url
        pTmp = strtok(pStart,"|");
        while(pTmp)
        {
           ppUrl[cnt] = pTmp;
           pTmp = strtok(NULL,"|");
           cnt++;
           if (cnt >= maxEntries)
           {
                break;
           }
        }
    }
    return cnt;
}

int main(int argc,char** argv)
{
    if(argc < 3)
    {
        _help();
        exit(1);
    }
    int i = 1;
    char *inputUrl = argv[i];

    i++;
    char *pOutputUrl[TRANSCODE_MAX_OUTPUT];
    int32_t numOfOutput = parse_output_url(argv[i],pOutputUrl,TRANSCODE_MAX_OUTPUT);

    i++;
    int32_t outWidth = 0;
    if (argc > i) {outWidth = atoi(argv[i]);}

    i++;
    int32_t outFramerate = 0;
    if (argc > i) {outFramerate = atoi(argv[i]);}

    i++;
    int32_t outBitrate = 0;
    if (argc > i) {outBitrate = atoi(argv[i]);}

    i++;
    int32_t outGop = 0;
    if (argc > i) {outGop = atoi(argv[i]);}

    //init ffmpeg,register codec and format components
    av_register_all();
    avcodec_register_all();
    avformat_network_init();

    //init libaupcodec
    libaupera_init("/tmp/stream_mixer.conf");
    libaupcodec_init();

//    for(int32_t i = 0;i < 100;i++)
//    {
//        transcode_factory_t* pTranscodeFactory = transcode_factory_open(inputUrl,pOutputUrl,numOfOutput,outWidth,outFramerate,outBitrate,outGop);
//        info("************************[%d]open transcode factory done************************\n",i);
//        transcode_factory_close(pTranscodeFactory);
//        info("************************[%d]close transcode factory done************************\n",i);
//    }

//    //test code,open encoder and close it
//    for(int32_t i = 0;i < 100;i++)
//    {
//        aup_vcodec_config_t config;
//        memset(&config,0,sizeof(config));
//        config.codecType = AUP_VCODEC_H264_V108;
//        config.outputPixelFormat = AUP_VCODEC_PIXFMT_YUV420P;
//        config.inputWidth = 576;//(width%16 == 0) ? width:((width/16 + 1)*16);
//        config.inputHeight = 1024;
//    //    config.inputLineSize = (width%16 == 0) ? width:((width/16 + 1)*16);
//        config.inputFrameRate = 15;
//        config.outputWidth =  576;//(width%16 == 0) ? width:((width/16 + 1)*16);  //make it aligned to 16
//        config.outputHeight = 1024;
//        info("config.in[w,h] = [%d,%d],config.out[w,h] = [%d,%d]\n",config.inputWidth,config.inputHeight,config.outputWidth,config.outputHeight);
//        config.outputFrameRate = 15;
//        config.outputBitRate = 1024000;

//        //h264
//        if (aup_vcodec_is_h264(config.codecType))
//        {
//            aup_venc_h264_config_default(&config);
//            config.h264.gopSize = 12;
//        }
//        else
//        {
//            err("Not support yet!");
//        }

//        aup_vcodec_t* pVcodec = aup_venc_open(&config);
//        info("************************[%d]open encoder done************************\n",i);
//        if(pVcodec) {aup_venc_close(pVcodec);}
//        info("************************[%d]pVcodec addr = %p,close encoder done************************\n",i,pVcodec);
//    }
//    return 0;

    transcode_factory_t* pTranscodeFactory = transcode_factory_open(inputUrl,pOutputUrl,numOfOutput,outWidth,outFramerate,outBitrate,outGop);
    if(pTranscodeFactory)
    {
        transcode_factory_launch(pTranscodeFactory);
        transcode_factory_close(pTranscodeFactory);
    }
    else
    {
        err("open transcode factory failed\n");
    }

    return 0;
}
