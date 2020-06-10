/*
 * Copyright (C) 2016 Rockchip Electronics Co.Ltd
 * Authors:
 *	PutinLee <putin.lee@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include "im2d.h"
#include "im2d_cpp.h"

#include <RockchipRga.h>
#include "normal/NormalRga.h"
#include <sstream>
#include <ui/GraphicBuffer.h>

#define ALIGN(val, align) (((val) + ((align) - 1)) & ~((align) - 1))

using namespace android;
using namespace std;

RockchipRga& rkRga(RockchipRga::get());

IM_API buffer_t warpbuffer_virtualaddr(void* vir_addr, int width, int height, int wstride, int hstride, int format)
{
    buffer_t buffer;

    memset(&buffer, 0, sizeof(buffer_t));

    buffer.vir_addr = vir_addr;
    buffer.width = width;
    buffer.height = height;
    buffer.wstride = wstride;
    buffer.hstride = hstride;
    buffer.format = format;

    return buffer;
}

IM_API buffer_t warpbuffer_physicaladdr(void* phy_addr, int width, int height, int wstride, int hstride, int format)
{
    buffer_t buffer;

    memset(&buffer, 0, sizeof(buffer_t));

    buffer.phy_addr = phy_addr;
    buffer.width = width;
    buffer.height = height;
    buffer.wstride = wstride;
    buffer.hstride = hstride;
    buffer.format = format;

    return buffer;
}

IM_API buffer_t warpbuffer_fd(int fd, int width, int height, int wstride, int hstride, int format)
{
    buffer_t buffer;

    memset(&buffer, 0, sizeof(buffer_t));

    buffer.fd = fd;
    buffer.width = width;
    buffer.height = height;
    buffer.wstride = wstride;
    buffer.hstride = hstride;
    buffer.format = format;

    return buffer;
}

#if 1 //Android
IM_API buffer_t warpbuffer_GraphicBuffer(sp<GraphicBuffer> buf)
{
    buffer_t buffer;
    int ret = 0;

    memset(&buffer, 0, sizeof(buffer_t));

    ret = rkRga.RkRgaGetBufferFd(buf->handle, &buffer.fd);
    if (ret)
        ALOGE("rga_im2d: get buffer fd fail: %s, hnd=%p", strerror(errno), (void*)(buf->handle));

    if (buffer.fd <= 0)
    {
        ret = RkRgaGetHandleMapAddress(buf->handle, &buffer.vir_addr);
        if(!buffer.vir_addr)
            ALOGE("rga_im2d: invaild GraphicBuffer");
    }

    buffer.width   = buf->getWidth();
    buffer.height  = buf->getHeight();
    buffer.wstride = buf->getStride();
    buffer.hstride = buf->getHeight();
    buffer.format  = buf->getPixelFormat();

    return buffer;
}
#endif

IM_API int rga_set_buffer_info(buffer_t dst, rga_info_t* dstinfo)
{
    if(dst.phy_addr != NULL)
        dstinfo->phyAddr= dst.phy_addr;
    else if(dst.fd > 0)
    {
        dstinfo->fd = dst.fd;
        dstinfo->mmuFlag = 1;
    }
    else if(dst.vir_addr != NULL)
    {
        dstinfo->virAddr = dst.vir_addr;
        dstinfo->mmuFlag = 1;
    }
    else
    {
        ALOGE("rga_im2d: invaild dst buffer");
        return IM_STATUS_INVALID_PARAM;
    }

    return IM_STATUS_SUCCESS;
}

IM_API int rga_set_buffer_info(const buffer_t src, buffer_t dst, rga_info_t* srcinfo, rga_info_t* dstinfo)
{
    if(src.phy_addr != NULL)
        srcinfo->phyAddr = src.phy_addr;
    else if(src.fd > 0)
    {
        srcinfo->fd = src.fd;
        srcinfo->mmuFlag = 1;
    }
    else if(src.vir_addr != NULL)
    {
        srcinfo->virAddr = src.vir_addr;
        srcinfo->mmuFlag = 1;
    }
    else
    {
        ALOGE("rga_im2d: invaild src buffer");
        return IM_STATUS_INVALID_PARAM;
    }

    if(dst.phy_addr != NULL)
        dstinfo->phyAddr= dst.phy_addr;
    else if(dst.fd > 0)
    {
        dstinfo->fd = dst.fd;
        dstinfo->mmuFlag = 1;
    }
    else if(dst.vir_addr != NULL)
    {
        dstinfo->virAddr = dst.vir_addr;
        dstinfo->mmuFlag = 1;
    }
    else
    {
        ALOGE("rga_im2d: invaild dst buffer");
        return IM_STATUS_INVALID_PARAM;
    }

    return IM_STATUS_SUCCESS;
}

IM_API const char* querystring(int name)
{
    bool all_output = 0, all_output_prepared = 0;
    char buf[16];
    int rgafd, rga_version = 0;
    const char *temp;
    const char *output_vendor = "Rockchip Electronics Co.,Ltd.";
    const char *output_name[] = {
        "RGA vendor : ",
        "RGA vesion : ",
        "Max input  : ",
        "Max output : ",
        "Scale limit: ",
        "Input support format : ",
        "output support format: "
    };
    const char *output_version[] = {
        "unknown",
        "RGA_1",
        "RGA_1_plus",
        "RGA_2",
        "RGA_2_lite0",
        "RGA_2_lite1",
        "RGA_2_Enhance"
    };
    const char *output_resolution[] = {
        "unknown",
        "2048x2048",
        "4096x4096",
        "8192x8192"
    };
    const char *output_scale_limit[] = {
        "unknown",
        "0.125 ~ 8",
        "0.0625 ~ 16"
    };
    const char *output_format[] = {
        "unknown",
        "RGBA_8888 RGBA_4444 RGBA_5551 RGB_565 RGB_888 ",
        "BPP8 BPP4 BPP2 BPP1 ",
        "YUV420/YUV422 ",
        "YUV420_10bit/YUV422_10bit ",
        "YUYV ",
        "YUV400/Y4 "
    };
    ostringstream out;
    string info;

    /*open /dev/rga node in order to get rga vesion*/
    rgafd = open("/dev/rga", O_RDWR, 0);
    if (rgafd < 0)
    {
        ALOGE("rga_im2d: failed to open /dev/rga: %s.",strerror(errno));
        return "err";
    }
    if (ioctl(rgafd, RGA_GET_VERSION, buf))
    {
        ALOGE("rga_im2d: rga get version fail: %s",strerror(errno));
        return "err";
    }
    if (strncmp(buf,"1.3",3) == 0)
        rga_version = RGA_1;
    else if (strncmp(buf,"1.6",3) == 0)
        rga_version = RGA_1_PLUS;
    else if (strncmp(buf,"2.00",4) == 0) //3288 vesion is 2.00
        rga_version = RGA_2;
    else if (strncmp(buf,"3.00",4) == 0) //3288w version is 3.00
        rga_version = RGA_2;
    else if (strncmp(buf,"3.02",4) == 0)
        rga_version = RGA_2_ENHANCE;
    else if (strncmp(buf,"4.00",4) == 0)
        rga_version = RGA_2_LITE0;
    else if (strncmp(buf,"4.00",4) == 0)
        rga_version = RGA_2_LITE1;
    else
        rga_version = RGA_V_ERR;

    close(rgafd);

    do{
        switch(name)
        {
            case RGA_VENDOR :
                    out << output_name[name] << output_vendor << endl;
                break;

            case RGA_VERSION :
                switch(rga_version)
                {
                    case RGA_1 :
                        out << output_name[name] << output_version[RGA_1] << endl;
                        break;
                    case RGA_1_PLUS :
                        out << output_name[name] << output_version[RGA_1_PLUS] << endl;
                        break;
                    case RGA_2 :
                        out << output_name[name] << output_version[RGA_2] << endl;
                        break;
                    case RGA_2_LITE0 :
                        out << output_name[name] << output_version[RGA_2_LITE0] << endl;
                        break;
                    case RGA_2_LITE1 :
                        out << output_name[name] << output_version[RGA_2_LITE1] << endl;
                        break;
                    case RGA_2_ENHANCE :
                        out << output_name[name] << output_version[RGA_2_ENHANCE] << endl;
                        break;
                    case RGA_V_ERR :
                        out << output_name[name] << output_version[RGA_V_ERR] << endl;
                        break;
                    default:
                        return "err";
                }
                break;

            case RGA_MAX_INPUT :
                switch(rga_version)
                {
                    case RGA_1 :
                    case RGA_1_PLUS :
                    case RGA_2 :
                    case RGA_2_LITE0 :
                    case RGA_2_LITE1 :
                    case RGA_2_ENHANCE :
                        out << output_name[name] << output_resolution[3] << endl;
                        break;
                    case RGA_V_ERR :
                        out << output_name[name] << output_resolution[0] << endl;
                        break;
                    default:
                        return "err";
                }
                break;

            case RGA_MAX_OUTPUT :
                switch(rga_version)
                {
                    case RGA_1 :
                    case RGA_1_PLUS :
                        out << output_name[name] << output_resolution[1] << endl;
                        break;
                    case RGA_2 :
                    case RGA_2_LITE0 :
                    case RGA_2_LITE1 :
                    case RGA_2_ENHANCE :
                        out << output_name[name] << output_resolution[2] << endl;
                        break;
                    case RGA_V_ERR :
                        out << output_name[name] << output_resolution[0] << endl;
                        break;
                    default:
                        return "err";
                }
                break;

            case RGA_SCALE_LIMIT :
                switch(rga_version)
                {
                    case RGA_1 :
                    case RGA_1_PLUS :
                    case RGA_2_LITE0 :
                    case RGA_2_LITE1 :
                        out << output_name[name] << output_scale_limit[1] << endl;
                    case RGA_2 :
                    case RGA_2_ENHANCE :
                        out << output_name[name] << output_scale_limit[2] << endl;
                        break;
                    case RGA_V_ERR :
                        out << output_name[name] << output_resolution[0] << endl;
                        break;
                    default:
                        return "err";
                }
                break;

            case RGA_INPUT_FORMAT :
                switch(rga_version)
                {
                    case RGA_1 :
                    case RGA_1_PLUS :
                        out << output_name[name] << output_format[1] << output_format[2] << output_format[3] << endl;
                    case RGA_2 :
                    case RGA_2_LITE0 :
                        out << output_name[name] << output_format[1] << output_format[3] << endl;
                        break;
                    case RGA_2_LITE1 :
                    case RGA_2_ENHANCE :
                        out << output_name[name] << output_format[1] << output_format[3] << output_format[4] << endl;
                        break;
                    case RGA_V_ERR :
                        out << output_name[name] << output_format[0] << endl;
                        break;
                    default:
                        return "err";
                }
                break;

            case RGA_OUTPUT_FORMAT :
                switch(rga_version)
                {
                    case RGA_1 :
                        out << output_name[name] << output_format[1] << output_format[3] << endl;
                        break;
                    case RGA_1_PLUS :
                        out << output_name[name] << output_format[1] << output_format[3] << endl;
                        break;
                    case RGA_2 :
                        out << output_name[name] << output_format[1] << output_format[3] << endl;
                        break;
                    case RGA_2_LITE0 :
                        out << output_name[name] << output_format[1] << output_format[3] << endl;
                        break;
                    case RGA_2_LITE1 :
                        out << output_name[name] << output_format[1] << output_format[3] << output_format[4] << endl;
                        break;
                    case RGA_2_ENHANCE :
                        out << output_name[name] << output_format[1] << output_format[3] << output_format[4] << output_format[5] << endl;
                        break;
                    case RGA_V_ERR :
                        out << output_name[name] << output_format[0] << endl;
                        break;
                    default:
                        return "err";
                }
                break;

            case RGA_ALL :
                if (!all_output)
                {
                    all_output = 1;
                    name = 0;
                }
                else
                    all_output_prepared = 1;
                break;

            default:
                return "err";
        }

        info = out.str();

        if (all_output_prepared)
            break;
        else if (all_output && strcmp(info.c_str(),"0")>0)
            name++;

    }while(all_output);

    temp = info.c_str();

    return temp;
}

IM_API IM_STATUS imresize_t(const buffer_t src, buffer_t dst, double fx, double fy, int interpolation, int sync)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    if (fx > 0 || fy > 0)
    {
        if (fx == 0) fx = 1;
        if (fy == 0) fy = 1;

        dst.width = (int)(src.width * fx);
        dst.height = (int)(src.height * fy);

        if(NormalRgaIsYuvFormat(RkRgaGetRgaFormat(src.format)))
            dst.width = ALIGN(dst.width, 2);
    }

    rga_set_rect(&srcinfo.rect, 0, 0, src.width, src.height, src.wstride, src.hstride, src.format);
    rga_set_rect(&dstinfo.rect, 0, 0, dst.width, dst.height, dst.wstride, dst.hstride, dst.format);

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imcrop_t(const buffer_t src, buffer_t dst, im_rect rect, int sync)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    if ((rect.width + rect.x > src.width) || (rect.height + rect.y > src.height))
    {
        ALOGE("rga_im2d: invaild rect");
        return IM_STATUS_INVALID_PARAM;
    }

    rga_set_rect(&srcinfo.rect, rect.x, rect.y, rect.width, rect.height, src.wstride, src.hstride, src.format);
    rga_set_rect(&dstinfo.rect, 0, 0, rect.width, rect.height, dst.wstride, dst.hstride, dst.format);

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imrotate_t(const buffer_t src, buffer_t dst, int rotation, int sync)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    srcinfo.rotation = rotation;

    rga_set_rect(&srcinfo.rect, 0, 0, src.width, src.height, src.wstride, src.hstride, src.format);
    rga_set_rect(&dstinfo.rect, 0, 0, dst.width, dst.height, dst.wstride, dst.hstride, dst.format);

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imflip_t (const buffer_t src, buffer_t dst, int mode, int sync)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    srcinfo.rotation = mode;

    rga_set_rect(&srcinfo.rect, 0, 0, src.width, src.height, src.wstride, src.hstride, src.format);
    rga_set_rect(&dstinfo.rect, 0, 0, dst.width, dst.height, dst.wstride, dst.hstride, dst.format);

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imfill_t(buffer_t dst, im_rect rect, unsigned char color, int sync)
{
    rga_info_t dstinfo;
    int ret;

    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(dst, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    dstinfo.color = color;

    if ((rect.width + rect.x > dst.width) || (rect.height + rect.y > dst.height))
    {
        ALOGE("rga_im2d: invaild rect");
        return IM_STATUS_INVALID_PARAM;
    }

    rga_set_rect(&dstinfo.rect, rect.x, rect.y, rect.width, rect.height, dst.wstride, dst.hstride, dst.format);

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaCollorFill(&dstinfo);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imtranslate_t(const buffer_t src, buffer_t dst, int x, int y, int sync)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    rga_set_rect(&srcinfo.rect, 0, 0, src.width - x, src.height - y, src.wstride, src.hstride, src.format);
    rga_set_rect(&dstinfo.rect, x, y, src.width - x, src.height - y, dst.wstride, dst.hstride, dst.format);

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imcopy_t(const buffer_t src, buffer_t dst, int sync)
{
    int usage = 0;
    int ret = IM_STATUS_SUCCESS;
    im_rect srect;
    im_rect drect;

    if ((src.width != dst.width) || (src.height != dst.height))
        return IM_STATUS_INVALID_PARAM;

    if (sync == 0)
        usage |= IM_SYNC;

    ret = improcess(src, dst, srect, drect, usage);
    if (!ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imblend_t(const buffer_t srcA, const buffer_t srcB, buffer_t dst, int mode, int sync)
{
    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imcvtcolor_t(const buffer_t src, buffer_t dst, int sfmt, int dfmt, int mode, int sync)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    if (src.width != dst.width || src.height != dst.height)
        return IM_STATUS_INVALID_PARAM;

    rga_set_rect(&srcinfo.rect, 0, 0, src.width, src.height, src.wstride, src.hstride, sfmt);
    rga_set_rect(&dstinfo.rect, 0, 0, dst.width, dst.height, dst.wstride, dst.hstride, dfmt);

    if (mode > 0)
    {
        dstinfo.color_space_mode = mode;
    }

    if (sync == 0)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imquantize_t(const buffer_t src, buffer_t dst, rga_nn_t nn_info, int sync)
{
    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS improcess(const buffer_t src, buffer_t dst, im_rect srect, im_rect drect, int usage)
{
    rga_info_t srcinfo;
    rga_info_t dstinfo;
    int ret;

    memset(&srcinfo, 0, sizeof(rga_info_t));
    memset(&dstinfo, 0, sizeof(rga_info_t));

    ret = rga_set_buffer_info(src, dst, &srcinfo, &dstinfo);
    if (ret < 0)
        return IM_STATUS_INVALID_PARAM;

    rga_set_rect(&srcinfo.rect, 0, 0, src.width, src.height, src.wstride, src.hstride, src.format);
    rga_set_rect(&dstinfo.rect, 0, 0, dst.width, dst.height, dst.wstride, dst.hstride, dst.format);

    if (dst.color_space_mode > 0)
    {
        dstinfo.color_space_mode = dst.color_space_mode;
    }

    if (usage & IM_SYNC)
        dstinfo.sync_mode = RGA_BLIT_ASYNC;

    ret = rkRga.RkRgaBlit(&srcinfo, &dstinfo, NULL);
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}

IM_API IM_STATUS imsync(void)
{
    int ret = 0;
    ret = rkRga.RkRgaFlush();
    if (ret)
        return IM_STATUS_FAILED;

    return IM_STATUS_SUCCESS;
}
