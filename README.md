# LG C5 PyroWave

Minimal, isolated development project for a PyroWave-capable Moonlight client on the rooted LG C5 (webOS, ARM32 soft-float).

## Current milestone: buildable baseline

The GitHub Actions workflow builds a **separate, stock upstream Aurora** test package as a known-good packaging/launch baseline. This is **not yet a PyroWave decoder build**. It deliberately does not claim to decode or display PyroWave.

- App ID: `com.pyrowave.lgc5.test` (does not replace Eclipse).
- Upstream: `GuiDev1994/aurora-tv`, pinned by commit in `UPSTREAM.lock`.
- webOS toolchain: openlgtv/buildroot-nc4 `webos-a38c582`.
- Build: GitHub Actions only; no QNAP VM needed.
- Target for later stages: PyroWave decoder `186f0393b77f7755953b5ecde994bb1cec2e4155`, 3200×1800 @ 60 FPS.

## Test procedure

Open Actions → **Build webOS baseline** → **Run workflow**. After a successful run, download the `lgc5-baseline-ipk` artifact and install the IPK using your existing webOS homebrew installation method. Confirm it launches separately from Eclipse. Do not expect PyroWave support yet.

## Next gates

1. Confirm clean ARM32 .ipk builds and launches on the C5.
2. Establish Vulkan presentation or Vulkan→EGL interop on actual webOS.
3. Integrate the pinned PyroWave decoder, packet transport and 4:2:0 GPU output.
4. Verify 3200×1800/60 streaming and latency.

No fork of `edkoch25/aurora-tv-1.7.1` is used.
