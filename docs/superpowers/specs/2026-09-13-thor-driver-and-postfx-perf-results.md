# AYN Thor: custom GPU driver and post-processing perf results (2026-09-13)

Device: AYN Thor, Snapdragon 8 Gen 2 / Adreno 740, Android 13, 1920x1080. System driver: Qualcomm
Adreno Vulkan 512.676.53 (Vulkan 1.3). Custom driver already installed on the device: `vulkan.purple.so`
(MrPurple666 purple-turnip build), loaded through libadrenotools.

Game: *Star Wars: Rogue Squadron II — Rogue Leader* (GSWE64), attract mode, no input. Settings: IR 4x,
Vulkan, librashader post-processing, RetroCrisis "RC GDV-NTSC - PS2" 1080p preset, hybrid ubershaders,
wait-for-shaders on. Each run: launch, 130 s, pull `Logs/render_times.txt`, skip the first 30 s.
"unlimited" = `EmulationSpeed = 0`; otherwise the 100% speed limiter caps at 60 fps.

## Frame-time results

| Run | Code | Driver | Speed | fps (mean) | median ms | p95 ms | p99 ms | Note |
|---|---|---|---|---|---|---|---|---|
| base-sys | old (master) | Qualcomm | 100% | 57.6 | 16.66 | 19.00 | 29.73 | capped at 60 |
| base-turnip | old | Turnip | 100% | 57.4 | 16.67 | 24.06 | 29.77 | capped at 60 |
| unl-sys | old | Qualcomm | unlimited | **71.2** | 14.21 | 16.47 | 21.96 | |
| unl-turnip | old | Turnip | unlimited | **69.3** | 14.25 | 17.46 | 24.81 | |
| new-sys | branch (D1+D3 active) | Qualcomm | unlimited | **71.2** | 14.02 | 16.82 | 23.53 | |
| new-sys-nochain | branch, chain off | Qualcomm | unlimited | 123.3 | 5.34 | 22.27 | 35.05 | diagnostic |
| new-turnip | branch | Turnip | unlimited | 68.1 | 14.52 | 16.73 | 25.05 | stops at ~67 s, see below |
| control-oldcode-turnip | master rebuilt today | Turnip | unlimited | 67.2 | 14.52 | 16.67 | 25.11 | stops at ~67 s |
| new-balemuni | branch | Balemuni Apex v2 (Mesa 26.3-devel b9a2bf3) | unlimited | **72.9** | 13.44 | 16.37 | 23.09 | full run, downloaded via the new UI |

D2 (direct-to-backbuffer) does not fire in these runs because the game is pillarboxed; a separate
stretch-aspect run confirmed it renders correctly with dynamic rendering on.

## Conclusions

1. **Custom driver.** The Turnip build that was installed on the device (purple-turnip T30) is
   about 3% slower than the Qualcomm driver at unlimited speed and has worse tail latency (p95 17.5
   vs 16.5 ms, p99 24.8 vs 22.0 ms), and it stops rendering this game after ~75 s (item 3). The
   Balemuni Apex v2 pack that the new driver manager recommends for the Adreno 740 and downloaded
   in-app is the best of the three: 72.9 fps vs 71.2 (Qualcomm) vs 69.3 (purple), with the lowest
   median and tail frame times, and it ran the whole window. A ~2% gain: real but small; the
   recommendation is Balemuni Apex v2 or the system driver, not purple T30.
2. **Post-processing optimizations.** D1 (dynamic rendering for librashader) and D3 (deferred
   backbuffer clear) change nothing measurable: 71.2 fps before and after on the Qualcomm driver. The
   no-chain diagnostic explains why: the 18-pass CRT chain itself costs ~6 ms of a 14 ms frame, so the
   emulator-side waste those two changes remove was already negligible next to it. They remain
   structural wins (fewer per-pass objects, one backbuffer round-trip fewer) with no pixel change.
3. **Turnip stops rendering ~75 s into the attract mode.** Every Turnip run of today's builds ends at
   the same point: the game enters a widescreen space scene, Dolphin logs a mid-frame command-buffer
   flush for texture uploads, and no further frames are presented (process alive, no crash). It
   reproduces with each of this branch's changes individually disabled (dynamic rendering off, the
   device feature not enabled, Vulkan 1.2 instance, eager clear, forced 4:3, built-in post-processor)
   and **with unmodified origin/master rebuilt today**, so it is not caused by this branch. The system
   driver renders the same run to the end. The two morning runs of the August 23 APK on Turnip did
   not reach that scene within 130 s (they were still on the title screen), which is why they passed.
   The Balemuni Apex v2 pack (newer Mesa) hit the same texture-upload flush twice and kept rendering
   through the demo and into the next attract loop. Root cause not established; treat purple-turnip
   T30 as incompatible with this game on the Thor.

## What was verified on device with the final build

- Vulkan 1.3 instance, `dynamic rendering enabled`, chain created with `(dynamic rendering on)`.
- Rendering identical by inspection (title screen, both drivers, both builds); stretch mode (D2 path)
  renders correctly.
- Driver manager UI (Graphics Settings > GPU Driver), all on device:
  - Header: `GPU: Adreno (TM) 740 · Vulkan 1.3.128 · driver 0.676.53` (the driver-version
    formatting is upstream's `VK_API_VERSION_*` split of a Qualcomm 512.x number — pre-existing).
  - `Recommended source: Balemuni · Aurora`.
  - Legacy `Extracted/` Turnip migrated on first open to
    `Installed/turnip_adreno_driver_t30___mr_purple_666_-vt30/`, `DriverPackage` written.
  - Installed list: System driver, Balemuni (after download), MrPurple T30; `● Active` follows the
    selection (first-render ordering bug found and fixed in commit 32cc9e943e).
  - Row tap → dialog with name, description, Delete / Cancel / Use this driver. "Use" wrote
    `DriverLibName = vulkan.freedreno.so`, `DriverPackage = balemuni-v2-balemuni_apex_v2_ultimate_sd8gen2`.
  - Download drivers…: fetch of the seven sources took ~20 s; source picker listed Balemuni (5,
    Recommended), K11MCH1 (63), purple-turnip (30), StevenMXZ (43), crueter (7) + two more below;
    asset picker `release · asset (MB)` with the first item preselected; download + install of the
    14 MB SD8 Gen 2 pack succeeded and the list refreshed with the pack's real `meta.json`.
  - Not exercised on device: Delete, "Install from file…" (SAF picker), bare-`.so` synthesis
    (covered by JVM tests).

## Reproduction

`run.sh <label> <GFX.ini>` launches via MainActivity + tap on the Rogue Leader card, waits 130 s,
pulls logs; `stats.py <labels…>` skips the first 30 s. Both are reproduced below because `/tmp` was
wiped during the session.

```sh
#!/bin/sh
# run.sh <label> <gfx.ini>
LABEL=$1; INI=$2
PKG=org.dolphinemu.dolphinemu
FILES=/sdcard/Android/data/$PKG/files
OUT=/tmp/thor-exp/$LABEL; mkdir -p "$OUT"
adb shell am force-stop $PKG
adb push "$INI" $FILES/Config/GFX.ini >/dev/null
adb shell rm -f $FILES/Logs/render_times.txt $FILES/Logs/vblank_times.txt
adb logcat -c
adb shell input keyevent KEYCODE_WAKEUP; sleep 1
adb shell am start -W -n $PKG/.ui.main.MainActivity >/dev/null 2>&1; sleep 10
adb shell input tap 1440 990          # Rogue Leader card
sleep 130
adb exec-out screencap -p > "$OUT/screen.png"
adb logcat -d > "$OUT/logcat-full.txt" 2>/dev/null
adb pull $FILES/Logs/render_times.txt "$OUT/" >/dev/null 2>&1
adb pull $FILES/Logs/vblank_times.txt "$OUT/" >/dev/null 2>&1
adb shell am force-stop $PKG
```

```python
# stats.py <labels...>  -- mean/median/p95/p99 frame time after a 30 s warm-up skip
import sys
def stats(path, skip_s=30.0):
    xs=[float(l) for l in open(path) if l.strip()]
    t=0.0; keep=[]
    for x in xs:
        t+=x
        if t>skip_s*1000: keep.append(x)
    keep.sort(); n=len(keep)
    if not n: return None
    mean=sum(keep)/n
    return dict(n=n, secs=sum(keep)/1000, mean_ms=mean, med_ms=keep[n//2],
                p95_ms=keep[int(n*0.95)-1], p99_ms=keep[int(n*0.99)-1], fps=1000/mean)
for lab in sys.argv[1:]:
    for f in ("render_times.txt","vblank_times.txt"):
        try: s=stats(f"/tmp/thor-exp/{lab}/{f}")
        except FileNotFoundError: s=None
        print(lab, f, s)
```
