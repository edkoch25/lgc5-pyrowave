# LG C5 PyroWave compatibility notes

## Verified on the LG C5 (2026-11-04)

- LG webOS exposes Vulkan through `/usr/lib/libmali.so`, with `vk_icdGetInstanceProcAddr` and `vk_icdNegotiateLoaderICDInterfaceVersion`; `libvulkan.so` is absent.
- In-app ICD probe: negotiation succeeds, `vkCreateInstance` returns success, and one physical GPU is enumerated.
- The earlier PyroWave 0.6.0 build returned `PYROWAVE_ERROR_NO_VULKAN (-5)` at `Context::init_loader(nullptr)` before device initialization. Do not confuse this with Vulkan's numeric error -5.
- Build #64 updates PyroWave to API 1.0.0. The `pyrowave_cpu_buffer` and `pyrowave_decoder_create_info` field layouts used by the current C bridge are unchanged between 0.6.0 and 1.0.0; the shared-library SONAME changes from `.so.0` to `.so.1`.
- Earlier GLIBCXX_3.4.30 mismatch was cleared by statically linking the PyroWave shared library against the build toolchain's C++ runtime. Avoid changing this without evidence.
- Build #46 attempts a targeted Granite fallback to the Mali ICD. Its first run failed at a source-patch assertion, not at TV runtime.

## Upstream references and possible future blockers

- PyroWave Shield TV Vulkan 1.1 compatibility report: https://github.com/Themaister/pyrowave/issues/30
- Granite legacy implicit queue synchronization deadlock fix: https://github.com/Themaister/Granite/pull/158
- Shield report documents Vulkan API version filtering, synchronization2 requirements, and a tested compatibility shim. Do **not** import the ~700-line shim unless LG device-capability logs demonstrate it is necessary.
- Inspect the LG GPU's reported Vulkan API version, synchronization2, subgroup-size control, supported subgroup operations, and extension set **after** the loader works.
- The Shield's 1080p60 results do not establish LG C5 performance or compatibility.

## Workflow discipline

1. Keep upstream SHAs pinned in `UPSTREAM.lock`. Validate every source patch against the pinned source before changing the workflow.
2. Prefer one substantive, reviewed commit per test. Avoid repeated inline source rewriting and new workflows.
3. Fail early with descriptive patch assertions; verify generated C++ syntax and target matches before cross-compilation.
4. Keep compatibility fixes gated by measured C5 capabilities. No speculative Vulkan fallbacks or unrelated runtime/toolchain changes.
5. Preserve the isolated `com.pyrowave.lgc5.test` app and avoid modifying system libraries on the TV.
6. Progress through loader → instance → physical device → logical device/features → decoder → frame rendering → playback performance. Record the first failing stage.


## Build #63: Mali-G57 FP32 diagnostic

- Set `PYROWAVE_PRECISION=2` before PyroWave device/decoder initialization. Upstream defines precision 2 as full FP32 wavelet storage; precision 0 uses FP16 and precision 1 uses mixed FP16/FP32.
- Keep PyroWave's automatic fragment-path selection unchanged and log the selected value. The upstream decoder documents fragment decoding as the preferred path for mobile GPUs with weak compute support.
- Keep the existing CPU-buffer readback and SDL presentation unchanged for this controlled test. This isolates precision from presentation-path changes.
- Compare the resulting TV image and diagnostic log with Build #62. If corruption remains, do not assume the CPU-buffer path is the cause; proceed to a separate native GPU-buffer presentation experiment.
- This is a diagnostic build, not a final performance configuration. FP32 may increase GPU memory use and processing cost.


## Build #64: upstream Shield compatibility baseline

- Pin PyroWave to `7fe0e2667cd3076b8b2bf04bcb80bc8d691b800b` (API 1.0.0); its `checkout_granite.sh` pins Granite to `53090429539bf3404250d18fe58db971f0907c04`.
- This upstream line includes Vulkan 1.1 instance/device compatibility, Granite's synchronization2 compatibility support, and the merged legacy implicit-queue semaphore deadlock fix (Granite PR #158).
- The Shield's approximately 700-line Vulkan 1.0 sync2 shim is not imported. C5 previously reported Vulkan 1.3; use upstream Granite behavior and log the actual physical-device vendor/device IDs and API level.
- Update the webOS bridge's expected API from 0.6 to 1.0 and load SONAME `libpyrowave-shared.so.1` / versioned file `.so.1.0.0`.
- Keep Build #63's full FP32 request (`PYROWAVE_PRECISION=2`), automatic fragment-path selection, CPU-buffer readback, and SDL presentation unchanged for the controlled first comparison.
- Keep the isolated app ID, parser validation, Mali ICD loader fallback, and 50 Mbps default unchanged.
