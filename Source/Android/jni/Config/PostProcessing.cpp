// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string>
#include <vector>

#include <jni.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "VideoCommon/PostProcessing/LibrashaderLoader.h"
#include "VideoCommon/PostProcessing/MultipassPostProcessing.h"
#include "VideoCommon/PostProcessing/ShaderPackDownload.h"
#include "VideoCommon/PostProcessing/ShaderParameterEditor.h"
#include "jni/AndroidCommon/AndroidCommon.h"

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getShaderList(JNIEnv* env,
                                                                                    jclass)
{
  return SpanToJStringArray(env, VideoCommon::MultipassPostProcessing::GetPresetList());
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_downloadShaderPack(
    JNIEnv* env, jclass, jstring pack_id, jstring profile)
{
  const std::string id = GetJString(env, pack_id);
  const std::string profile_str = GetJString(env, profile);
  const VideoCommon::ShaderPackDownloadResult result = VideoCommon::DownloadShaderPackById(
      id, File::GetUserPath(D_SHADERS_IDX), /*progress=*/nullptr, profile_str);
  return result.ok ? static_cast<jint>(result.preset_count) : -1;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getParameterEditingAvailability(
    JNIEnv* env, jclass, jstring video_backend, jstring preset_spec)
{
  // GetAvailability() loads librashader once per process; later calls are a cached lookup.
  return static_cast<jint>(VideoCommon::GetParameterEditingAvailability(
      VideoCommon::Librashader::GetAvailability().available, GetJString(env, video_backend),
      GetJString(env, preset_spec)));
}
}
