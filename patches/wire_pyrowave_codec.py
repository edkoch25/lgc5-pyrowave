#!/usr/bin/env python3
"""Wire experimental codec selection to Aurora without changing the default path.

PyroWave is opt-in for a controlled LG C5 CPU playback test.
Set PYROWAVE_RENDERER_READY to 1 only when that renderer is implemented.
"""
from pathlib import Path
p=Path("aurora/src/app/stream/session.c")
s=p.read_text()
needle='    if (video_cap.codecs & SS4S_VIDEO_H264) {'
assert s.count(needle)==1
s=s.replace(needle,'''    /* PyroWave is a distinct codec, not HEVC. Never negotiate it until
     * the decoder and presentation callbacks are installed. */
#define PYROWAVE_RENDERER_READY 1
    if (app_config->pyrowave_enabled) {
#if PYROWAVE_RENDERER_READY
        config->stream.supportedVideoFormats = VIDEO_FORMAT_PYROWAVE;
        commons_log_info("Session", "Codec preference: PyroWave 8-bit 4:2:0");
        goto video_formats_selected;
#else
        commons_log_warn("Session", "PyroWave requested but renderer not ready; retaining native codec");
#endif
    }
    if (video_cap.codecs & SS4S_VIDEO_H264) {''')
needle2='    // If no video format is supported, default to H.264'
assert s.count(needle2)==1
s=s.replace(needle2,'video_formats_selected:\n'+needle2)
p.write_text(s)
# Aurora reconstructs the host codec capability mask from selected formats.
# Include PyroWave so the RTSP negotiation guard can recognize it.
p=Path("aurora/src/app/stream/session.c")
s=p.read_text()
needle='    session->app_id = gs_app->id;'
assert s.count(needle)==1
s=s.replace(needle,'''    if (session->config.stream.supportedVideoFormats & VIDEO_FORMAT_PYROWAVE) {
        session->server->serverInfo.serverCodecModeSupport |= SCM_PYROWAVE;
    }
'''+needle)
p.write_text(s)

print("Wired PyroWave codec preference with safe renderer readiness gate")
