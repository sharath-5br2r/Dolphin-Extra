# librashader

**Version:** `librashader-cache-v0.12.0`  
**Upstream:** https://github.com/SnowflakePowered/librashader  
**License:** MPL-2.0 OR GPL-3.0-only

## Build Configuration

This directory contains prebuilt shared libraries and C headers for librashader. The desktop builds (macOS arm64, Windows x64) enable the native-backend runtimes (Metal, D3D11, D3D12, OpenGL) in addition to Vulkan. The Android arm64 binary (Vulkan-only) is vendored separately in `Source/Android/app/src/main/jniLibs/arm64-v8a/`.

### Vendored binaries and their hashes

Every binary in this tree, with the SHA-256 of the file as committed. Sizes alone cannot tell one build of the same source from another; these can, which is what makes "is the binary in my tree the one this file describes?" an answerable question — and it has to be answerable, because none of these are built from source by Dolphin's build system.

| file | bytes | SHA-256 |
|---|---|---|
| `lib/macos-arm64/librashader.dylib` | 12,480,240 | `f583b4f792a593fa26b711df45c584516920a00c5bd1b8ce8737e897d07537e2` |
| `lib/windows-x64/librashader.dll` | 38,358,016 | `a4674a7424ccc4e1eb1c6376be03f4a2d0c39ebad1b29df9a2f0b3a2328bf7e3` |
| `Source/Android/.../arm64-v8a/librashader.so` | 13,472,152 | `ef6e2006b995b5207c5c446f436e178ee11a9a9802b088e21a7ba61f1b4e374e` |
| `Source/Android/.../arm64-v8a/libc++_shared.so` | 9,290,184 | `0c52cfab2df0d957d8b346a2bdc5ae8d71feca2591924d77e1cd724d5bf74352` |

Recompute with `shasum -a 256 <file>` (macOS/Linux) or `certutil -hashfile <file> SHA256` (Windows). `libc++_shared.so` is an NDK artifact rather than a librashader build product, listed because it is vendored for librashader's sake and nothing else (see Dependencies below).

### Build Environment
- **NDK:** `29.0.14206865`
- **Target:** `aarch64-linux-android`
- **API Level:** 24 (specified in linker)

### Build Command
```bash
cargo build -p librashader-capi --release --target aarch64-linux-android \
  --no-default-features --features runtime-vulkan
```

### Cargo Configuration (`.cargo/config.toml`)
```toml
[target.aarch64-linux-android]
linker = "aarch64-linux-android24-clang"
```

### Environment Variables
```bash
export AR_aarch64_linux_android=llvm-ar
export CC_aarch64_linux_android=aarch64-linux-android24-clang
export CXX_aarch64_linux_android=aarch64-linux-android24-clang++
export RANLIB_aarch64_linux_android=llvm-ranlib
```

### Artifact Details
- **Library:** `librashader.so` (~13.5 MB)
- **ABI:** 2
- **API:** 5
- **Headers:** `librashader.h`, `librashader_ld.h`

The `cargo build` output is named `liblibrashader_capi.so`; it is vendored as
`librashader.so` because `librashader_ld.h` loads the runtime with a hardcoded
`dlopen("librashader.so", RTLD_LAZY)` on Android/Linux. The file carries no
SONAME, so the rename is safe — nothing references it by an embedded name.

### Dependencies
The prebuilt `librashader.so` requires `libc++_shared.so` as a NEEDED dependency. Both are vendored into `Source/Android/app/src/main/jniLibs/arm64-v8a/` to ensure runtime availability. Dolphin builds with `ANDROID_STL=c++_static`, so `libc++_shared.so` is not otherwise present in the APK. The two libc++ instances do not clash because librashader's ABI boundary is plain C.

### Architecture Support
Only `arm64-v8a` is supported. On Android, the `x86_64` ABI falls back to Dolphin's built-in post-processing. On desktop Linux, no librashader binary is vendored, so all architectures fall back to the built-in executor (§4.3 of the design spec).

## Desktop Builds

### macOS arm64

**Build command** (executed on macOS arm64):
```bash
rustup target add aarch64-apple-darwin
git clone --branch librashader-cache-v0.12.0 --depth 1 \
    https://github.com/SnowflakePowered/librashader /tmp/librashader
cd /tmp/librashader
cargo build -p librashader-capi --release --target aarch64-apple-darwin \
    --no-default-features --features runtime-vulkan,runtime-metal,runtime-opengl
```

**Artifact:**
- **Path:** `lib/macos-arm64/librashader.dylib`
- **Size:** 12,480,240 bytes
- **Source:** `target/aarch64-apple-darwin/release/liblibrashader_capi.dylib` (renamed)
- **ABI:** 2 / **API:** 5
- **Exported symbol families:** `libra_vk_*` (8), `libra_mtl_*` (8), `libra_gl_*` (7) — total 52 `libra_` exports

### Windows x64

**Build command** (executed on Windows x64 with VS 18 Community):
```batch
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set RUSTC=C:\Users\Ilya\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin\rustc.exe
cd /d C:\src\librashader
"C:\Users\Ilya\.rustup\toolchains\stable-x86_64-pc-windows-msvc\bin\cargo.exe" build -p librashader-capi --release --target x86_64-pc-windows-msvc --no-default-features --features runtime-vulkan,runtime-d3d11,runtime-d3d12-static,runtime-opengl
```

**Note:** The build invokes the real toolchain binaries directly rather than `rustup run stable cargo` because rustup shims in `C:\Users\Ilya\.cargo\bin\` cannot be launched from ssh sessions (error 448 / "untrusted mount point").

**Feature rationale:** `runtime-d3d12-static` rather than `runtime-d3d12` links `mach-dxcompiler-rs`'s `DxcCreateInstance` in statically (`librashader-runtime-d3d12/src/util.rs:233-252`) instead of importing `dxcompiler.dll`, so no extra DLL has to be packaged with Dolphin. The DLL carries no `dxcompiler` dependency in its import table.

**Source revision: not recorded.** The macOS build above pins its source (`--branch librashader-cache-v0.12.0 --depth 1`). This one was built from a pre-existing working copy at `C:\src\librashader` on the UAT host, and neither a tag nor a commit was written down, so this file cannot say which revision produced the DLL. What the artifact itself supports: ABI 2 / API 5, matching the macOS build; panic strings carrying paths like `C:\src\librashader\vendor\aho-corasick-1.1.4\src\...` and `librashader-runtime-d3d12\src\filter_chain.rs`, i.e. a working copy rather than a registry checkout, which is why they carry no crate version; and no version or commit string embedded anywhere in the binary. That is corroboration that the two builds are the same generation of the code, not proof that they are the same revision. To close the gap, run `git -C C:\src\librashader rev-parse HEAD` and `git -C C:\src\librashader describe --tags --always` on that host and record both here. Until then the Windows DLL is reproducible only from that machine.

**Import table**, read directly from the vendored DLL (PE import and delay-load directories, 2026-09-18):

- No `dxcompiler.dll`, in either directory. That is the checkable form of the `runtime-d3d12-static` claim above.
- `d3d12.dll` is **delay-loaded**, so it is resolved on first use rather than at load time.
- `d3dcompiler_47.dll` is a **static** import — `D3DCompile` and `D3DCreateBlob`, for the D3D11 runtime's HLSL path — and this is worth knowing because Dolphin's own use of that same DLL is deliberately not static. `D3DCommon::LoadLibraries()` opens it by name at D3D backend startup (`Source/Core/VideoBackends/D3DCommon/D3DCommon.cpp:42-58`) and, when it is missing, panics with the Windows 7 KB hint and fails *that backend only*. librashader cannot degrade that way: a static import means `LoadLibraryW("librashader.dll")` fails outright, `GetAvailability()` reports unavailable, and every backend — Vulkan and OpenGL included — quietly falls back to the built-in post-processor. On a supported host this is unreachable: `d3dcompiler_47.dll` has been in-box since Windows 8.1 and Dolphin requires Windows 10 1903 (`README.md:16`). It is recorded because it makes librashader's availability depend on a Direct3D DLL even on a non-Direct3D backend, which nothing else in the tree would tell a reader.
- Also static: `MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll` — the VC++ redistributable that Dolphin's own MSVC build already requires — plus `bcryptprimitives`, `combase`, `ole32`, `oleaut32`, `shell32`, `user32`, `ntdll`, `kernel32` and the usual `api-ms-win-crt-*` set.

**Artifact:**
- **Path:** `lib/windows-x64/librashader.dll`
- **Size:** 38,358,016 bytes
- **Source:** `target\x86_64-pc-windows-msvc\release\librashader_capi.dll` (renamed)
- **ABI:** 2 / **API:** 5
- **Exported symbol families:** `libra_vk_*` (8), `libra_d3d11_*` (8), `libra_d3d12_*` (8), `libra_gl_*` (7) — total 60 `libra_` exports

**OpenGL runtime note:** `libra_gl_*` exports 7 symbols, not 8, because the OpenGL runtime has no `*_create_deferred` function — deferred creation exists for command-buffer APIs (Vulkan, D3D11, D3D12, Metal), but OpenGL is immediate-mode and submits commands synchronously.

The desktop builds now enable the native-backend runtimes in addition to Vulkan. ABI 2 / API 5 is unchanged from the original Android-only build.

## Local Modifications

The headers are vendored unmodified. `librashader_ld.h` is kept for reference only — nothing in Dolphin includes it.

Dolphin resolves the C API itself, in `Source/Core/VideoCommon/PostProcessing/LibrashaderLoader.cpp`. The header cannot be used as intended here because it declares a single `libra_instance_t` whose members are gated by the `LIBRA_RUNTIME_*` macros and loads it from a `static inline` function: two translation units including it with different runtime sets would disagree on that struct's layout, and driving every backend from one translation unit would mean including the Vulkan, D3D11, D3D12, OpenGL and Metal headers together. Dolphin therefore includes plain `librashader.h` for the `PFN_libra_*` typedefs and resolves symbols through one `dlopen`/`LoadLibraryW`, which also lets it supply the absolute packaged path that the header's bare-name load cannot find inside a macOS `.app` bundle or a Windows install directory.

An earlier revision of this tree patched the header's three `_LIBRASHADER_LOAD` defines to be overridable. That patch has been reverted; do not reintroduce it.

**macOS dylib install name:**  
The macOS dylib's `LC_ID_DYLIB` install name still references the build-time path (`/private/tmp/librashader/target/aarch64-apple-darwin/release/deps/liblibrashader_capi.dylib`). This is harmless because `dlopen` with an absolute path ignores the install name. Do not rewrite it with `install_name_tool` unless `POSTPROCESS_BUNDLE=ON` (default OFF) requires it — the current packaging has been verified to work as-is.
