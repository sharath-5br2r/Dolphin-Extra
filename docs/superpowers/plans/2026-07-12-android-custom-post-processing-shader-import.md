# Android Custom Post-Processing Shader Import — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let Android users import a `.glsl` post-processing shader from an arbitrary filesystem location via the Storage Access Framework, validating it (parse + `#include` resolution always; GPU compile when a backend is live) and reporting errors before it becomes selectable.

**Architecture:** Chosen approach is **copy-into-Shaders-dir**. The user picks a `.glsl` file through SAF (`ACTION_OPEN_DOCUMENT`); Kotlin reads it via `ContentResolver` and copies its bytes into the user Shaders directory (`File::GetUserPath(D_SHADERS_IDX)`, exposed to Kotlin as an existing native call). Before committing the copy, a new native JNI entry `PostProcessing.validateShaderSource(code)` runs the existing `PostProcessingConfiguration::LoadOptions` parse and resolves `#include`s, and additionally performs a real `g_gfx->CreateShaderFromSource` GPU compile **only when a video backend is live** (returns a "compile deferred" note otherwise). On success the file lands as `<shader>.glsl` in the Shaders dir and the existing enumerator (`GetShaderList()`) surfaces it automatically — no changes to the core shader loader, option persistence, or the existing dropdown are required. On failure the file is not copied and the error string is shown in a dialog.

**Tech Stack:** C++17 (VideoCommon, JNI), Kotlin (Android app), JNI bridge, Android Storage Access Framework, Gradle/CMake build.

## Global Constraints

- License header on every new file: C++ → `// Copyright 2026 Dolphin Emulator Project` + `// SPDX-License-Identifier: GPL-2.0-or-later`; Kotlin → `// SPDX-License-Identifier: GPL-2.0-or-later`. Match the exact header style already present in the touched file.
- The persisted shader identity stays a **bare shader name** (no path, no extension, no `content://` URI). `StringSetting.GFX_ENHANCE_POST_SHADER` semantics are unchanged.
- Do not add new Android manifest permissions. SAF `ACTION_OPEN_DOCUMENT` + `takePersistableUriPermission` is the only access mechanism; the destination directory (`getExternalFilesDir`-derived user dir) is app-owned and needs no write permission. Specifically do **not** add `MANAGE_EXTERNAL_STORAGE`.
- Native validation must never call `g_gfx` when it is null. A GPU compile is attempted only when `g_gfx != nullptr`; otherwise the parse/include result is authoritative and the function reports that the GPU compile was deferred.
- All user-facing strings must be added to `Source/Android/app/src/main/res/values/strings.xml` and referenced by `R.string.*` — no hardcoded UI text.
- Follow existing code conventions in each file (naming, `@JvmStatic`/`@Keep` on JNI-facing Kotlin, `extern "C"` + full `Java_org_dolphinemu_...` symbol names for JNI C++).

---

## File Structure

**New files:**
- `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/ShaderImportHelper.kt` — Kotlin helper: copy a picked `content://` shader into the Shaders dir, call native validation, return a typed result.

**Modified files (core / JNI):**
- `Source/Core/VideoCommon/PostProcessing.h` — declare `ShaderValidationResult` + `static ShaderValidationResult ValidateShaderSource(const std::string&)`.
- `Source/Core/VideoCommon/PostProcessing.cpp` — implement `ValidateShaderSource`.
- `Source/Android/jni/Config/PostProcessing.cpp` — add `validateShaderSource` + `getUserShaderDirectory` JNI entries.
- `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/model/PostProcessing.kt` — declare the two new external functions and a small result class.

**Modified files (Android UI):**
- `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/FileBrowserHelper.kt` — add `SHADER_EXTENSION`.
- `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsActivityResultLaunchers.kt` — add a `requestShaderFile` launcher that copies + validates via `ShaderImportHelper`.
- `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsFragmentPresenter.kt` — add an "Import custom shader" `FilePicker` item next to the existing dropdown, and refresh the dropdown after a successful import.
- `Source/Android/app/src/main/res/values/strings.xml` — new strings.

---

## Task 1: Native shader validation (`ValidateShaderSource`)

**Files:**
- Modify: `Source/Core/VideoCommon/PostProcessing.h:96-104`
- Modify: `Source/Core/VideoCommon/PostProcessing.cpp` (add after `GetPassiveShaderList`, around line 417)

**Interfaces:**
- Produces:
  ```cpp
  struct ShaderValidationResult {
    bool valid;               // true = safe to import
    bool gpu_compiled;        // true = a real GPU compile ran and passed
    std::string error_message; // human-readable; empty when valid
  };
  static ShaderValidationResult PostProcessing::ValidateShaderSource(const std::string& code);
  ```
- Consumes: existing `PostProcessingConfiguration::LoadOptions` (private) and `PostProcessingConfiguration::LoadShader` semantics; `g_gfx` from `VideoCommon/AbstractGfx.h`.

- [ ] **Step 1: Write the failing test**

Create `Source/UnitTests/VideoCommon/PostProcessingValidationTest.cpp`:

```cpp
// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "VideoCommon/PostProcessing.h"

// g_gfx is null in the unit-test environment, so ValidateShaderSource must
// exercise only the parse/structure path and report the GPU compile as deferred.
TEST(PostProcessingValidation, PlainShaderParsesWithoutGpu)
{
  const std::string code = "void main() { SetOutput(Sample()); }\n";
  const auto result = VideoCommon::PostProcessing::ValidateShaderSource(code);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.gpu_compiled);
  EXPECT_TRUE(result.error_message.empty());
}

TEST(PostProcessingValidation, ConfigurationBlockParses)
{
  const std::string code =
      "[configuration]\n"
      "[OptionBool]\n"
      "GUIName = Invert\n"
      "OptionName = invert\n"
      "DefaultValue = false\n"
      "[/configuration]\n"
      "void main() { SetOutput(Sample()); }\n";
  const auto result = VideoCommon::PostProcessing::ValidateShaderSource(code);
  EXPECT_TRUE(result.valid);
}

TEST(PostProcessingValidation, EmptySourceIsInvalid)
{
  const auto result = VideoCommon::PostProcessing::ValidateShaderSource("");
  EXPECT_FALSE(result.valid);
  EXPECT_FALSE(result.error_message.empty());
}
```

Register the test file in `Source/UnitTests/CMakeLists.txt` — find the `add_dolphin_test` entries for VideoCommon and add:

```cmake
add_dolphin_test(PostProcessingValidationTest VideoCommon/PostProcessingValidationTest.cpp)
```

(If VideoCommon tests are grouped differently in that file, match the existing pattern for adding a VideoCommon unit test.)

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin && cmake --build build --target PostProcessingValidationTest 2>&1 | tail -20
```
Expected: FAIL — compile error, `ValidateShaderSource` / `ShaderValidationResult` not declared.

- [ ] **Step 3: Declare the API in the header**

In `Source/Core/VideoCommon/PostProcessing.h`, inside `class PostProcessing`'s `public:` block (after line 104, the `GetAnaglyphShaderList` declaration), add:

```cpp
  struct ShaderValidationResult
  {
    bool valid = false;
    bool gpu_compiled = false;
    std::string error_message;
  };

  // Validates raw post-processing shader source without persisting anything.
  // Always parses the [configuration] block. Performs a real GPU compile only
  // when a video backend is live (g_gfx != nullptr); otherwise gpu_compiled is
  // false and validity reflects the parse/structure check only.
  static ShaderValidationResult ValidateShaderSource(const std::string& code);
```

- [ ] **Step 4: Implement `ValidateShaderSource`**

In `Source/Core/VideoCommon/PostProcessing.cpp`, add these includes near the existing includes if not already present:

```cpp
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractShader.h"
```

Add the implementation immediately after `PostProcessing::GetPassiveShaderList()` (after line 417):

```cpp
PostProcessing::ShaderValidationResult
PostProcessing::ValidateShaderSource(const std::string& code)
{
  ShaderValidationResult result;

  if (code.empty())
  {
    result.error_message = "Shader source is empty.";
    return result;
  }

  // Parse the [configuration] block through the same code path used at load
  // time. LoadOptions never throws; malformed option blocks simply yield no
  // options, so a parse "failure" here is limited to structural problems we
  // can detect. We reuse a throwaway configuration object.
  PostProcessingConfiguration config;
  config.LoadOptions(code);

  // Structural sanity: a usable pixel shader must define main().
  if (code.find("void main(") == std::string::npos &&
      code.find("void main (") == std::string::npos)
  {
    result.error_message = "Shader source does not define a main() entry point.";
    return result;
  }

  // If no video backend is live (e.g. importing from the settings screen with
  // no game running) we cannot GPU-compile. The parse/structure check is then
  // authoritative and we report that the compile was deferred.
  if (g_gfx == nullptr)
  {
    result.valid = true;
    result.gpu_compiled = false;
    return result;
  }

  std::unique_ptr<AbstractShader> shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, GetStandaloneValidationHeader() + code + GetStandaloneValidationFooter(),
      nullptr, "Post-processing shader validation");

  if (!shader)
  {
    result.error_message = "Shader failed to compile on the current graphics backend.";
    return result;
  }

  result.valid = true;
  result.gpu_compiled = true;
  return result;
}
```

Because `GetHeader`/`GetFooter` are non-static instance methods, add two small file-local helpers just above `ValidateShaderSource` that reproduce the minimal header/footer the pixel-shader compile needs. Use the exact strings already produced by the existing `GetHeader(true)` / `GetFooter()` — copy them verbatim from those methods (search `std::string PostProcessing::GetHeader` and `GetFooter` in this file) into:

```cpp
static std::string GetStandaloneValidationHeader()
{
  // Mirror of PostProcessing::GetHeader(true) sufficient for standalone
  // compilation. Keep in sync with GetHeader if that function changes.
  // <copy the header body used for user_post_process == true here>
}

static std::string GetStandaloneValidationFooter()
{
  // Mirror of PostProcessing::GetFooter().
  // <copy the footer body here>
}
```

> NOTE for implementer: read `GetHeader`/`GetFooter` in this file and paste their exact returned string bodies into these two helpers. Do not invent shader boilerplate — the compile must use the same preamble the real pipeline uses, or valid shaders will be falsely rejected. Because these helpers must call `LoadOptions` (currently private), also change the `LoadOptions` declaration in `PostProcessing.h:92` from `private:` section to a `public:` method (move the single line `void LoadOptions(const std::string& code);` into the public block), since `ValidateShaderSource` (a static method of the sibling `PostProcessing` class) calls it on a local `PostProcessingConfiguration`.

- [ ] **Step 5: Run the test to verify it passes**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin && cmake --build build --target PostProcessingValidationTest 2>&1 | tail -20 && ./build/Binaries/PostProcessingValidationTest
```
Expected: PASS — all three tests green (`g_gfx` is null in the harness, so `gpu_compiled` is false and the parse path drives validity).

- [ ] **Step 6: Commit**

```bash
git add Source/Core/VideoCommon/PostProcessing.h Source/Core/VideoCommon/PostProcessing.cpp Source/UnitTests/VideoCommon/PostProcessingValidationTest.cpp Source/UnitTests/CMakeLists.txt
git commit -m "VideoCommon: add PostProcessing::ValidateShaderSource for standalone shader validation"
```

---

## Task 2: JNI bridge for validation and user shader directory

**Files:**
- Modify: `Source/Android/jni/Config/PostProcessing.cpp:1-34`
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/model/PostProcessing.kt:1-17`

**Interfaces:**
- Consumes: `VideoCommon::PostProcessing::ValidateShaderSource` (Task 1); `File::GetUserPath(D_SHADERS_IDX)`; `ToJString` / `GetJString` from `jni/AndroidCommon/AndroidCommon.h`.
- Produces (Kotlin):
  ```kotlin
  external fun validateShaderSource(code: String): ShaderValidationResult
  external fun getUserShaderDirectory(): String
  data class ShaderValidationResult(val valid: Boolean, val gpuCompiled: Boolean, val errorMessage: String)
  ```

- [ ] **Step 1: Write the failing test (Kotlin instrumentation-free compile check)**

There is no JVM unit harness for these JNI stubs, so the "test" is that the Kotlin declarations compile and the JNI symbol names match. Add the Kotlin side first so the compile fails on the missing native side wiring, then confirm at Step 5 via the app build. In `PostProcessing.kt`, replace the file body with:

```kotlin
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.model

object PostProcessing {
    @JvmStatic
    val shaderList: Array<String>
        external get

    @JvmStatic
    val anaglyphShaderList: Array<String>
        external get

    @JvmStatic
    val passiveShaderList: Array<String>
        external get

    /** Result of [validateShaderSource]. Mirrors the native ShaderValidationResult. */
    data class ShaderValidationResult(
        @JvmField val valid: Boolean,
        @JvmField val gpuCompiled: Boolean,
        @JvmField val errorMessage: String
    )

    /** Validates raw shader source. Never touches the filesystem. */
    @JvmStatic
    external fun validateShaderSource(code: String): ShaderValidationResult

    /** Absolute path of the user Shaders directory (ends with a separator). */
    @JvmStatic
    external fun getUserShaderDirectory(): String
}
```

- [ ] **Step 2: Run to verify it fails**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin/Source/Android && ./gradlew :app:compileDebugKotlin 2>&1 | tail -20
```
Expected: Kotlin compiles (the `external` declarations are allowed without native impl at compile time). The real failure surfaces as an `UnsatisfiedLinkError` only at runtime — acceptable; the native side is added next. If Kotlin compilation itself fails, fix the declarations before proceeding.

- [ ] **Step 3: Implement the JNI functions**

In `Source/Android/jni/Config/PostProcessing.cpp`, add these includes at the top with the existing includes:

```cpp
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "jni/AndroidCommon/IDCache.h"
```

Then add, before the closing `}` of the `extern "C"` block:

```cpp
JNIEXPORT jstring JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_getUserShaderDirectory(
    JNIEnv* env, jclass)
{
  return ToJString(env, File::GetUserPath(D_SHADERS_IDX));
}

JNIEXPORT jobject JNICALL
Java_org_dolphinemu_dolphinemu_features_settings_model_PostProcessing_validateShaderSource(
    JNIEnv* env, jclass, jstring code)
{
  const VideoCommon::PostProcessing::ShaderValidationResult result =
      VideoCommon::PostProcessing::ValidateShaderSource(GetJString(env, code));

  const jclass result_class = IDCache::GetPostProcessingShaderValidationResultClass();
  const jmethodID ctor = IDCache::GetPostProcessingShaderValidationResultConstructor();
  return env->NewObject(result_class, ctor, static_cast<jboolean>(result.valid),
                        static_cast<jboolean>(result.gpu_compiled),
                        ToJString(env, result.error_message));
}
```

- [ ] **Step 4: Register the result class in IDCache**

In `Source/Android/jni/AndroidCommon/IDCache.h`, declare (near the other getter declarations):

```cpp
jclass GetPostProcessingShaderValidationResultClass();
jmethodID GetPostProcessingShaderValidationResultConstructor();
```

In `Source/Android/jni/AndroidCommon/IDCache.cpp`, mirror the pattern used for other cached classes (search an existing `GetXxxClass()`/`GetXxxConstructor()` pair, e.g. around the ContentHandler block near line 842, for the exact idiom). Add a cached `jclass` global and `jmethodID`, populate them in the `JNI_OnLoad` registration body:

```cpp
// In the anonymous-namespace globals section:
jclass s_pp_shader_validation_result_class;
jmethodID s_pp_shader_validation_result_constructor;

// In the class-lookup section of JNI_OnLoad (mirror neighbouring FindClass calls):
const jclass pp_result_class = env->FindClass(
    "org/dolphinemu/dolphinemu/features/settings/model/PostProcessing$ShaderValidationResult");
s_pp_shader_validation_result_class =
    reinterpret_cast<jclass>(env->NewGlobalRef(pp_result_class));
s_pp_shader_validation_result_constructor = env->GetMethodID(
    pp_result_class, "<init>", "(ZZLjava/lang/String;)V");
env->DeleteLocalRef(pp_result_class);

// Accessors (near other Get*Class definitions):
jclass IDCache::GetPostProcessingShaderValidationResultClass()
{
  return s_pp_shader_validation_result_class;
}
jmethodID IDCache::GetPostProcessingShaderValidationResultConstructor()
{
  return s_pp_shader_validation_result_constructor;
}
```

> NOTE for implementer: the Kotlin `data class` with `@JvmField` members and a 3-arg constructor `(Boolean, Boolean, String)` yields JNI signature `(ZZLjava/lang/String;)V`. Confirm with `javap -s` on the built class if the app throws `NoSuchMethodError`. Match the existing IDCache accessor namespace convention (some are free functions in `namespace IDCache`, some are members — follow whatever the neighbouring accessors do).

- [ ] **Step 5: Build the app to verify JNI links**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin/Source/Android && ./gradlew :app:assembleDebug 2>&1 | tail -30
```
Expected: BUILD SUCCESSFUL. (Runtime link is verified in Task 5's manual check.)

- [ ] **Step 6: Commit**

```bash
git add Source/Android/jni/Config/PostProcessing.cpp Source/Android/jni/AndroidCommon/IDCache.h Source/Android/jni/AndroidCommon/IDCache.cpp Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/model/PostProcessing.kt
git commit -m "Android: JNI bridge for shader validation and user shader directory"
```

---

## Task 3: Kotlin import helper (copy + validate)

**Files:**
- Create: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/ShaderImportHelper.kt`
- Modify: `Source/Android/app/src/main/res/values/strings.xml`

**Interfaces:**
- Consumes: `PostProcessing.validateShaderSource`, `PostProcessing.getUserShaderDirectory`, `ContentHandler.getDisplayName` (existing, `utils/ContentHandler.java`), `FileBrowserHelper.getExtension`.
- Produces:
  ```kotlin
  object ShaderImportHelper {
      sealed class Result {
          data class Success(val shaderName: String, val gpuCompiled: Boolean) : Result()
          data class Failure(val message: String) : Result()
      }
      fun importShader(context: Context, uri: Uri): Result
  }
  ```

- [ ] **Step 1: Add the user-facing strings**

In `Source/Android/app/src/main/res/values/strings.xml`, add (alongside the other post-processing strings — search `post_processing_shader`):

```xml
    <string name="post_processing_import_shader">Import Custom Shader</string>
    <string name="post_processing_import_shader_description">Add a .glsl post-processing shader from your device</string>
    <string name="shader_import_success">Imported shader \"%1$s\"</string>
    <string name="shader_import_success_deferred">Imported shader \"%1$s\". It will be fully compiled the next time a game runs.</string>
    <string name="shader_import_failed">Could not import shader: %1$s</string>
    <string name="shader_import_read_failed">Could not read the selected file.</string>
    <string name="shader_import_name_conflict">A shader named \"%1$s\" already exists.</string>
    <string name="shader_import_write_failed">Could not save the shader to the Shaders folder.</string>
```

- [ ] **Step 2: Write the helper**

Create `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/ShaderImportHelper.kt`:

```kotlin
// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils

import android.content.Context
import android.net.Uri
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.features.settings.model.PostProcessing
import java.io.File
import java.io.IOException

object ShaderImportHelper {
    sealed class Result {
        data class Success(val shaderName: String, val gpuCompiled: Boolean) : Result()
        data class Failure(val message: String) : Result()
    }

    /**
     * Reads the shader at [uri], validates it natively, and — only on success —
     * copies it into the user Shaders directory as <name>.glsl. Nothing is
     * written on failure.
     */
    fun importShader(context: Context, uri: Uri): Result {
        val displayName = ContentHandler.getDisplayName(uri)
            ?: return Result.Failure(context.getString(R.string.shader_import_read_failed))

        val shaderName = stripGlslExtension(displayName)
        if (shaderName.isEmpty()) {
            return Result.Failure(context.getString(R.string.shader_import_read_failed))
        }

        val source = try {
            context.contentResolver.openInputStream(uri)?.use { it.readBytes().toString(Charsets.UTF_8) }
        } catch (e: IOException) {
            null
        } ?: return Result.Failure(context.getString(R.string.shader_import_read_failed))

        val validation = PostProcessing.validateShaderSource(source)
        if (!validation.valid) {
            return Result.Failure(
                context.getString(R.string.shader_import_failed, validation.errorMessage)
            )
        }

        val shaderDir = File(PostProcessing.getUserShaderDirectory())
        if (!shaderDir.exists() && !shaderDir.mkdirs()) {
            return Result.Failure(context.getString(R.string.shader_import_write_failed))
        }

        val target = File(shaderDir, "$shaderName.glsl")
        if (target.exists()) {
            return Result.Failure(
                context.getString(R.string.shader_import_name_conflict, shaderName)
            )
        }

        return try {
            target.writeText(source)
            Result.Success(shaderName, validation.gpuCompiled)
        } catch (e: IOException) {
            if (target.exists()) target.delete()
            Result.Failure(context.getString(R.string.shader_import_write_failed))
        }
    }

    private fun stripGlslExtension(fileName: String): String {
        val ext = FileBrowserHelper.getExtension(fileName, false)
        return if (ext != null && ext.equals("glsl", ignoreCase = true)) {
            fileName.substring(0, fileName.length - ext.length - 1)
        } else {
            fileName
        }
    }
}
```

- [ ] **Step 3: Verify it compiles**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin/Source/Android && ./gradlew :app:compileDebugKotlin 2>&1 | tail -20
```
Expected: BUILD SUCCESSFUL. If `ContentHandler.getDisplayName` has a different signature, adjust the call — verify with `grep -n "getDisplayName" Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/ContentHandler.java`.

- [ ] **Step 4: Commit**

```bash
git add Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/ShaderImportHelper.kt Source/Android/app/src/main/res/values/strings.xml
git commit -m "Android: add ShaderImportHelper to copy and validate imported shaders"
```

---

## Task 4: Wire up the SAF picker and settings UI

**Files:**
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/FileBrowserHelper.kt:47-54`
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsActivityResultLaunchers.kt`
- Modify: `Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsFragmentPresenter.kt:1621-1630`

**Interfaces:**
- Consumes: `ShaderImportHelper.importShader` (Task 3), existing `FilePicker` view (`model/view/FilePicker.kt`), `StringSetting.GFX_ENHANCE_POST_SHADER`, `SettingsAdapter.onFilePickerConfirmation` (existing).
- Produces: a `requestShaderFile` launcher on `SettingsActivityResultLaunchers`; a new `FilePicker` row in the Enhancements list.

- [ ] **Step 1: Add the shader extension set**

In `FileBrowserHelper.kt`, after `RAW_EXTENSION` (line 51), add:

```kotlin
    @JvmField
    val SHADER_EXTENSION: HashSet<String> = hashSetOf("glsl")
```

- [ ] **Step 2: Add the launcher**

In `SettingsActivityResultLaunchers.kt`, add imports:

```kotlin
import android.widget.Toast
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.utils.ShaderImportHelper
```

Add a new launcher property after `requestRawFile` (line 65). It reuses the extension check but, instead of storing the URI, imports the file and reports the result:

```kotlin
    val requestShaderFile = fragment.registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result: ActivityResult ->
        val intent = result.data
        val uri = intent?.data
        val context = fragment.requireContext()
        if (result.resultCode == Activity.RESULT_OK && uri != null) {
            val canonicalizedUri = context.contentResolver.canonicalize(uri) ?: uri
            FileBrowserHelper.runAfterExtensionCheck(
                context, canonicalizedUri, FileBrowserHelper.SHADER_EXTENSION
            ) {
                when (val importResult = ShaderImportHelper.importShader(context, canonicalizedUri)) {
                    is ShaderImportHelper.Result.Success -> {
                        val msgId = if (importResult.gpuCompiled) {
                            R.string.shader_import_success
                        } else {
                            R.string.shader_import_success_deferred
                        }
                        Toast.makeText(
                            context,
                            context.getString(msgId, importResult.shaderName),
                            Toast.LENGTH_LONG
                        ).show()
                        getAdapter()?.onShaderImported(importResult.shaderName)
                    }
                    is ShaderImportHelper.Result.Failure -> {
                        com.google.android.material.dialog.MaterialAlertDialogBuilder(context)
                            .setMessage(importResult.message)
                            .setPositiveButton(R.string.ok, null)
                            .show()
                    }
                }
            }
        }
    }
```

> NOTE for implementer: `R.string.ok` exists in this project (used elsewhere); if the lint complains, use the exact id already used for generic "OK" buttons — `grep -rn "R.string.ok\b" Source/Android/app/src/main/java | head`.

- [ ] **Step 3: Add `onShaderImported` to the adapter**

In `SettingsAdapter.kt`, after `onFilePickerConfirmation` (line 457), add a method that refreshes the shader dropdown so the new file appears immediately:

```kotlin
    fun onShaderImported(shaderName: String) {
        // Persist the newly imported shader as the active selection and force the
        // settings list to rebuild so the enumerated dropdown includes it.
        StringSetting.GFX_ENHANCE_POST_SHADER.setString(fragmentView.settings!!, shaderName)
        fragmentView.onSettingChanged()
        fragmentView.settings?.let { loadSettingsList() }
    }
```

> NOTE for implementer: confirm the exact rebuild call. Search `SettingsAdapter.kt` and `SettingsFragmentView` for the method that reloads the settings list (candidates: `loadSettingsList()`, `fragmentView.showSettingsList(...)`, or `notifyAllSettingsChanged()`). Use whichever the fragment already exposes to repopulate rows; the goal is that `SettingsFragmentPresenter` re-runs the Enhancements section so `PostProcessing.shaderList` is re-queried. Add the necessary import for `StringSetting` if not present.

- [ ] **Step 4: Add the FilePicker row to the Enhancements list**

In `SettingsFragmentPresenter.kt`, immediately after the existing `StringSingleChoiceSetting` block for the shader (after line 1630), add:

```kotlin
        sl.add(
            FilePicker(
                context,
                StringSetting.GFX_ENHANCE_POST_SHADER,
                R.string.post_processing_import_shader,
                R.string.post_processing_import_shader_description,
                fragmentView.activityResultLaunchers.requestShaderFile,
                null
            )
        )
```

Ensure `FilePicker` is imported in this file (it is already used at lines 591/708 — no new import needed).

- [ ] **Step 5: Build and verify compilation**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin/Source/Android && ./gradlew :app:assembleDebug 2>&1 | tail -30
```
Expected: BUILD SUCCESSFUL.

- [ ] **Step 6: Commit**

```bash
git add Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/FileBrowserHelper.kt Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsActivityResultLaunchers.kt Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsAdapter.kt Source/Android/app/src/main/java/org/dolphinemu/dolphinemu/features/settings/ui/SettingsFragmentPresenter.kt
git commit -m "Android: add SAF shader import picker to Enhancements settings"
```

---

## Task 5: End-to-end verification on device/emulator

**Files:** none (manual verification).

- [ ] **Step 1: Install the debug build**

Run:
```bash
cd /Users/ilya.lissoboi/work/dolphin/Source/Android && ./gradlew :app:installDebug 2>&1 | tail -10
```
Expected: `Installed on 1 device`.

- [ ] **Step 2: Prepare test shader files**

Push a valid shader and an invalid one to the device Downloads folder:
```bash
cat > /tmp/valid_test.glsl <<'EOF'
void main() { SetOutput(Sample()); }
EOF
cat > /tmp/broken_test.glsl <<'EOF'
this is not glsl at all
EOF
adb push /tmp/valid_test.glsl /sdcard/Download/valid_test.glsl
adb push /tmp/broken_test.glsl /sdcard/Download/broken_test.glsl
```

- [ ] **Step 3: Exercise the happy path (no game running)**

In the app: Settings → Graphics → Enhancements → "Import Custom Shader" → pick `valid_test.glsl`.
Expected: a Toast "Imported shader \"valid_test\". It will be fully compiled the next time a game runs." (deferred, since no backend is live), and `valid_test` now appears in the Post-Processing Shader dropdown and is selected. Confirm the file landed:
```bash
adb shell run-as org.dolphinemu.dolphinemu.debug find . -name 'valid_test.glsl'
```
Expected: a path ending in `/files/.../Shaders/valid_test.glsl` (or the app's external files Shaders dir).

- [ ] **Step 4: Exercise the failure path**

Import `broken_test.glsl`.
Expected: a `MaterialAlertDialog` with "Could not import shader: Shader source does not define a main() entry point." No new file in the Shaders dir; the dropdown is unchanged.

- [ ] **Step 5: Exercise the name-conflict path**

Import `valid_test.glsl` again.
Expected: dialog "A shader named \"valid_test\" already exists." No overwrite.

- [ ] **Step 6: Exercise the GPU-compile path (game running)**

Boot any game, then open Settings → Graphics → Enhancements → select the imported `valid_test` shader. Then re-import a second valid shader while the game runs.
Expected: Toast "Imported shader \"...\"" (non-deferred), and the selected shader visibly applies with no PanicAlert.

- [ ] **Step 7: Regression check**

Confirm the pre-existing bundled shaders still enumerate and apply (select a stock shader like "sepia"), and that anaglyph/passive lists still populate when stereo mode changes. Run the unit tests once more:
```bash
cd /Users/ilya.lissoboi/work/dolphin && ./build/Binaries/PostProcessingValidationTest
```
Expected: PASS.

- [ ] **Step 8: Commit any doc/string fixups discovered during verification** (only if changes were needed)

```bash
git add -A
git commit -m "Android: fixups from shader import end-to-end verification"
```

---

## Self-Review Notes

- **Spec coverage:** requesting permissions → Task 4 reuses SAF `ACTION_OPEN_DOCUMENT` + persistable-permission pattern, no new manifest perms (Global Constraints + Task 4). Validating shader data → Task 1 (native parse + conditional GPU compile) + Task 3 (read/name checks). Reporting errors to user → Task 3 returns typed failures; Task 4 shows dialog/Toast. Arbitrary filesystem location → SAF picker in Task 4; copy-into-Shaders-dir in Task 3 keeps the core loader unchanged.
- **Deferred-compile honesty:** when no backend is live, the user is told the compile is deferred (Task 1 `gpu_compiled=false` → deferred Toast string), matching the "Parse + GPU when available" decision.
- **`#include` gotcha:** an imported single file that `#include`s helpers not present in the Shaders/Sys dirs will pass the parse check but fail the eventual GPU compile. When a backend is live this is caught at import (Task 1); when deferred it surfaces at game start via the existing `PanicAlertFmt` path. Documented as a known limitation; a future enhancement could let the user import dependencies too.
- **Open implementer confirmations (flagged inline, not placeholders):** exact `GetHeader/GetFooter` bodies to copy (Task 1 Step 4); IDCache accessor convention + JNI ctor signature (Task 2 Step 4); the fragment's settings-list rebuild method name (Task 4 Step 3); generic OK-button string id (Task 4 Step 2).
