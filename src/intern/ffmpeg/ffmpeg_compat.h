/*
 * compatibility macros to make every ffmpeg installation appear
 * like the most current installation (wrapping some functionality sometimes)
 * it also includes all ffmpeg header files at once, no need to do it 
 * separately.
 *
 * Copyright (c) 2011 Peter Schlaile
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __FFMPEG_COMPAT_H__
#define __FFMPEG_COMPAT_H__

/* ============================================================
 * FFmpeg version detection - MUST COME FIRST
 * ============================================================ */
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <libswscale/version.h>

/* FFmpeg 5.0: LIBAVCODEC_MAJOR >= 59, LIBAVFORMAT_MAJOR >= 59 */
#if LIBAVCODEC_VERSION_MAJOR >= 59
#  define FFMPEG5
#endif
/* FFmpeg 4.0+: send/receive API */
#if LIBAVCODEC_VERSION_MAJOR >= 58
#  define FFMPEG_NEW_API
#endif
/* FFmpeg 3.0+: AVPicture deprecated in favor of AVFrame */
#if LIBAVCODEC_VERSION_MAJOR >= 57
#  define FFMPEG_NO_AVPICTURE
#endif

#include <libavformat/avformat.h>

#if (LIBAVFORMAT_VERSION_MAJOR < 52) || ((LIBAVFORMAT_VERSION_MAJOR == 52) && (LIBAVFORMAT_VERSION_MINOR <= 64))
#  error "FFmpeg 0.7 or newer is needed"
#endif

#ifdef _MSC_VER
#  define FFMPEG_INLINE static __inline
#else
#  define FFMPEG_INLINE static inline
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>

#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#  include <libavutil/parseutils.h>
#endif
#include <libswscale/swscale.h>

/* ============================================================
 * AVPicture compat: FFmpeg 3+ removed AVPicture entirely.
 * In both C and C++ we simply treat AVPicture as AVFrame.
 * ============================================================ */
#ifdef FFMPEG_NO_AVPICTURE
#  ifndef __cplusplus
#    define AVPicture AVFrame
#  else
typedef AVFrame AVPicture;
#  endif
#endif /* FFMPEG_NO_AVPICTURE */

/* ============================================================
 * FFmpeg 5+ const correctness for Codec/Format pointers.
 * avcodec_find_encoder/find_decoder return const AVCodec*,
 * av_guess_format returns const AVOutputFormat*.
 * Wrap them to maintain old non-const interface.
 * ============================================================ */
#ifndef FFMPEG5
static inline AVCodec *ffmpeg_avcodec_find_encoder(enum AVCodecID id) { return avcodec_find_encoder(id); }
static inline AVCodec *ffmpeg_avcodec_find_decoder(enum AVCodecID id) { return avcodec_find_decoder(id); }
#else
static inline AVCodec *ffmpeg_avcodec_find_encoder(enum AVCodecID id) { return (AVCodec *)avcodec_find_encoder(id); }
static inline AVCodec *ffmpeg_avcodec_find_decoder(enum AVCodecID id) { return (AVCodec *)avcodec_find_decoder(id); }
#endif
#define avcodec_find_encoder ffmpeg_avcodec_find_encoder
#define avcodec_find_decoder ffmpeg_avcodec_find_decoder

/* ============================================================
 * Removed helper functions - replaced with av_image_* wrappers
 * ============================================================ */
#ifndef avpicture_get_size
#  define avpicture_get_size(pix_fmt, width, height) \
     av_image_get_buffer_size(pix_fmt, width, height, 1)
#endif
#ifndef avpicture_fill
/* Note: in C mode, pic is AVFrame* due to macro; in C++ it's AVPicture* which is AVFrame* */
#  define avpicture_fill(pic, ptr, pix_fmt, width, height) \
     av_image_fill_arrays(((AVFrame *)(void *)(pic))->data, \
                          ((AVFrame *)(void *)(pic))->linesize, \
                          ptr, pix_fmt, width, height, 1)
#endif

/* ============================================================
 * Deprecated/removed functions
 * ============================================================ */
/* av_register_all removed in FFmpeg 4.x */
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(58, 9, 100)
#  define av_register_all() ((void)0)
#endif

/* av_free_packet -> av_packet_unref (FFmpeg 4+) */
#ifdef FFMPEG_NEW_API
#  define av_free_packet av_packet_unref
/* av_dup_packet: no-op in reference-counted world */
FFMPEG_INLINE int av_dup_packet_compat(AVPacket *pkt) { (void)pkt; return 0; }
#  define av_dup_packet av_dup_packet_compat
#endif

/* ============================================================
 * Encode/decode API compat: old avcodec_encode_video2 etc -> send/receive
 * ============================================================ */
#ifdef FFMPEG_NEW_API
/* avcodec_encode_video2 */
FFMPEG_INLINE int avcodec_encode_video2_compat(AVCodecContext *avctx, AVPacket *avpkt,
                                                const AVFrame *frame, int *got_packet)
{
    int ret;
    *got_packet = 0;
    if (frame) {
        ret = avcodec_send_frame(avctx, frame);
        if (ret < 0 && ret != AVERROR_EOF) return ret;
    }
    ret = avcodec_receive_packet(avctx, avpkt);
    if (ret >= 0) { *got_packet = 1; return 0; }
    if (ret == AVERROR(EAGAIN)) return 0;
    return ret;
}
#  define avcodec_encode_video2(avctx, avpkt, frame, got) avcodec_encode_video2_compat(avctx, avpkt, frame, got)

/* avcodec_encode_audio2 */
FFMPEG_INLINE int avcodec_encode_audio2_compat(AVCodecContext *avctx, AVPacket *avpkt,
                                                const AVFrame *frame, int *got_packet)
{
    int ret;
    *got_packet = 0;
    if (frame) {
        ret = avcodec_send_frame(avctx, frame);
        if (ret < 0 && ret != AVERROR_EOF) return ret;
    }
    ret = avcodec_receive_packet(avctx, avpkt);
    if (ret >= 0) { *got_packet = 1; return 0; }
    if (ret == AVERROR(EAGAIN)) return 0;
    return ret;
}
#  define avcodec_encode_audio2(avctx, avpkt, frame, got) avcodec_encode_audio2_compat(avctx, avpkt, frame, got)

/* avcodec_decode_video2 */
FFMPEG_INLINE int avcodec_decode_video2_compat(AVCodecContext *avctx, AVFrame *picture,
                                                int *got_picture, AVPacket *avpkt)
{
    int ret;
    *got_picture = 0;
    if (avpkt && avpkt->size > 0) {
        ret = avcodec_send_packet(avctx, avpkt);
        if (ret < 0 && ret != AVERROR_EOF) return ret;
    }
    ret = avcodec_receive_frame(avctx, picture);
    if (ret >= 0) { *got_picture = 1; return 0; }
    if (ret == AVERROR(EAGAIN)) return 0;
    return ret;
}
#  define avcodec_decode_video2(avctx, pic, got, pkt) avcodec_decode_video2_compat(avctx, pic, got, pkt)

/* avcodec_decode_audio4 */
FFMPEG_INLINE int avcodec_decode_audio4_compat(AVCodecContext *avctx, AVFrame *frame,
                                                int *got_frame, AVPacket *avpkt)
{
    int ret;
    *got_frame = 0;
    if (avpkt && avpkt->size > 0) {
        ret = avcodec_send_packet(avctx, avpkt);
        if (ret < 0 && ret != AVERROR_EOF) return ret;
    }
    ret = avcodec_receive_frame(avctx, frame);
    if (ret >= 0) { *got_frame = 1; return 0; }
    if (ret == AVERROR(EAGAIN)) return 0;
    return ret;
}
#  define avcodec_decode_audio4(avctx, frame, got, pkt) avcodec_decode_audio4_compat(avctx, frame, got, pkt)

#endif /* FFMPEG_NEW_API */

/* ============================================================
 * FF_MIN_BUFFER_SIZE: removed in FFmpeg 4+, provide default
 * ============================================================ */
#ifndef FF_MIN_BUFFER_SIZE
#  define FF_MIN_BUFFER_SIZE 16384
#endif

#ifndef FFMPEG_HAVE_ENCODE_AUDIO2
/* FFMPEG_DEF_OPT_VAL_INT / _DOUBLE macros for expert option defaults */
#  define FFMPEG_DEF_OPT_VAL_INT(o) ((o)->default_val.i64)
#  define FFMPEG_DEF_OPT_VAL_DOUBLE(o) ((o)->default_val.dbl)
#else
#  if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 0, 0)
#    define FFMPEG_DEF_OPT_VAL_INT(o) ((o)->default_val.i64)
#    define FFMPEG_DEF_OPT_VAL_DOUBLE(o) ((o)->default_val.dbl)
#  else
#    define FFMPEG_DEF_OPT_VAL_INT(o) ((int)((o)->default_val))
#    define FFMPEG_DEF_OPT_VAL_DOUBLE(o) ((double)((o)->default_val))
#  endif
#endif

/* ============================================================
 * avcodec_close wrapper: in FFmpeg 5+ we free the context
 * ============================================================ */
#ifdef FFMPEG5
FFMPEG_INLINE int avcodec_close_compat(AVCodecContext *avctx)
{
    avcodec_free_context(&avctx);
    return 0;
}
#  define avcodec_close(avctx) avcodec_close_compat(avctx)
#endif

/* ============================================================
 * avcodec_get_context_defaults3: removed in FFmpeg 5+
 * ============================================================ */
#ifdef FFMPEG5
#  define avcodec_get_context_defaults3(c, codec) avcodec_parameters_to_context((c), NULL)
#endif

/* ============================================================
 * av_update_cur_dts: uses cur_dts which is GONE in FFmpeg 5+
 * ============================================================ */
#ifdef FFMPEG5
FFMPEG_INLINE void av_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp)
{
    (void)s; (void)ref_st; (void)timestamp;
}
#else
#  if ((LIBAVFORMAT_VERSION_MAJOR > 53) || ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR > 32)) || ((LIBAVFORMAT_VERSION_MAJOR == 53) && (LIBAVFORMAT_VERSION_MINOR == 24) && (LIBAVFORMAT_VERSION_MICRO >= 100)))
FFMPEG_INLINE void av_update_cur_dts(AVFormatContext *s, AVStream *ref_st, int64_t timestamp)
{
    int i;
    for (i = 0; i < s->nb_streams; i++) {
        s->streams[i]->cur_dts = av_rescale(timestamp,
            s->streams[i]->time_base.den * (int64_t)ref_st->time_base.num,
            s->streams[i]->time_base.num * (int64_t)ref_st->time_base.den);
    }
}
#  endif
#endif

/* ============================================================
 * av_get_cropped_height_from_codec
 * ============================================================ */
#if ((LIBAVCODEC_VERSION_MAJOR > 54) || ((LIBAVCODEC_VERSION_MAJOR >= 54) && (LIBAVCODEC_VERSION_MINOR > 14)))
#  define FFMPEG_HAVE_CANON_H264_RESOLUTION_FIX
#endif

FFMPEG_INLINE int av_get_cropped_height_from_codec(AVCodecContext *pCodecCtx)
{
    int y = pCodecCtx->height;
#ifndef FFMPEG_HAVE_CANON_H264_RESOLUTION_FIX
    if (pCodecCtx->width == 1920 && pCodecCtx->height == 1088 &&
        pCodecCtx->pix_fmt == AV_PIX_FMT_YUVJ420P &&
        pCodecCtx->codec_id == AV_CODEC_ID_H264) {
        y = 1080;
    }
#endif
    return y;
}

/* ============================================================
 * av_get_pts_from_frame
 * ============================================================ */
FFMPEG_INLINE int64_t av_get_pts_from_frame(AVFormatContext *avctx, AVFrame *picture)
{
    int64_t pts;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 34, 100)
    pts = picture->pts;
#else
    pts = picture->pkt_pts;
#endif
    if (pts == AV_NOPTS_VALUE) pts = picture->pkt_dts;
    if (pts == AV_NOPTS_VALUE) pts = 0;
    (void)avctx;
    return pts;
}

/* ============================================================
 * av_get_r_frame_rate_compat
 * ============================================================ */
FFMPEG_INLINE AVRational av_get_r_frame_rate_compat(AVFormatContext *ctx, const AVStream *stream)
{
    const char *encoder = NULL;
    AVDictionaryEntry *tag = NULL;
    if (ctx->metadata) {
        while ((tag = av_dict_get(ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if (!strcmp(tag->key, "ENCODER") && !strncmp(tag->value, "Lavf", 4)) {
                encoder = tag->value; break;
            }
        }
    }
    if (encoder) return stream->r_frame_rate;
    return stream->avg_frame_rate;
}

/* ============================================================
 * avpicture_deinterlace - local implementation (FFmpeg 2.6.4 LGPL)
 * Used when library function is not available on FFmpeg 3+
 * ============================================================ */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 24, 102)

#define FFMPEG_COMPAT_MAX_NEG_CROP 1024

/* Crop table for the deinterlace filter (from FFmpeg 2.6.4 LGPL) */
static const uint8_t ff_compat_crop_tab[256 + 2 * FFMPEG_COMPAT_MAX_NEG_CROP] = {
    /* 0x00 repeated 256 times */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    /* 0x00..0xFF */
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF,
    /* 0xFF repeated 256 times */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

FFMPEG_INLINE void ff_compat_deinterlace_line(uint8_t *dst, const uint8_t *lum_m4,
    const uint8_t *lum_m3, const uint8_t *lum_m2, const uint8_t *lum_m1,
    const uint8_t *lum, int size)
{
    const uint8_t *cm = ff_compat_crop_tab + FFMPEG_COMPAT_MAX_NEG_CROP;
    int sum;
    for (; size > 0; size--) {
        sum = -lum_m4[0]; sum += lum_m3[0] << 2;
        sum += lum_m2[0] << 1; sum += lum_m1[0] << 2;
        sum += -lum[0]; dst[0] = cm[(sum + 4) >> 3];
        lum_m4++; lum_m3++; lum_m2++; lum_m1++; lum++; dst++;
    }
}

FFMPEG_INLINE void ff_compat_deinterlace_line_inplace(uint8_t *lum_m4, uint8_t *lum_m3,
    uint8_t *lum_m2, uint8_t *lum_m1, uint8_t *lum, int size)
{
    const uint8_t *cm = ff_compat_crop_tab + FFMPEG_COMPAT_MAX_NEG_CROP;
    int sum;
    for (; size > 0; size--) {
        sum = -lum_m4[0]; sum += lum_m3[0] << 2;
        sum += lum_m2[0] << 1; lum_m4[0] = lum_m2[0];
        sum += lum_m1[0] << 2; sum += -lum[0];
        lum_m2[0] = cm[(sum + 4) >> 3];
        lum_m4++; lum_m3++; lum_m2++; lum_m1++; lum++;
    }
}

/* avpicture_deinterlace implementation using local deinterlace
 *
 * Deinterlaces a frame by applying a 5-tap FIR filter vertically.
 * For each output row y: out[y] = clamp((-in[y-2] + 4*in[y-1] + 2*in[y] + 4*in[y+1] - in[y+2]) / 8)
 * Edge rows (y=0,1,h-2,h-1) are copied unchanged.
 */
FFMPEG_INLINE int avpicture_deinterlace(
    AVFrame *dst, const AVFrame *src,
    enum AVPixelFormat pix_fmt, int width, int height)
{
    int i, y, x;
    const uint8_t *cm = ff_compat_crop_tab + FFMPEG_COMPAT_MAX_NEG_CROP;

    if (pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUVJ420P &&
        pix_fmt != AV_PIX_FMT_YUV422P && pix_fmt != AV_PIX_FMT_YUVJ422P &&
        pix_fmt != AV_PIX_FMT_YUV444P && pix_fmt != AV_PIX_FMT_YUV411P &&
        pix_fmt != AV_PIX_FMT_GRAY8)
        return -1;
    if ((width & 3) != 0 || (height & 3) != 0) return -1;

    for (i = 0; i < 3; i++) {
        int w2 = width, h2 = height;
        int src_stride, dst_stride;
        const uint8_t *src_data;
        uint8_t *dst_data;

        if (i == 1) {
            switch (pix_fmt) {
            case AV_PIX_FMT_YUVJ420P: case AV_PIX_FMT_YUV420P: w2 >>= 1; h2 >>= 1; break;
            case AV_PIX_FMT_YUV422P: case AV_PIX_FMT_YUVJ422P: w2 >>= 1; break;
            case AV_PIX_FMT_YUV411P: w2 >>= 2; break;
            default: break;
            }
            if (pix_fmt == AV_PIX_FMT_GRAY8) break;
        }

        src_data = src->data[i];
        dst_data = dst->data[i];
        src_stride = src->linesize[i];
        dst_stride = dst->linesize[i];

        if (h2 < 5) {
            /* Too short - just copy */
            for (y = 0; y < h2; y++)
                memcpy(dst_data + y * dst_stride, src_data + y * src_stride, (size_t)w2);
            continue;
        }

        /* First 2 rows: copy */
        for (y = 0; y < 2; y++)
            memcpy(dst_data + y * dst_stride, src_data + y * src_stride, (size_t)w2);

        /* Middle rows: 5-tap FIR filter */
        for (y = 2; y < h2 - 2; y++) {
            const uint8_t *l_m4 = src_data + (y - 2) * src_stride;
            const uint8_t *l_m3 = src_data + (y - 1) * src_stride;
            const uint8_t *l_m2 = src_data + (y - 0) * src_stride;
            const uint8_t *l_m1 = src_data + (y + 1) * src_stride;
            const uint8_t *l    = src_data + (y + 2) * src_stride;
            uint8_t *out = dst_data + y * dst_stride;

            for (x = 0; x < w2; x++) {
                int sum = -l_m4[x] + (l_m3[x] << 2) + (l_m2[x] << 1) + (l_m1[x] << 2) - l[x];
                out[x] = cm[(sum + 4) >> 3];
            }
        }

        /* Last 2 rows: copy */
        for (y = h2 - 2; y < h2; y++)
            memcpy(dst_data + y * dst_stride, src_data + y * src_stride, (size_t)w2);
    }
    return 0;
}

#endif /* LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 24, 102) */

/* ============================================================
 * avformat_open_input compat: handle filename const issue
 * ============================================================ */
#ifndef FFMPEG5
#  include <libavformat/avformat.h>
#endif

/* ============================================================
 * HAVE_ macros for feature detection
 * ============================================================ */
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 105))
#  define FFMPEG_HAVE_AVIO
#endif
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 101))
#  define FFMPEG_HAVE_AV_DUMP_FORMAT
#  define FFMPEG_HAVE_PARSE_UTILS
#endif
#if (LIBAVFORMAT_VERSION_MAJOR > 52) || ((LIBAVFORMAT_VERSION_MAJOR >= 52) && (LIBAVFORMAT_VERSION_MINOR >= 45))
#  define FFMPEG_HAVE_AV_GUESS_FORMAT
#endif
#if (LIBAVCODEC_VERSION_MAJOR > 52) || ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 23))
#  define FFMPEG_HAVE_DECODE_AUDIO3
#endif
#if (LIBAVCODEC_VERSION_MAJOR > 52) || ((LIBAVCODEC_VERSION_MAJOR >= 52) && (LIBAVCODEC_VERSION_MINOR >= 64))
#  define FFMPEG_HAVE_AVMEDIA_TYPES
#endif
#if (LIBAVCODEC_VERSION_MAJOR > 53) || ((LIBAVCODEC_VERSION_MAJOR >= 53) && (LIBAVCODEC_VERSION_MINOR >= 60))
#  define FFMPEG_HAVE_ENCODE_AUDIO2
#endif
#if ((LIBAVCODEC_VERSION_MAJOR > 54) || (LIBAVCODEC_VERSION_MAJOR == 54 && LIBAVCODEC_VERSION_MINOR >= 13))
#  define FFMPEG_HAVE_FRAME_CHANNEL_LAYOUT
#endif

/* ============================================================
 * CODEC_FLAG compat
 * ============================================================ */
#ifndef AVFMT_GLOBALHEADER
#  define AVFMT_GLOBALHEADER 0
#endif
#ifndef CODEC_FLAG_GLOBAL_HEADER
#  ifdef AV_CODEC_FLAG_GLOBAL_HEADER
#    define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#  endif
#endif
#ifndef CODEC_FLAG_INTERLACED_DCT
#  ifdef AV_CODEC_FLAG_INTERLACED_DCT
#    define CODEC_FLAG_INTERLACED_DCT AV_CODEC_FLAG_INTERLACED_DCT
#  endif
#endif
#ifndef CODEC_FLAG_INTERLACED_ME
#  ifdef AV_CODEC_FLAG_INTERLACED_ME
#    define CODEC_FLAG_INTERLACED_ME AV_CODEC_FLAG_INTERLACED_ME
#  endif
#endif

/* ============================================================
 * AV_PIX_FMT compat (pre-rename)
 * ============================================================ */
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 3, 100)
#  define AV_PIX_FMT_BGR32 PIX_FMT_BGR32
#  define AV_PIX_FMT_YUV422P PIX_FMT_YUV422P
#  define AV_PIX_FMT_BGRA PIX_FMT_BGRA
#  define AV_PIX_FMT_ARGB PIX_FMT_ARGB
#  define AV_PIX_FMT_RGBA PIX_FMT_RGBA
#endif

/* ============================================================
 * av_frame_alloc/free compat (old FFmpeg)
 * ============================================================ */
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(52, 8, 0)
FFMPEG_INLINE AVFrame *av_frame_alloc(void) { return avcodec_alloc_frame(); }
FFMPEG_INLINE void av_frame_free(AVFrame **f) { av_freep(f); }
#endif

/* ============================================================
 * Legacy IO compat
 * ============================================================ */
#ifndef FFMPEG_HAVE_AVIO
#  define AVIO_FLAG_WRITE URL_WRONLY
#  define avio_open url_fopen
#  define avio_tell url_ftell
#  define avio_close url_fclose
#  define avio_size url_fsize
#endif
#ifndef AVIO_FLAG_WRITE
#  define AVIO_FLAG_WRITE URL_WRONLY
#endif
#ifndef AV_PKT_FLAG_KEY
#  define AV_PKT_FLAG_KEY PKT_FLAG_KEY
#endif
#ifndef FFMPEG_HAVE_AV_DUMP_FORMAT
#  define av_dump_format dump_format
#endif
#ifndef FFMPEG_HAVE_AV_GUESS_FORMAT
#  define av_guess_format guess_format
#endif
#ifndef FFMPEG_HAVE_AVMEDIA_TYPES
#  define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#  define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif

#endif /* __FFMPEG_COMPAT_H__ */
