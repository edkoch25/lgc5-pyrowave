/* Experimental SDL IYUV presentation path for PyroWave CPU bring-up.
 * SDL video calls must be made from the video/UI thread on webOS; do not
 * activate codec negotiation until this threading contract is validated. */
#ifndef LGC5_PYROWAVE_SDL_RENDERER_H
#define LGC5_PYROWAVE_SDL_RENDERER_H
#include <SDL.h>
#include <stdbool.h>
#include <string.h>
#include "pyrowave_bridge.h"
typedef struct {
    SDL_Window *window;
    bool owns_window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    unsigned width, height;
    unsigned long frames;
} lgc5_pw_sdl_renderer;
static inline void lgc5_pw_sdl_close(lgc5_pw_sdl_renderer *p) {
    if (!p) return;
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
    if (p->frames<3 || p->frames%120==0) {
        const unsigned char *y=(const unsigned char *)frame->data[0];
        const unsigned char *u=(const unsigned char *)frame->data[1];
        const unsigned char *v=(const unsigned char *)frame->data[2];
        size_t mid=(size_t)(p->height/2)*frame->row_stride_in_bytes[0]+p->width/2;
        size_t chroma=(size_t)(p->height/4)*frame->row_stride_in_bytes[1]+p->width/4;
        lgc5_pw_diag("SDL decoded sample frame=%lu Y=%u,%u,%u U=%u V=%u",
            p->frames,(unsigned)y[0],(unsigned)y[mid],(unsigned)y[frame->plane_size_in_bytes[0]-1],
            (unsigned)u[chroma],(unsigned)v[chroma]);
    }
    if (SDL_UpdateYUVTexture(p->texture,NULL,
                             (const Uint8 *)frame->data[0],(int)frame->row_stride_in_bytes[0],
                             (const Uint8 *)frame->data[1],(int)frame->row_stride_in_bytes[1],
                             (const Uint8 *)frame->data[2],(int)frame->row_stride_in_bytes[2])!=0)
        return false;
    /* First 60 frames: show solid magenta through the same SDL window.
     * Thereafter resume decoded video without changing codec or lifecycle. */
    bool diagnostic_color=p->frames<60;
    if (diagnostic_color && SDL_SetRenderDrawColor(p->renderer,255,0,255,255)!=0) {
        lgc5_pw_diag("SDL test color failed: %s",SDL_GetError());
        return false;
    }
    if (SDL_RenderClear(p->renderer)!=0 ||
        (!diagnostic_color && SDL_RenderCopy(p->renderer,p->texture,NULL,NULL)!=0)) {
        lgc5_pw_diag("SDL render failed: %s",SDL_GetError());
        return false;
    }
    SDL_RenderPresent(p->renderer);
    if (p->frames==0 || p->frames==59 || p->frames==60)
        lgc5_pw_diag("SDL diagnostic display frame=%lu mode=%s window_flags=0x%x",
            p->frames,diagnostic_color ? "magenta" : "decoded",SDL_GetWindowFlags(p->window));
    p->frames++;
    return true;
}
#endif
