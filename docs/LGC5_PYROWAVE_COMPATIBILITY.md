# LG C5 PyroWave compatibility notes

## Verified on the LG C5 (2026-11-04)

- LG webOS exposes Vulkan through `/usr/lib/libmali.so`, with `vk_icdGetInstanceProcAddr` and `vk_icdNegotiateLoaderICDInterfaceVersion`; `libvulkan.so` is absent.
- In-app ICD probe: negotiation succeeds, `vkCreateInstance` returns success, and one physical GPU is enumerated.
- Pinned PyroWave 0.6.0 returns `PYROWAVE_ERROR_NO_VULKAN (-5)` at `Context::init_loader(nullptr)` before device initialization. Do not confuse this with Vulkan's numeric error -5.
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
