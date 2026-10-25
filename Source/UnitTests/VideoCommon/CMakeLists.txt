add_dolphin_test(VertexLoaderTest VertexLoaderTest.cpp)
add_dolphin_test(VideoConfigTest VideoConfigTest.cpp)
add_dolphin_test(PerformanceMetricsTest PerformanceMetricsTest.cpp)
add_dolphin_test(PresentationScalingTest PresentationScalingTest.cpp)
add_dolphin_test(NetPlayChatUITest NetPlayChatUITest.cpp)
target_link_libraries(NetPlayChatUITest PRIVATE imgui)
add_dolphin_test(BigPictureNavigationUITest BigPictureNavigationUITest.cpp)
target_link_libraries(BigPictureNavigationUITest PRIVATE imgui)
add_dolphin_test(BigPictureInGameMenuTest BigPictureInGameMenuTest.cpp)
target_link_libraries(BigPictureInGameMenuTest PRIVATE imgui)
add_dolphin_test(BigPictureStandaloneSessionTest BigPictureStandaloneSessionTest.cpp)
add_dolphin_test(SlangPresetTest PostProcessing/SlangPresetTest.cpp)
add_dolphin_test(SlangShaderTest PostProcessing/SlangShaderTest.cpp)
add_dolphin_test(PassSizingTest PostProcessing/PassSizingTest.cpp)
add_dolphin_test(SlangTranslatorTest PostProcessing/SlangTranslatorTest.cpp)
add_dolphin_test(SlangSamplersTest PostProcessing/SlangSamplersTest.cpp)
add_dolphin_test(MipGenTest PostProcessing/MipGenTest.cpp)
add_dolphin_test(PassGraphTest PostProcessing/PassGraphTest.cpp)
add_dolphin_test(PresetArchiveTest PostProcessing/PresetArchiveTest.cpp)
add_dolphin_test(ShaderPackDownloadTest PostProcessing/ShaderPackDownloadTest.cpp)
add_dolphin_test(ShaderPackSourceTest PostProcessing/ShaderPackSourceTest.cpp)
add_dolphin_test(PresetNameTest PostProcessing/PresetNameTest.cpp)
add_dolphin_test(SlangCompileTest PostProcessing/SlangCompileTest.cpp)
# SlangCompileTest cross-compiles its SPIR-V to HLSL/MSL, so it needs what the D3D and Metal
# backends need. glslang alone cannot see a sampler-dimension mismatch: `sampler2D` is valid GLSL.
# Only link it where it exists: the top-level CMakeLists adds Externals/spirv_cross under
# `if(WIN32 OR APPLE)`, and naming a target that was never defined makes CMake fall back to a raw
# `-lspirv_cross`, which fails at link time on Android and Linux. The test guards the matching code
# on DOLPHIN_TEST_HAS_SPIRV_CROSS; keep the two conditions in step.
if(WIN32 OR APPLE)
  target_link_libraries(SlangCompileTest PRIVATE spirv_cross)
endif()
# glslang-default-resource-limits is for the low-GLSL-version rows, which drive glslang's front end
# directly instead of going through SPIRV::Compile* (that path cannot target below 310 es).
target_link_libraries(SlangCompileTest PRIVATE glslang-default-resource-limits)
add_dolphin_test(RetroCrisisInstallTest PostProcessing/RetroCrisisInstallTest.cpp)
add_dolphin_test(PostProcessingConfigTest PostProcessing/PostProcessingConfigTest.cpp)
add_dolphin_test(SlangSourceDownscaleTest PostProcessing/SlangSourceDownscaleTest.cpp)
add_dolphin_test(ChainOutputPolicyTest PostProcessing/ChainOutputPolicyTest.cpp)
add_dolphin_test(ChainDebugDumpTest PostProcessing/ChainDebugDumpTest.cpp)
add_dolphin_test(LibrashaderLoaderTest PostProcessing/LibrashaderLoaderTest.cpp)
add_dolphin_test(LibrashaderParametersTest PostProcessing/LibrashaderParametersTest.cpp)
add_dolphin_test(ShaderParameterEditorTest PostProcessing/ShaderParameterEditorTest.cpp)
add_dolphin_test(PresetTreeTest PostProcessing/PresetTreeTest.cpp)
