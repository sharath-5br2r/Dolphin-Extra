// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <map>

#include "VideoCommon/PostProcessing/SlangShader.h"

using namespace VideoCommon;

TEST(SlangShader, SplitsStagesAndSharesPrologue)
{
  const std::string text =
      "#pragma name FIRST_PASS\n"
      "#pragma format R16G16B16A16_SFLOAT\n"
      "layout(std140) uniform UBO { vec4 SourceSize; };\n"  // common prologue
      "#pragma stage vertex\n"
      "void main() { gl_Position = vec4(0); }\n"
      "#pragma stage fragment\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "void main() { FragColor = vec4(1); }\n";
  std::string error;
  const auto shader = ParseSlangShader(text, &error);
  ASSERT_TRUE(shader.has_value()) << error;
  EXPECT_EQ(shader->name, "FIRST_PASS");
  EXPECT_EQ(shader->format, "R16G16B16A16_SFLOAT");
  // Common prologue appears in both stages.
  EXPECT_NE(shader->vertex_source.find("uniform UBO"), std::string::npos);
  EXPECT_NE(shader->fragment_source.find("uniform UBO"), std::string::npos);
  // Stage-specific bodies land in the right stage only.
  EXPECT_NE(shader->vertex_source.find("gl_Position"), std::string::npos);
  EXPECT_EQ(shader->vertex_source.find("FragColor"), std::string::npos);
  EXPECT_NE(shader->fragment_source.find("FragColor"), std::string::npos);
}

TEST(SlangShader, ParsesParameters)
{
  const std::string text =
      "#pragma parameter BRIGHTNESS \"Brightness\" 1.0 0.0 2.0 0.05\n"
      "#pragma stage vertex\n"
      "void main() {}\n"
      "#pragma stage fragment\n"
      "void main() {}\n";
  std::string error;
  const auto shader = ParseSlangShader(text, &error);
  ASSERT_TRUE(shader.has_value()) << error;
  ASSERT_EQ(shader->parameters.size(), 1u);
  EXPECT_EQ(shader->parameters[0].id, "BRIGHTNESS");
  EXPECT_EQ(shader->parameters[0].label, "Brightness");
  EXPECT_FLOAT_EQ(shader->parameters[0].default_value, 1.0f);
  EXPECT_FLOAT_EQ(shader->parameters[0].max_value, 2.0f);
}

TEST(SlangShader, RejectsMissingStages)
{
  std::string error;
  const auto shader = ParseSlangShader("void main() {}\n", &error);
  EXPECT_FALSE(shader.has_value());
  EXPECT_FALSE(error.empty());
}

TEST(SlangShader, ExpandsLocalIncludes)
{
  // crt-royale ships passes whose #pragma stage bodies live in an #include'd header.
  const std::string root =
      "#version 450\n"
      "#include \"body.h\"\n";
  const auto reader = [](const std::string& path, std::string* out) -> bool {
    if (path == "/shaders/body.h")
    {
      *out =
          "#pragma stage vertex\n"
          "void main() { gl_Position = vec4(0); }\n"
          "#pragma stage fragment\n"
          "layout(location = 0) out vec4 FragColor;\n"
          "void main() { FragColor = vec4(1); }\n";
      return true;
    }
    return false;
  };

  const std::string expanded = ExpandSlangIncludes(root, "/shaders", reader);
  EXPECT_NE(expanded.find("#pragma stage vertex"), std::string::npos);
  EXPECT_EQ(expanded.find("#include"), std::string::npos);  // include consumed

  std::string error;
  const auto shader = ParseSlangShader(expanded, &error);
  ASSERT_TRUE(shader.has_value()) << error;
  EXPECT_NE(shader->vertex_source.find("gl_Position"), std::string::npos);
  EXPECT_NE(shader->fragment_source.find("FragColor"), std::string::npos);
}

TEST(SlangShader, StripsVersionDirectiveFromStages)
{
  // crt-royale .slang files begin with "#version 450"; Dolphin's backend prepends its own
  // #version header, and GLSL requires #version to be the first token, so the shader's own
  // #version must not survive into the stage sources.
  const std::string text =
      "#version 450\n"
      "layout(std140) uniform UBO { vec4 SourceSize; };\n"
      "#pragma stage vertex\n"
      "void main() { gl_Position = vec4(0); }\n"
      "#pragma stage fragment\n"
      "layout(location = 0) out vec4 FragColor;\n"
      "void main() { FragColor = vec4(1); }\n";
  std::string error;
  const auto shader = ParseSlangShader(text, &error);
  ASSERT_TRUE(shader.has_value()) << error;
  EXPECT_EQ(shader->vertex_source.find("#version"), std::string::npos);
  EXPECT_EQ(shader->fragment_source.find("#version"), std::string::npos);
}

TEST(SlangShader, ExpandsNestedIncludesRelativeToIncluder)
{
  const std::string root = "#include \"a/one.h\"\n";
  const auto reader = [](const std::string& path, std::string* out) -> bool {
    if (path == "/root/a/one.h")
    {
      *out = "#include \"two.h\"\n";  // relative to one.h's own dir
      return true;
    }
    if (path == "/root/a/two.h")
    {
      *out = "RESOLVED_NESTED\n";
      return true;
    }
    return false;
  };
  const std::string expanded = ExpandSlangIncludes(root, "/root", reader);
  EXPECT_NE(expanded.find("RESOLVED_NESTED"), std::string::npos);
}

TEST(SlangShader, ParameterOverrideBeatsDefault)
{
  const std::vector<SlangParameter> params = {
      {/*id=*/"masksize", "Mask Size", /*default=*/1.0f, 0.0f, 4.0f, 1.0f},
      {/*id=*/"gamma", "Gamma", /*default=*/2.4f, 0.0f, 5.0f, 0.1f},
  };
  const std::map<std::string, float> overrides = {{"masksize", 3.0f}};

  float out = -1.0f;
  EXPECT_TRUE(ResolveShaderParameter(overrides, params, "masksize", &out));
  EXPECT_FLOAT_EQ(out, 3.0f);  // override wins

  out = -1.0f;
  EXPECT_TRUE(ResolveShaderParameter(overrides, params, "gamma", &out));
  EXPECT_FLOAT_EQ(out, 2.4f);  // falls back to #pragma default

  EXPECT_FALSE(ResolveShaderParameter(overrides, params, "unknown", &out));
}

TEST(SlangShader, ExpandsIncludesFromWindowsDirectory)
{
  const std::map<std::string, std::string> files = {
      {"E:/shaders/inc/common.h", "float common_value = 1.0;\n"},
  };
  const auto reader = [&files](const std::string& path, std::string* out) {
    const auto it = files.find(path);
    if (it == files.end())
      return false;
    *out = it->second;
    return true;
  };
  const std::string expanded =
      ExpandSlangIncludes("#include \"inc/common.h\"\n", "E:\\shaders", reader);
  EXPECT_NE(expanded.find("common_value"), std::string::npos);
}
