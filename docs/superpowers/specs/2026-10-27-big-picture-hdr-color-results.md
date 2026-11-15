# Big Picture HDR artwork color correction

The reported desaturation reproduced with HDR output enabled. The ImGui pipeline targets a
linear RGBA16F scRGB backbuffer in that mode, but the old fragment shader only linearized
vertex colors. Sampled RGBA8 artwork remained encoded as sRGB and was written as linear
light. For a neutral 128/255 texture pixel, native D3D11 GPU readback was 0.501953 instead of
the correct 0.215861. Non-neutral pixels exhibited similarly elevated midtones.

The UI shader now applies the piecewise sRGB transfer function to texture RGB and vertex
RGB independently, before multiplying them. Texture and vertex alpha remain coverage, and
their product is unchanged. The SDR path retains its texture-times-tint calculation. No
emulated rendering, user graphics settings, or postprocessing configuration was changed.

Validation on 2026-10-27:

- A temporary native host-UI draw hook drew eight swatches with known texture/tint values,
  including gray, colored midtones, the low sRGB segment, black/white endpoints, tinted
  artwork, and combined texture/tint transparency over an opaque gray background.
- An ImGui draw callback read back the actual backbuffer after drawing the swatches, then
  compared RGB against an independent CPU reference. All 24 channel comparisons passed
  for each of D3D11, D3D12, and Vulkan, in both RGBA16F HDR and packed RGB10_A2 SDR output.
  HDR tolerance was 0.0015; SDR tolerance was 2/255. Corrected HDR middle gray was 0.215820.
- The permanent shader regression test compiles 16 combinations: OpenGL, D3D, Vulkan, and
  Metal; both varying layouts; SDR and linear output. All compiled successfully.
- All 119 selected shader, Big Picture, and presentation-scaling unit tests passed.
- Temporary probe code and isolated profiles are ignored build artifacts. The probe hook
  was removed from the normal Dolphin target before producing the user-testing executable.

These checks verify GPU framebuffer values. They do not measure a physical display's HDR
tone mapping or tune HDR paper-white brightness.
