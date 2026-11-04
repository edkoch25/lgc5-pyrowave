#!/usr/bin/env python3
"""Apply minimal, fail-fast Aurora settings changes for experimental PyroWave."""
from pathlib import Path

root = Path("aurora")

def replace_once(path, old, new):
    p = root / path
    data = p.read_text()
    if data.count(old) != 1:
        raise RuntimeError(f"{path}: expected exactly one anchor, got {data.count(old)}")
    p.write_text(data.replace(old, new))

replace_once("src/app/app_settings.h",
             "    bool hevc_sliced_frames;",
             "    bool pyrowave_enabled; /* Experimental Vulkan decoder; defaults off. */\n    bool hevc_sliced_frames;")
replace_once("src/app/app_settings.c",
             "    config->hevc_sliced_frames = false;",
             "    config->pyrowave_enabled = false;\n    config->hevc_sliced_frames = false;")
replace_once("src/app/app_settings.c",
             '    ini_write_bool(fp, "hevc_sliced_frames", config->hevc_sliced_frames);',
             '    ini_write_bool(fp, "hevc_sliced_frames", config->hevc_sliced_frames);\n    ini_write_bool(fp, "pyrowave_enabled", config->pyrowave_enabled);')
replace_once("src/app/app_settings.c",
             '    } else if (INI_FULL_MATCH("video", "hevc_sliced_frames") || INI_NAME_MATCH("hevc_sliced_frames")) {',
             '    } else if (INI_FULL_MATCH("video", "pyrowave_enabled")) {\n        config->pyrowave_enabled = INI_IS_TRUE(value);\n    } else if (INI_FULL_MATCH("video", "hevc_sliced_frames") || INI_NAME_MATCH("hevc_sliced_frames")) {')
replace_once("src/app/ui/settings/panes/experimental.pane.c",
             '    pref_header(view, locstr("Video"));',
             '    pref_header(view, locstr("Video"));\n\n'
             '    lv_obj_t *pyrowave = pref_checkbox(view, "PyroWave Vulkan decoder (experimental)",\n'
             '                                         &app_configuration->pyrowave_enabled, false);\n'
             '    lv_obj_add_event_cb(pyrowave, reconnect_cb, LV_EVENT_VALUE_CHANGED, pane);\n'
             '    pref_desc_label(view, "Experimental CPU playback test; disable to restore native HEVC.", false);')
replace_once("src/app/stream/video/session_video.c",
             '    SS4S_VideoInfo info;',
             '    if (app_configuration && app_configuration->pyrowave_enabled) {\n'
             '        commons_log_warn("Session", "PyroWave selected, but stream integration is not yet available; using native LG decoder");\n'
             '    }\n\n    SS4S_VideoInfo info;')
print("Applied PyroWave settings toggle and explicit native-decoder fallback.")
