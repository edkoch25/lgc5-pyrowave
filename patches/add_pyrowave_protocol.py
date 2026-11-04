#!/usr/bin/env python3
"""Small, pinned Aurora Moonlight protocol patch for PyroWave 8-bit 4:2:0.
This does not enable the decoder or presentation path by itself.
"""
from pathlib import Path

root = Path("aurora/core/moonlight-common-c/src")

def edit(name, changes):
    p = root / name
    text = p.read_text()
    for old, new in changes:
        if text.count(old) != 1:
            raise RuntimeError(f"{name}: expected one occurrence, got {text.count(old)}: {old[:80]!r}")
        text = text.replace(old, new)
    p.write_text(text)

edit("Limelight.h", [
    ("#define VIDEO_FORMAT_AV1_HIGH10_444  0x8000 // AV1 High 4:4:4 10-bit profile",
     "#define VIDEO_FORMAT_AV1_HIGH10_444  0x8000 // AV1 High 4:4:4 10-bit profile\n#define VIDEO_FORMAT_PYROWAVE        0x010000 // PyroWave 8-bit 4:2:0"),
    ("#define VIDEO_FORMAT_MASK_AV1    0xF000",
     "#define VIDEO_FORMAT_MASK_AV1    0xF000\n#define VIDEO_FORMAT_MASK_PYROWAVE 0x0F0000"),
    ("#define SCM_AV1_HIGH10_444  0x00400000 // Sunshine extension",
     "#define SCM_AV1_HIGH10_444  0x00400000 // Sunshine extension\n#define SCM_PYROWAVE        0x00800000 // VibePollo PyroWave 8-bit 4:2:0"),
])
edit("RtspConnection.c", [
    ('if ((StreamConfig.supportedVideoFormats & VIDEO_FORMAT_MASK_AV1) && strstr(response.payload, "AV1/90000")) {',
     '''if ((StreamConfig.supportedVideoFormats & VIDEO_FORMAT_PYROWAVE) &&
            (serverInfo->serverCodecModeSupport & SCM_PYROWAVE) &&
            strstr(response.payload, "PYROWAVE/90000") &&
            strstr(response.payload, "x-ss-pyrowave.bitstream:186f0393")) {
            NegotiatedVideoFormat = VIDEO_FORMAT_PYROWAVE;
        }
        else if ((StreamConfig.supportedVideoFormats & VIDEO_FORMAT_MASK_AV1) && strstr(response.payload, "AV1/90000")) {'''),
])
edit("SdpGenerator.c", [
    ('if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_AV1) {\n            err |= addAttributeString(&optionHead, "x-nv-vqos[0].bitStreamFormat", "2");',
     '''if (NegotiatedVideoFormat & VIDEO_FORMAT_PYROWAVE) {
            err |= addAttributeString(&optionHead, "x-nv-vqos[0].bitStreamFormat", "3");
            err |= addAttributeString(&optionHead, "x-ss-video[0].pyrowaveAdaptiveFec", "0");
            err |= addAttributeString(&optionHead, "x-ss-video[0].pyrowaveAdaptiveBitrate", "0");
            err |= addAttributeString(&optionHead, "x-ss-video[0].pyrowaveFeatures", "1");
        }
        else if (NegotiatedVideoFormat & VIDEO_FORMAT_MASK_AV1) {
            err |= addAttributeString(&optionHead, "x-nv-vqos[0].bitStreamFormat", "2");'''),
])
print("Applied minimal PyroWave 8-bit protocol negotiation patch")
