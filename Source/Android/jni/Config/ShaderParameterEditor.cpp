// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <string>

#include <jni.h>

#include "VideoCommon/PostProcessing/ShaderParameterEditor.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#include "jni/AndroidCommon/IDCache.h"

using VideoCommon::ShaderParameterEditor;

static ShaderParameterEditor* GetPointer(JNIEnv* env, jobject obj)
{
  return reinterpret_cast<ShaderParameterEditor*>(
      env->GetLongField(obj, IDCache::GetShaderParameterEditorPointer()));
}

static ShaderParameterEditor& GetReference(JNIEnv* env, jobject obj)
{
  return *GetPointer(env, obj);
}

static const ShaderParameterEditor::Row& GetRow(JNIEnv* env, jobject obj, jint index)
{
  return GetReference(env, obj).GetRows().at(static_cast<size_t>(index));
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_initialize(
    JNIEnv* env, jclass, jstring preset_spec)
{
  return reinterpret_cast<jlong>(ShaderParameterEditor::Open(GetJString(env, preset_spec)).release());
}

JNIEXPORT void JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_finalize(JNIEnv* env,
                                                                                     jobject obj)
{
  delete GetPointer(env, obj);
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getStatus(JNIEnv* env,
                                                                                      jobject obj)
{
  return static_cast<jint>(GetReference(env, obj).GetStatus());
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getError(JNIEnv* env,
                                                                                     jobject obj)
{
  return ToJString(env, GetReference(env, obj).GetError());
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getParameterCount(
    JNIEnv* env, jobject obj)
{
  return static_cast<jint>(GetReference(env, obj).GetRows().size());
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getName(JNIEnv* env,
                                                                                    jobject obj,
                                                                                    jint index)
{
  return ToJString(env, GetRow(env, obj, index).info.name);
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getDescription(
    JNIEnv* env, jobject obj, jint index)
{
  return ToJString(env, GetRow(env, obj, index).info.description);
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getValue(JNIEnv* env,
                                                                                     jobject obj,
                                                                                     jint index)
{
  return GetRow(env, obj, index).value;
}

JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getValueText(
    JNIEnv* env, jobject obj, jint index)
{
  return ToJString(env, GetReference(env, obj).FormatValue(static_cast<size_t>(index)));
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getMinimum(
    JNIEnv* env, jobject obj, jint index)
{
  return GetRow(env, obj, index).info.minimum;
}

JNIEXPORT jfloat JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getMaximum(
    JNIEnv* env, jobject obj, jint index)
{
  return GetRow(env, obj, index).info.maximum;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getSliderPositions(
    JNIEnv* env, jobject obj, jint index)
{
  return GetRow(env, obj, index).slider.positions;
}

JNIEXPORT jint JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_getSliderPosition(
    JNIEnv* env, jobject obj, jint index)
{
  return GetReference(env, obj).GetSliderPosition(static_cast<size_t>(index));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_isDefault(JNIEnv* env,
                                                                                      jobject obj,
                                                                                      jint index)
{
  return static_cast<jboolean>(GetReference(env, obj).IsDefault(static_cast<size_t>(index)));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_setValue(JNIEnv* env,
                                                                                     jobject obj,
                                                                                     jint index,
                                                                                     jfloat value)
{
  return static_cast<jboolean>(GetReference(env, obj).SetValue(static_cast<size_t>(index), value));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_setSliderPosition(
    JNIEnv* env, jobject obj, jint index, jint position)
{
  return static_cast<jboolean>(
      GetReference(env, obj).SetSliderPosition(static_cast<size_t>(index), position));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_reset(JNIEnv* env,
                                                                                  jobject obj,
                                                                                  jint index)
{
  return static_cast<jboolean>(GetReference(env, obj).Reset(static_cast<size_t>(index)));
}

JNIEXPORT jboolean JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_ShaderParameterEditor_resetAll(JNIEnv* env,
                                                                                     jobject obj)
{
  return static_cast<jboolean>(GetReference(env, obj).ResetAll());
}
}
