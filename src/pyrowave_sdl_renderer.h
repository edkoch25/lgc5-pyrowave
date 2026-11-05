/* Experimental SDL IYUV presentation path for PyroWave CPU bring-up.
 * SDL video calls must be made from the video/UI thread on webOS; do not
 * activate codec negotiation until this threading contract is validated. */
#ifndef LGC5_PYROWAVE_SDL_RENDERER_H
#define LGC5_PYROWAVE_SDL_RENDERER_H
#include <SDL.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "pyrowave_bridge.h"
typedef struct {
    SDL_Window *window;
    bool owns_window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    unsigned width, height;
    unsigned long frames;
    Uint8 *test_y, *test_u, *test_v;
} lgc5_pw_sdl_renderer;
static inline void lgc5_pw_sdl_close(lgc5_pw_sdl_renderer *p) {
    if (!p) return;
    free(p->test_y); free(p->test_u); free(p->test_v);
    if (p->texture) SDL_DestroyTexture(p->texture);
    if (p->renderer) SDL_DestroyRenderer(p->renderer);
    if (p->window && p->owns_window) SDL_DestroyWindow(p->window);
    memset(p,0,sizeof(*p));
}
static inline bool lgc5_pw_sdl_open(lgc5_pw_sdl_renderer *p,SDL_Window *existing_window,unsigned width,unsigned height) {
    if (!p || !width || !height || (width&1u) || (height&1u)) return false;
    memset(p,0,sizeof(*p));
    if (!existing_window) {
        lgc5_pw_diag("SDL existing Aurora window missing");
        return false;
    }
    p->window=existing_window;
    p->owns_window=false;
    SDL_ShowWindow(p->window);
    lgc5_pw_diag("SDL using Aurora window=%p", (void *)p->window);
    lgc5_pw_diag("SDL renderer create begin");
    p->renderer=SDL_CreateRenderer(p->window,-1,SDL_RENDERER_ACCELERATED);
    if (!p->renderer) { lgc5_pw_diag("SDL renderer failed: %s",SDL_GetError()); goto fail; }
    lgc5_pw_diag("SDL renderer create complete");
    SDL_RendererInfo ri;
    if (SDL_GetRendererInfo(p->renderer,&ri)==0)
        lgc5_pw_diag("SDL renderer name=%s flags=0x%x max_texture=%dx%d",
            ri.name ? ri.name : "unknown",ri.flags,ri.max_texture_width,ri.max_texture_height);
    else lgc5_pw_diag("SDL renderer info failed: %s",SDL_GetError());
    lgc5_pw_diag("SDL texture create begin");
    p->texture=SDL_CreateTexture(p->renderer,SDL_PIXELFORMAT_IYUV,
                                 SDL_TEXTUREACCESS_STREAMING,(int)width,(int)height);
    if (!p->texture) { lgc5_pw_diag("SDL texture failed: %s",SDL_GetError()); goto fail; }
    lgc5_pw_diag("SDL texture create complete");
    p->width=width; p->height=height;
    const size_t luma=(size_t)width*height, chroma=luma/4;
    p->test_y=(Uint8 *)malloc(luma);
    p->test_u=(Uint8 *)malloc(chroma);
    p->test_v=(Uint8 *)malloc(chroma);
    if (!p->test_y || !p->test_u || !p->test_v) {
        lgc5_pw_diag("SDL diagnostic plane allocation failed");
        goto fail;
    }
    /* Eight vertical bars. YUV values are BT.709 limited-range. */
    static const Uint8 bars_y[8]={235,210,170,145,106,81,41,16};
    static const Uint8 bars_u[8]={128,16,166,54,202,90,240,128};
    static const Uint8 bars_v[8]={128,146,16,34,222,240,110,128};
    for (unsigned y=0;y<height;++y)
        for (unsigned x=0;x<width;++x)
            p->test_y[(size_t)y*width+x]=bars_y[(x*8u)/width];
    for (unsigned y=0;y<height/2;++y)
        for (unsigned x=0;x<width/2;++x) {
            unsigned bar=(x*16u)/width;
            p->test_u[(size_t)y*(width/2)+x]=bars_u[bar];
            p->test_v[(size_t)y*(width/2)+x]=bars_v[bar];
        }
    lgc5_pw_diag("SDL YUV420 diagnostic planes ready %ux%u",width,height);
    int ww=0,wh=0;
    SDL_GetWindowSize(p->window,&ww,&wh);
    lgc5_pw_diag("SDL window flags=0x%x size=%dx%d display=%d video_driver=%s",
        SDL_GetWindowFlags(p->window),ww,wh,SDL_GetWindowDisplayIndex(p->window),
        SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
    lgc5_pw_diag("SDL renderer ready");
    return true;
fail:
    lgc5_pw_sdl_close(p);
    return false;
}
static inline bool lgc5_pw_sdl_present(void *userdata,const lgc5_pw_cpu_buffer *frame) {
    lgc5_pw_sdl_renderer *p=(lgc5_pw_sdl_renderer *)userdata;
    if (!p || !frame || !p->texture || frame->width!=(int)p->width ||
        frame->height!=(int)p->height || frame->format!=1) return false;
    /* #58: isolate SDL IYUV conversion from PyroWave output.
     * 0-59: known-good SDL RGB magenta
     * 60-119: generated YUV420 color bars through SDL_UpdateYUVTexture
     * 120-179: decoded Y with neutral U/V (luma-only)
     * 180+: untouched decoded YUV420P */
    const unsigned phase=p->frames<60 ? 0 : p->frames<120 ? 1 : p->frames<180 ? 2 : 3;
    const Uint8 *y=(const Uint8 *)frame->data[0];
    const Uint8 *u=(const Uint8 *)frame->data[1];
    const Uint8 *v=(const Uint8 *)frame->data[2];
    if (!y || !u || !v) return false;
    if (p->frames<3 || p->frames%120==0) {
        size_t mid=(size_t)(p->height/2)*frame->row_stride_in_bytes[0]+p->width/2;
        size_t chroma=(size_t)(p->height/4)*frame->row_stride_in_bytes[1]+p->width/4;
        lgc5_pw_diag("SDL decoded frame=%lu phase=%u stride=%zu/%zu/%zu sizes=%zu/%zu/%zu Y=%u,%u,%u U=%u V=%u",
            p->frames,phase,frame->row_stride_in_bytes[0],frame->row_stride_in_bytes[1],
            frame->row_stride_in_bytes[2],frame->plane_size_in_bytes[0],
            frame->plane_size_in_bytes[1],frame->plane_size_in_bytes[2],
            (unsigned)y[0],(unsigned)y[mid],(unsigned)y[frame->plane_size_in_bytes[0]-1],
            (unsigned)u[chroma],(unsigned)v[chroma]);
    }
    const Uint8 *draw_y=y, *draw_u=u, *draw_v=v;
    int pitch_y=(int)frame->row_stride_in_bytes[0];
    int pitch_u=(int)frame->row_stride_in_bytes[1];
    int pitch_v=(int)frame->row_stride_in_bytes[2];
    if (phase==1) {
        draw_y=p->test_y; draw_u=p->test_u; draw_v=p->test_v;
        pitch_y=(int)p->width; pitch_u=(int)p->width/2; pitch_v=(int)p->width/2;
    } else if (phase==2) {
        draw_u=p->test_u; draw_v=p->test_v;
        /* Neutral chroma must be 128 everywhere, not color bars. */
        memset(p->test_u,128,(size_t)p->width*p->height/4);
        memset(p->test_v,128,(size_t)p->width*p->height/4);
        pitch_u=(int)p->width/2; pitch_v=(int)p->width/2;
    }
    if (phase!=0 && SDL_UpdateYUVTexture(p->texture,NULL,draw_y,pitch_y,
                                          draw_u,pitch_u,draw_v,pitch_v)!=0) {
        lgc5_pw_diag("SDL UpdateYUVTexture failed phase=%u: %s",phase,SDL_GetError());
        return false;
    }
    if (SDL_SetRenderDrawColor(p->renderer,255,0,255,255)!=0 ||
        SDL_RenderClear(p->renderer)!=0 ||
        (phase!=0 && SDL_RenderCopy(p->renderer,p->texture,NULL,NULL)!=0)) {
        lgc5_pw_diag("SDL render failed phase=%u: %s",phase,SDL_GetError());
        return false;
    }
    if (p->frames==0 || p->frames==59 || p->frames==60 || p->frames==119 ||
        p->frames==120 || p->frames==179 || p->frames==180)
        lgc5_pw_diag("SDL diagnostic frame=%lu phase=%u (0=magenta 1=bars 2=luma 3=decoded)",
                     p->frames,phase);
    SDL_RenderPresent(p->renderer);
    if (p->frames==0 || p->frames==59 || p->frames==60)
        lgc5_pw_diag("SDL diagnostic display frame=%lu mode=%s window_flags=0x%x",
            p->frames,diagnostic_color ? "magenta" : "decoded",SDL_GetWindowFlags(p->window));
    p->frames++;
    return true;
}
#endif
