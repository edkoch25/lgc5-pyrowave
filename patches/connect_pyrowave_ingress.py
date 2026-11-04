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
once('static int vdec_stream_format = 0;', 'static int vdec_stream_format = 0;\nstatic lgc5_pw_bridge pyrowave_bridge;\nstatic bool pyrowave_active = false;\nstatic lgc5_pw_sdl_renderer pyrowave_renderer;')
once('    SS4S_VideoInfo info;','''    if (videoFormat == VIDEO_FORMAT_PYROWAVE) {
        lgc5_pw_diag("video setup PyroWave %dx%d",width,height);
        if (!lgc5_pw_bridge_open(&pyrowave_bridge,(unsigned)width,(unsigned)height)) {
            commons_log_error("Session","PyroWave Vulkan decoder initialization failed");
            free(buffer); buffer=NULL;
            return CALLBACKS_SESSION_ERROR_VDEC_UNSUPPORTED;
        }
        if (!lgc5_pw_sdl_open(&pyrowave_renderer,(unsigned)width,(unsigned)height)) {
            commons_log_error("Session","PyroWave SDL video renderer initialization failed: %s",SDL_GetError());
            lgc5_pw_bridge_close(&pyrowave_bridge);
            free(buffer); buffer=NULL;
            return CALLBACKS_SESSION_ERROR_VDEC_UNSUPPORTED;
        }
        pyrowave_bridge.present_cpu_frame=lgc5_pw_sdl_present;
        pyrowave_bridge.present_userdata=&pyrowave_renderer;
        pyrowave_active=true;
        commons_log_info("Session","PyroWave CPU playback test initialized (%dx%d)",width,height);
        return 0;
    }

    SS4S_VideoInfo info;''')
once('    SS4S_PlayerVideoClose(player);','''    if (pyrowave_active) {
        lgc5_pw_bridge_close(&pyrowave_bridge);
        lgc5_pw_sdl_close(&pyrowave_renderer);
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
            lgc5_pw_diag("frame callback count=%lu bytes=%zu",pw_callbacks,(size_t)length);
        return lgc5_pw_bridge_submit(&pyrowave_bridge,buffer,length) ? DR_OK : DR_NEED_IDR;
    }
    SS4S_VideoFeedFlags flags =''')
p.write_text(s)
for name in ("pyrowave_records.h","pyrowave_ingest.h","pyrowave_bridge.h","pyrowave_sdl_renderer.h","pyrowave_diag.h"):
    (root/name).write_bytes((Path("src")/name).read_bytes())
print("Aurora live decoder callbacks connected to PyroWave ingress (SDL presenter wired; codec selection remains gated pending TV runtime validation).")
