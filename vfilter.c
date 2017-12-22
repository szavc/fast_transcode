#include "transcode_test.h"

using namespace libyuv;


int32_t vfilter_yuv420p_simple_crop(aup_vpic_t* pDstPic,aup_vpic_t* pSrcPic)
{
    if(pDstPic && pSrcPic)
    {
//        info("pDstPic[w,h] = [%d,%d]\n",pDstPic->w,pDstPic->h);
//        info("pSrcPic[w,h] = [%d,%d],crop[x,y,w,h] = [%d,%d,%d,%d] \n",pSrcPic->w,pSrcPic->h,pSrcPic->crop.x,pSrcPic->crop.y,pSrcPic->crop.w,pSrcPic->crop.h);

        if(pDstPic->Y && pSrcPic->Y && (pSrcPic->crop.w == pDstPic->w) && (pSrcPic->crop.h == pDstPic->h))
        {
            uint8_t* pDstBuf = NULL;
            uint8_t* pSrcBuf = NULL;
            int32_t  line;
            //for Y
            pDstBuf = pDstPic->Y;
            pSrcBuf = pSrcPic->Y + pSrcPic->crop.y * pSrcPic->w + pSrcPic->crop.x;
            for(line = 0;line < pSrcPic->crop.h;line++)
            {
                memcpy(pDstBuf,pSrcBuf,pSrcPic->crop.w);
                pDstBuf += pSrcPic->crop.w;
                pSrcBuf += pSrcPic->w;
            }

            //for U
            pDstBuf = pDstPic->U;
            pSrcBuf = pSrcPic->U + pSrcPic->crop.y * pSrcPic->w/4 + pSrcPic->crop.x/2;
            for(line = 0;line < pSrcPic->crop.h/2;line++)
            {
                memcpy(pDstBuf,pSrcBuf,pSrcPic->crop.w/2);
                pDstBuf += pSrcPic->crop.w/2;
                pSrcBuf += pSrcPic->w/2;
            }

            //for V
            pDstBuf = pDstPic->V;
            pSrcBuf = pSrcPic->V + pSrcPic->crop.y * pSrcPic->w/4 + pSrcPic->crop.x/2;
            for(line = 0;line < pSrcPic->crop.h/2;line++)
            {
                memcpy(pDstBuf,pSrcBuf,pSrcPic->crop.w/2);
                pDstBuf += pSrcPic->crop.w/2;
                pSrcBuf += pSrcPic->w/2;
            }
        }
    }
    return 0;
}


int32_t vfilter_yuv420p_scale(aup_vpic_t* pDstPic,aup_vpic_t* pSrcPic, LIBYUV_BOOL scaleMode)
{
    if(!pDstPic || !pSrcPic){return -1;}

    int ret;
    int src_y_stride,src_u_stride,src_v_stride,dst_y_stride,dst_u_stride,dst_v_stride;
    if(pSrcPic->w > 0 && pSrcPic->h > 0 && pDstPic->w > 0 && pDstPic->h > 0)
    {
       src_y_stride = pSrcPic->w;
       src_u_stride = pSrcPic->w/2;
       src_v_stride = pSrcPic->w/2;

       dst_y_stride = pDstPic->w;
       dst_u_stride = pDstPic->w/2;
       dst_v_stride = pDstPic->w/2;

       ret = libyuv::Scale(pSrcPic->Y,pSrcPic->U,pSrcPic->V,src_y_stride,src_u_stride,src_v_stride,pSrcPic->w,pSrcPic->h,  \
                     pDstPic->Y,pDstPic->U,pDstPic->V,dst_y_stride,dst_u_stride,dst_v_stride, pDstPic->w,pDstPic->h,scaleMode);
       return ret;
    }
    return -1;
}


//support more filter to process yuv420p picture
//......
