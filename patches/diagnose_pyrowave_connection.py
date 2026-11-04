#!/usr/bin/env python3
from pathlib import Path
p=Path("aurora/src/app/stream/session_worker.c")
s=p.read_text()
def once(a,b):
 global s
 if s.count(a)!=1: raise RuntimeError(f"anchor count {s.count(a)}: {a[:80]}")
 s=s.replace(a,b)
once('#include "session_worker.h"', '#include "session_worker.h"\n#include "pyrowave_diag.h"')
once('    session_set_state(session, STREAMING_CONNECTING);','    lgc5_pw_diag("session worker entered formats=0x%x",session->config.stream.supportedVideoFormats);\n    session_set_state(session, STREAMING_CONNECTING);')
once('    int ret = gs_start_app(client, server, &session->config.stream, appId, server->isGfe, session->config.sops,','    lgc5_pw_diag("host launch begin appId=%d",appId);\n    int ret = gs_start_app(client, server, &session->config.stream, appId, server->isGfe, session->config.sops,')
once('    if (ret != GS_OK) {','    lgc5_pw_diag("host launch result=%d",ret);\n    if (ret != GS_OK) {')
once('    int startResult = LiStartConnection(&server->serverInfo, &session->config.stream,','    lgc5_pw_diag("LiStartConnection begin formats=0x%x serverCodecFlags=0x%x",session->config.stream.supportedVideoFormats,server->serverInfo.serverCodecModeSupport);\n    int startResult = LiStartConnection(&server->serverInfo, &session->config.stream,')
once('    if (startResult != 0) {','    lgc5_pw_diag("LiStartConnection result=%d streaming_errno=%d",startResult,streaming_errno);\n    if (startResult != 0) {')
once('    session_set_state(session, STREAMING_STREAMING);','    lgc5_pw_diag("LiStartConnection succeeded");\n    session_set_state(session, STREAMING_STREAMING);')
p.write_text(s)
print("Instrumented PyroWave session worker")
