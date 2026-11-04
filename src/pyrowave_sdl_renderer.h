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
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    unsigned width, height;
    unsigned long frames;
} lgc5_pw_sdl_renderer;
static inline void lgc5_pw_sdl_close(lgc5_pw_sdl_renderer *p) {
    if (!p) return;
    if (p->texture) SDL_DestroyTexture(p->texture);
    if (p->renderer) SDL_DestroyRenderer(p->renderer);
    if (p->window) SDL_DestroyWindow(p->window);
    memset(p,0,sizeof(*p));
}
static inline bool lgc5_pw_sdl_open(lgc5_pw_sdl_renderer *p,unsigned width,unsigned height) {
    if (!p || !width || !height || (width&1u) || (height&1u)) return false;
    memset(p,0,sizeof(*p));
    p->window=SDL_CreateWindow("PyroWave video",SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,(int)width,(int)height,
                                SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!p->window) goto fail;
    p->renderer=SDL_CreateRenderer(p->window,-1,SDL_RENDERER_ACCELERATED);
    if (!p->renderer) goto fail;
    p->texture=SDL_CreateTexture(p->renderer,SDL_PIXELFORMAT_IYUV,
                                 SDL_TEXTUREACCESS_STREAMING,(int)width,(int)height);
    if (!p->texture) goto fail;
    p->width=width; p->height=height;
    return true;
fail:
    lgc5_pw_sdl_close(p);
    return false;
}
static inline bool lgc5_pw_sdl_present(void *userdata,const lgc5_pw_cpu_buffer *frame) {
    lgc5_pw_sdl_renderer *p=(lgc5_pw_sdl_renderer *)userdata;
    if (!p || !frame || !p->texture || frame->width!=(int)p->width ||
        frame->height!=(int)p->height || frame->format!=1) return false;
    if (SDL_UpdateYUVTexture(p->texture,NULL,
                             (const Uint8 *)frame->data[0],(int)frame->row_stride_in_bytes[0],
                             (const Uint8 *)frame->data[1],(int)frame->row_stride_in_bytes[1],
                             (const Uint8 *)frame->data[2],(int)frame->row_stride_in_bytes[2])!=0)
        return false;
    if (SDL_RenderClear(p->renderer)!=0 ||
        SDL_RenderCopy(p->renderer,p->texture,NULL,NULL)!=0) return false;
    SDL_RenderPresent(p->renderer);
    p->frames++;
    return true;
}
#endif
