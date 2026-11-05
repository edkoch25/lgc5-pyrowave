#!/usr/bin/env python3
"""Connect Aurora video callbacks to PyroWave packet ingestion, gated by codec."""
from pathlib import Path
root=Path("aurora/src/app/stream/video")
p=root/"session_video.c"
s=p.read_text()
def once(a,b):
    global s
    if s.count(a)!=1: raise RuntimeError(f"anchor count {s.count(a)}: {a[:80]}")
    s=s.replace(a,b)
once('#include "session_video.h"', '#include "session_video.h"\n#include "pyrowave_bridge.h"\n#include "pyrowave_sdl_renderer.h"')
once('static int vdec_stream_format = 0;', 'static int vdec_stream_format = 0;\nstatic lgc5_pw_bridge pyrowave_bridge;\nstatic bool pyrowave_active = false;\nstatic bool pyrowave_renderer_ready = false;\nstatic lgc5_pw_sdl_renderer pyrowave_renderer;\n' + "\ntypedef struct {\n    app_t *app;\n    unsigned width, height;\n    bool ok;\n} lgc5_pw_ui_setup_request;\ntypedef struct {\n    const lgc5_pw_cpu_buffer *frame;\n    bool ok;\n} lgc5_pw_ui_frame_request;\n\n/* On webOS, both the compositor window and SDL render calls belong to\n * Aurora's UI thread. Do not close the Connecting scene from video setup:\n * session_worker has not yet posted USER_STREAM_OPEN at that point. */\nstatic void lgc5_pw_ui_activate(void *opaque) {\n    lgc5_pw_ui_setup_request *req = (lgc5_pw_ui_setup_request *)opaque;\n    lgc5_pw_diag(\"UI activation after stream started; close current UI\");\n    app_ui_close(&req->app->ui);\n    lgc5_pw_diag(\"UI closed; reuse registered window=%p\", (void *)req->app->ui.window);\n    req->ok = lgc5_pw_sdl_open(&pyrowave_renderer, req->app->ui.window, req->width, req->height);\n    lgc5_pw_diag(\"UI SDL activation result=%d\", req->ok ? 1 : 0);\n}\nstatic void lgc5_pw_ui_present_action(void *opaque) {\n    lgc5_pw_ui_frame_request *req = (lgc5_pw_ui_frame_request *)opaque;\n    req->ok = lgc5_pw_sdl_present(&pyrowave_renderer, req->frame);\n}\nstatic bool lgc5_pw_present_main(void *unused, const lgc5_pw_cpu_buffer *frame) {\n    (void)unused;\n    if (!session || !session->app) return false;\n    lgc5_pw_ui_frame_request req = {frame, false};\n    app_bus_post_sync(session->app, (bus_actionfunc)lgc5_pw_ui_present_action, &req);\n    return req.ok;\n}\nstatic void lgc5_pw_ui_release(void *unused) {\n    (void)unused;\n    lgc5_pw_sdl_close(&pyrowave_renderer);\n}\n" )
once('    SS4S_VideoInfo info;','''    if (videoFormat == VIDEO_FORMAT_PYROWAVE) {
        lgc5_pw_diag("video setup PyroWave %dx%d",width,height);
        if (!lgc5_pw_bridge_open(&pyrowave_bridge,(unsigned)width,(unsigned)height)) {
            commons_log_error("Session","PyroWave Vulkan decoder initialization failed");
            free(buffer); buffer=NULL;
            return CALLBACKS_SESSION_ERROR_VDEC_UNSUPPORTED;
        }
        /* Video setup runs before USER_STREAM_OPEN. Keep the launcher UI
         * alive until session_worker has entered STREAMING_STREAMING. */
        pyrowave_renderer_ready=false;
        pyrowave_active=true;
        lgc5_pw_diag("PyroWave decoder ready; deferring SDL/UI activation until streaming state");
        commons_log_info("Session","PyroWave decoder ready (%dx%d); SDL presenter deferred",width,height);
        return 0;
    }

    SS4S_VideoInfo info;''')
once('    SS4S_PlayerVideoClose(player);','''    if (pyrowave_active) {
        lgc5_pw_bridge_close(&pyrowave_bridge);
        if (pyrowave_renderer_ready && session && session->app)
            app_bus_post_sync(session->app,(bus_actionfunc)lgc5_pw_ui_release,NULL);
        pyrowave_renderer_ready=false;
        pyrowave_active=false;
    } else {
        SS4S_PlayerVideoClose(player);
    }''')
once('    SS4S_VideoFeedFlags flags =','''    if (pyrowave_active) {
        /* Decode readiness is not presentation. Do not enable codec selection
         * until a Vulkan image presenter consumes the decoded GPU buffers. */
        static unsigned long pw_callbacks=0;
        ++pw_callbacks;
        if (pw_callbacks<=3 || pw_callbacks%120==0)
            lgc5_pw_diag("frame callback count=%lu bytes=%zu streaming=%d",pw_callbacks,(size_t)length,session_is_streaming(session) ? 1 : 0);
        if (!pyrowave_renderer_ready) {
            if (!session_is_streaming(session))
                return DR_OK; /* Drop early intra frames; never tear down Connecting during setup. */
            lgc5_pw_ui_setup_request req = {session->app,pyrowave_bridge.width,pyrowave_bridge.height,false};
            app_bus_post_sync(session->app,(bus_actionfunc)lgc5_pw_ui_activate,&req);
            if (!req.ok) {
                commons_log_error("Session","PyroWave SDL presenter activation failed: %s",SDL_GetError());
                return DR_NEED_IDR;
            }
            pyrowave_bridge.present_cpu_frame=lgc5_pw_present_main;
            pyrowave_bridge.present_userdata=NULL;
            pyrowave_renderer_ready=true;
            commons_log_info("Session","PyroWave SDL presenter active on UI thread (%ux%u)",req.width,req.height);
        }
        return lgc5_pw_bridge_submit(&pyrowave_bridge,buffer,length) ? DR_OK : DR_NEED_IDR;
    }
    SS4S_VideoFeedFlags flags =''')
p.write_text(s)
for name in ("pyrowave_records.h","pyrowave_ingest.h","pyrowave_bridge.h","pyrowave_sdl_renderer.h","pyrowave_diag.h"):
    (root/name).write_bytes((Path("src")/name).read_bytes())
print("Aurora live decoder callbacks connected to PyroWave ingress (SDL presenter wired; codec selection remains gated pending TV runtime validation).")
