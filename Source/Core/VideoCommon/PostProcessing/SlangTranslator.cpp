// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing/SlangTranslator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <map>
#include <sstream>
#include <string_view>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/ShaderCompileUtils.h"

namespace VideoCommon
{
namespace
{
std::string_view Trim(std::string_view s)
{
  const auto first = s.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos)
    return {};
  const auto last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

// Extracts the sampler name from a line declaring `... uniform sampler2D[Array] <Name>;`.
// Returns empty if the line does not declare a uniform sampler (e.g. a `sampler2D` function
// parameter or a helper typedef is ignored -- only uniform declarations count).
// The array spelling is tested first: "sampler2D" is a prefix of "sampler2DArray", so the other
// order would report the name of an already-array declaration as "Array".
std::string ExtractSamplerName(std::string_view line)
{
  if (line.find("uniform") == std::string_view::npos)
    return {};
  constexpr std::string_view ARRAY_KW = "sampler2DArray";
  constexpr std::string_view PLAIN_KW = "sampler2D";
  auto kw = line.find(ARRAY_KW);
  size_t kw_size = ARRAY_KW.size();
  if (kw == std::string_view::npos)
  {
    kw = line.find(PLAIN_KW);
    kw_size = PLAIN_KW.size();
  }
  if (kw == std::string_view::npos)
    return {};
  std::string_view rest = Trim(line.substr(kw + kw_size));
  // Name runs until ';' or whitespace.
  size_t end = 0;
  while (end < rest.size() && (std::isalnum(static_cast<unsigned char>(rest[end])) != 0 ||
                               rest[end] == '_'))
  {
    ++end;
  }
  return std::string(rest.substr(0, end));
}

// If the line declares its sampler with an explicit `layout(binding = N)`, returns N.
// Returns -1 when no explicit binding is present. crt-royale reuses the same binding number
// in mutually-exclusive #ifdef/#else branches, so distinct binding numbers -- not distinct
// names -- give the true sampler count (the translator has no preprocessor).
int ExtractSamplerBinding(std::string_view line)
{
  const auto pos = line.find("binding");
  if (pos == std::string_view::npos)
    return -1;
  const auto eq = line.find('=', pos);
  if (eq == std::string_view::npos)
    return -1;
  std::string_view rest = Trim(line.substr(eq + 1));
  int value = 0;
  bool any = false;
  for (const char c : rest)
  {
    if (c < '0' || c > '9')
      break;
    value = value * 10 + (c - '0');
    any = true;
  }
  return any ? value : -1;
}

static_assert(SLANG_INPUT_TEXTURE_TYPE == AbstractTextureType::Texture_2DArray,
              "the layer-0 shim helpers below are written for the array sampler type");

// Slang shaders sample with 2-component coordinates, but Dolphin binds 2D-array textures, so the
// declarations are sampler2DArray (see SLANG_INPUT_TEXTURE_TYPE). Rewriting call-site argument
// lists is not an option: the real libretro pack has over 8000 of them, most behind macros, and
// crt-royale hands samplers to user functions (`vec4 tex2D_linearize(sampler2D tex, vec2 coords)`)
// whose bodies name no sampler at all. So the built-ins are shimmed instead.
//
// Every shim is a *renamed helper*: the built-in's name becomes dolphin_<builtin> wherever it
// is used as a function name (see IsFunctionNameUse), and the block below defines that helper to
// supply the layer-0 third coordinate and forward to the real built-in. Renaming rather than
// overloading the built-in is what makes the GLES path work at all: ESSL 3.00 and up forbid
// overloading a built-in outright -- glslang's own comment is "ES 300 does not allow redefining or
// overloading of built-in functions" (ParseHelper.cpp:1173-1174, enforced by the
// requireProfile(loc, ~EEsProfile, ...) just after it) -- while overloading a *user* function is
// legal on every target, and dolphin_texture is a user function. That path is reachable:
// OGLConfig.cpp selects GlslEs300/310/320, and arrays.xml:204 offers OGL as an Android backend.
//
// The helpers come in two shapes, because GLSL constrains their arguments differently:
//
// 1. Functions, for built-ins whose arguments are ordinary values. Two helpers may share one name
//    (dolphin_texture has a bias form, dolphin_textureGather a 2- and a 3-argument form); they
//    overload each other rather than a built-in, which is why ES accepts them.
// 2. Function-like macros, for the *Offset built-ins. Their `offset` must be a compile-time
//    constant expression and a function parameter never is -- glslang rejects the function form
//    outright ("'texel offset' : argument must be compile-time constant"). A macro forwards the
//    token verbatim, so a literal stays literal. Renaming also retires the old self-named macros,
//    which worked only because the preprocessor does not re-expand a macro inside its own
//    expansion.
//
// textureGather's `comp` must likewise be constant, but the pack calls it with both 2 and 3
// arguments and a macro cannot be overloaded on arity, so the 3-argument helper is a function that
// dispatches to four constant `comp` values.
//
// A sampling built-in not covered here fails to compile -- loudly, unlike the silent black frame a
// dimension mismatch produces. Known gaps, none of which the libretro pack uses: the bias form of
// textureOffset, textureGatherOffset, and textureProj (which has no array form in GLSL at all).
// Renaming adds one gap overloading did not have: a call on a sampler that really is not an array
// (usampler2D, sampler3D) no longer falls through to the built-in. No preset reaches it, because
// the translator emits the sampler declarations itself and always as SLANG_INPUT_TEXTURE_TYPE --
// test/decode-format.slang's `usampler2D Source` is already sampler2DArray by this point -- and if
// one ever does, that too is a loud compile error.
constexpr std::string_view SHIM_PREFIX = "dolphin_";

std::string ShimHelperName(std::string_view builtin)
{
  return std::string(SHIM_PREFIX) + std::string(builtin);
}

struct SamplerShim
{
  // Call sites of this built-in are renamed to SHIM_PREFIX + builtin. Entries may share a built-in;
  // the rename runs once per built-in, and each entry's helper is emitted on its own.
  std::string_view builtin;
  std::string_view glsl;  // prepended when the stage names the helper
  bool fragment_only = false;
  // When set, glsl is wrapped in `#if <version_guard>` / `#endif`, so a stage that carries the
  // helper without calling it compiles on a target the helper's own body could not.
  std::string_view version_guard = {};
};

// `textureGather` is core in GLSL 400 and in GLSL ES 310 -- ES 3.00 has no form of it at all -- and
// because a shim *calls* the built-in it wraps, declaring one unconditionally made every translated
// pass depend on GLSL 400. OGL's GetGLSLVersionString() can emit 130, 140, 150, 330, 300 es or
// 310 es and AbstractGfx::CreatePostProcessor() applies no version gate, so that broke every pass
// on a GL 3.3-class context, gathering or not. Measured against glslang's GLSL front end: the
// unguarded block at `#version 330` and at `#version 300 es` gives "'textureGather(...)' : not
// supported for this version or the enabled extensions"; at `310 es`, `320 es` and `410` it is
// clean, for the 2- and the 3-argument form alike.
//
// Two mechanisms keep a stage from paying for that, because one of them cannot be made sufficient:
//
// 1. The gate (below, where the shims are emitted): a shim is emitted only when the renamed stage
//    source names its helper as a whole word, in a copy with comments stripped. That is as precise
//    as it can get, because there is no preprocessor here: `textureGather` also survives inside a
//    never-taken `#if` branch and inside a macro body nothing expands -- the nnedi3
//    `-predict-h-rgb` family is exactly that, `#define NNEDI3_USE_GATHER 0` with the text left in
//    NNEDI3_DEF_GATHER -- and the gate then believes the stage gathers. So the gate is a size
//    optimization, not a correctness mechanism. It was first written as the latter, and 7 stages
//    across 5 presets of the libretro pack failed at `#version 300 es` for precisely that reason.
//    Measured over that pack (2987 presets, 23515 stages): the helper was emitted into 177 stages
//    before comments were stripped and 72 after -- the 105 that went away are all fsr-pass0 and
//    fsr-pass1, whose only mention of the built-in is the commented-out FsrEasu*H bodies in
//    ffx_fsr1.h -- and 7 of the surviving 72 are the nnedi3 macro bodies, which no textual gate can
//    rule out. Those 7 are the ones the guard rescues.
// 2. The guard (version_guard): the one helper whose availability depends on the version says so
//    itself, so an over-firing gate costs bytes and nothing else. A stage that carries the helper
//    without calling it compiles anywhere; a stage that really calls it where the built-in does not
//    exist fails loudly on an undeclared `dolphin_textureGather`, which is the same class of
//    failure, at the same point, as calling the built-in directly would have been.
//
// The guard tests versions only. `defined(GL_ARB_gpu_shader5)` deliberately does not appear in it:
// an extension macro is defined when the compiler *knows* the extension, not when the shader has
// enabled it, and a shader's initial state is `#extension all : disable`. Verified against glslang,
// which predefines GL_ARB_gpu_shader5 and GL_ARB_texture_gather at `#version 330` -- the helper
// body still fails to compile there unless the source also carries
// `#extension GL_ARB_gpu_shader5 : enable`. Admitting the helper on the macro alone would re-break
// every non-gathering pass on any driver that advertises the extension the shader has not enabled.
// What that costs: on a GL 3.3 context whose header did enable ARB_gpu_shader5 (ProgramShaderCache
// does, when v < Glsl400 && bSupportsGSInstancing) a genuinely gathering pass is refused although
// the driver could have run it. Closing that gap means the header telling the shader what it
// enabled -- a macro of Dolphin's own beside the `#extension` line -- which is not the translator's
// to decide, and which no preset in the pack needs today.
//
// The other built-ins need no guard: `texture`, `textureLod`, `textureGrad`, `texelFetch` and
// `textureSize` are core in their array forms since GLSL 130 / ES 300, and the three *Offset shims
// are macros, which cost nothing until expanded. Verified by parsing the whole block with every
// helper called at 300 es, 310 es, 320 es, 330 and 410. A future entry that does have a floor must
// carry its own guard -- the gate will not save it.
constexpr std::string_view TEXTURE_GATHER_GUARD =
    "__VERSION__ >= 400 || (defined(GL_ES) && __VERSION__ >= 310)";

constexpr SamplerShim SAMPLER_SHIMS[] = {
    {"texture",
     "vec4 dolphin_texture(sampler2DArray s, vec2 c) { return texture(s, vec3(c, 0.0)); }\n"},
    // The optional `bias` argument of the implicit-LOD built-ins is accepted only in fragment
    // shaders, so this helper must not be declared in the vertex stage.
    {"texture",
     "vec4 dolphin_texture(sampler2DArray s, vec2 c, float bias)\n"
     "{\n"
     "  return texture(s, vec3(c, 0.0), bias);\n"
     "}\n",
     /*fragment_only=*/true},
    {"textureLod",
     "vec4 dolphin_textureLod(sampler2DArray s, vec2 c, float l)\n"
     "{\n"
     "  return textureLod(s, vec3(c, 0.0), l);\n"
     "}\n"},
    {"textureGrad",
     "vec4 dolphin_textureGrad(sampler2DArray s, vec2 c, vec2 dx, vec2 dy)\n"
     "{\n"
     "  return textureGrad(s, vec3(c, 0.0), dx, dy);\n"
     "}\n"},
    {"texelFetch",
     "vec4 dolphin_texelFetch(sampler2DArray s, ivec2 c, int l)\n"
     "{\n"
     "  return texelFetch(s, ivec3(c, 0), l);\n"
     "}\n"},
    {"textureGather",
     "vec4 dolphin_textureGather(sampler2DArray s, vec2 c)\n"
     "{\n"
     "  return textureGather(s, vec3(c, 0.0));\n"
     "}\n"
     "vec4 dolphin_textureGather(sampler2DArray s, vec2 c, int comp)\n"
     "{\n"
     "  vec3 p = vec3(c, 0.0);\n"
     "  if (comp == 1) return textureGather(s, p, 1);\n"
     "  if (comp == 2) return textureGather(s, p, 2);\n"
     "  if (comp == 3) return textureGather(s, p, 3);\n"
     "  return textureGather(s, p, 0);\n"
     "}\n",
     /*fragment_only=*/false, TEXTURE_GATHER_GUARD},
    // textureSize is the built-in that could never have been an overload -- the array form differs
    // from the 2D form only in return type (ivec3 vs ivec2), and GLSL forbids overloading on return
    // type. It needed the rename first; now every entry is written the same way.
    {"textureSize",
     "ivec2 dolphin_textureSize(sampler2DArray s, int l) { return textureSize(s, l).xy; }\n"},
    {"textureOffset",
     "#define dolphin_textureOffset(s, c, o) textureOffset(s, vec3((c), 0.0), o)\n"},
    {"textureLodOffset",
     "#define dolphin_textureLodOffset(s, c, l, o) textureLodOffset(s, vec3((c), 0.0), l, o)\n"},
    {"texelFetchOffset",
     "#define dolphin_texelFetchOffset(s, c, l, o) texelFetchOffset(s, ivec3((c), 0), l, o)\n"},
};

// The one thing about the table above that the compiler cannot check: an entry whose glsl does
// not define the helper its built-in is renamed to would drop every one of that built-in's call
// sites into a function that does not exist.
constexpr bool EveryShimDefinesItsHelper()
{
  for (const SamplerShim& shim : SAMPLER_SHIMS)
  {
    bool defined = false;
    for (size_t pos = shim.glsl.find(shim.builtin); pos != std::string_view::npos;
         pos = shim.glsl.find(shim.builtin, pos + 1))
    {
      if (pos >= SHIM_PREFIX.size() &&
          shim.glsl.substr(pos - SHIM_PREFIX.size(), SHIM_PREFIX.size()) == SHIM_PREFIX)
      {
        defined = true;
      }
    }
    if (!defined)
      return false;
  }
  return true;
}
static_assert(EveryShimDefinesItsHelper(),
              "every shim must define the dolphin_-prefixed helper that its built-in's call sites "
              "are renamed to");

bool StartsWith(std::string_view s, std::string_view prefix)
{
  return s.substr(0, prefix.size()) == prefix;
}

bool IsWordChar(char c)
{
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// A copy of `text` with every `//` and `/* */` comment replaced by one space, for searches that are
// meant to answer "does the compiler see this?" rather than "does the file contain it?". Each
// comment becomes a space rather than nothing so that deleting it cannot fuse the identifiers on
// either side into a third one. GLSL has no string literals, so nothing here can be quoted.
//
// One knowingly unhandled corner: C99 splices a `\`-continued line before it removes comments, so a
// `//` comment ending in a backslash swallows the next line too. Five shaders in the libretro pack
// do it (grade.slang, grade-no-LUT.slang, grade_orig.slang, pre-shaders-afterglow-grade.slang,
// dave_hoskins-ray-q-bert.slang), so this is a corner the corpus actually reaches -- but treating
// that next line as code is the safe direction: it can only make a search say yes where the
// compiler says no, never the reverse, and a false yes costs an unused helper the version guard
// already makes harmless. The dangerous direction is the other one, and it is closed: the only way
// this function can drop real code is an unterminated `/*`, which is a hard glslang error before
// any of this matters.
std::string StripComments(std::string_view text)
{
  std::string out;
  out.reserve(text.size());
  size_t pos = 0;
  while (pos < text.size())
  {
    const bool two_left = pos + 1 < text.size();
    if (text[pos] == '/' && two_left && text[pos + 1] == '/')
    {
      out += ' ';
      const auto end = text.find('\n', pos + 2);
      if (end == std::string_view::npos)
        break;
      pos = end;  // the newline is not part of the comment, and it terminates a directive
    }
    else if (text[pos] == '/' && two_left && text[pos + 1] == '*')
    {
      out += ' ';
      const auto end = text.find("*/", pos + 2);
      if (end == std::string_view::npos)
        break;
      pos = end + 2;
    }
    else
    {
      out += text[pos];
      ++pos;
    }
  }
  return out;
}

// True when `word` occurs in `text` as a whole identifier rather than inside a longer one, so
// `texture` does not match `textureLod` and `texelFetch` does not match `texelFetchOffset`.
bool ContainsWord(std::string_view text, std::string_view word)
{
  for (size_t pos = text.find(word); pos != std::string_view::npos; pos = text.find(word, pos + 1))
  {
    const size_t after = pos + word.size();
    if ((pos == 0 || !IsWordChar(text[pos - 1])) &&
        (after >= text.size() || !IsWordChar(text[after])))
    {
      return true;
    }
  }
  return false;
}

// True when the whole-word occurrence spanning [begin, after) is used as the name of a function:
// either it is applied to an argument list right there, or it is the replacement list of an
// object-like macro, which is only ever called (`#define COMPAT_TEXTURE texture`, from
// crt/shaders/hyllian/crt-hyllian-fast.slang -- the only such alias in the pack, and its call sites
// all read COMPAT_TEXTURE(...)).
//
// Occurrences that are neither are deliberately left alone, because they name an object rather than
// a function: crt-royale's bloom-functions.h declares
// `tex2DblurNfast(const sampler2D texture, ...)` and passes that parameter on by name ten times, so
// a blanket rename would hide the helper behind a parameter of the same name inside those
// functions, and collide with it at the declaration.
bool IsFunctionNameUse(std::string_view text, size_t begin, size_t after)
{
  size_t pos = after;
  while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
    ++pos;
  if (pos < text.size() && text[pos] == '(')
    return true;

  // The rest of the line has to be empty for this to be a macro's replacement list; a `\`
  // continuation or a trailing `//` comment still counts as empty.
  while (pos < text.size() && text[pos] != '\n')
  {
    if (text[pos] == '\\' || (text[pos] == '/' && pos + 1 < text.size() && text[pos + 1] == '/'))
      break;
    if (text[pos] != ' ' && text[pos] != '\t' && text[pos] != '\r')
      return false;
    ++pos;
  }
  const size_t line_start = text.rfind('\n', begin);
  const std::string_view line =
      text.substr(line_start == std::string_view::npos ? 0 : line_start + 1);
  return StartsWith(Trim(line), "#define");
}

// Replaces every whole-word occurrence of `from` with `to` in `text`. When `function_names_only`,
// only the occurrences IsFunctionNameUse accepts are replaced.
std::string ReplaceWord(const std::string& text, const std::string& from, const std::string& to,
                        bool function_names_only = false)
{
  std::string out;
  out.reserve(text.size());
  const auto is_word_char = [](char c) { return IsWordChar(c); };
  size_t pos = 0;
  while (pos < text.size())
  {
    const auto found = text.find(from, pos);
    if (found == std::string::npos)
    {
      out.append(text, pos, std::string::npos);
      break;
    }
    const bool left_ok = found == 0 || !is_word_char(text[found - 1]);
    const size_t after = found + from.size();
    const bool right_ok = after >= text.size() || !is_word_char(text[after]);
    out.append(text, pos, found - pos);
    if (left_ok && right_ok && (!function_names_only || IsFunctionNameUse(text, found, after)))
    {
      out += to;
    }
    else
    {
      out += from;
    }
    pos = after;
  }
  return out;
}

// Strips a trailing `// ...` line comment (RetroArch shaders annotate varyings with comments
// that can contain the words "in"/"out", which must not be mistaken for storage qualifiers).
std::string_view StripLineComment(std::string_view s)
{
  const auto pos = s.find("//");
  return pos == std::string_view::npos ? s : Trim(s.substr(0, pos));
}

// For a `layout(...) <qualifier> <type> <name>;` declaration, returns the storage qualifier
// token immediately after the closing ')': "in", "out", or "" if none.
std::string_view StorageQualifierAfterLayout(std::string_view decl)
{
  const auto close = decl.find(')');
  if (close == std::string_view::npos)
    return {};
  std::string_view rest = Trim(decl.substr(close + 1));
  const auto space = rest.find_first_of(" \t");
  const std::string_view token = space == std::string_view::npos ? rest : rest.substr(0, space);
  if (token == "in" || token == "out")
    return token;
  return {};
}

// Classifies the type keyword at the start of a member declaration for std140 packing.
UboMemberType ClassifyMemberType(std::string_view decl)
{
  decl = Trim(decl);
  const auto space = decl.find_first_of(" \t");
  const std::string_view kw = space == std::string_view::npos ? decl : decl.substr(0, space);
  if (kw == "mat4" || kw == "float4x4")
    return UboMemberType::Mat4;
  if (kw == "vec4" || kw == "float4" || kw == "ivec4" || kw == "uvec4")
    return UboMemberType::Vec4;
  if (kw == "vec3" || kw == "float3" || kw == "ivec3" || kw == "uvec3")
    return UboMemberType::Vec3;
  if (kw == "vec2" || kw == "float2" || kw == "ivec2" || kw == "uvec2")
    return UboMemberType::Vec2;
  if (kw == "float" || kw == "int" || kw == "uint" || kw == "bool")
    return UboMemberType::Float;
  return UboMemberType::Unknown;
}

// Extracts every RetroArch uniform block from a stage source, appending merged member lines to
// `out_members` (deduplicated by member name -- the two blocks never share names in practice,
// but a member repeated across stages must not be emitted twice) and recording each block's
// instance name in `out_instances`. Returns the source with those block declarations removed.
std::string ExtractUniformBlocks(const std::string& source, std::vector<std::string>* out_members,
                                 std::vector<std::string>* out_instances,
                                 std::vector<std::string>* out_member_names,
                                 std::vector<UboMember>* out_typed_members)
{
  std::string out;
  std::istringstream in(source);
  std::string line;
  while (std::getline(in, line))
  {
    std::string_view view = line;
    if (!view.empty() && view.back() == '\r')
      view.remove_suffix(1);
    const std::string_view trimmed = Trim(view);

    // A block opener declares `uniform <Name>` and is a layout(push_constant|std140) line.
    const bool is_block_opener =
        trimmed.find("uniform ") != std::string_view::npos &&
        (trimmed.find("push_constant") != std::string_view::npos ||
         trimmed.find("std140") != std::string_view::npos) &&
        trimmed.find("sampler") == std::string_view::npos;

    if (!is_block_opener)
    {
      out += std::string(view);
      out += '\n';
      continue;
    }

    // Consume until the closing `} instance;`. The opener may or may not include the '{'.
    std::string block(trimmed);
    while (block.find('}') == std::string::npos && std::getline(in, line))
    {
      std::string_view v = line;
      if (!v.empty() && v.back() == '\r')
        v.remove_suffix(1);
      block += '\n';
      block += std::string(v);
    }

    // Instance name: text between '}' and ';'.
    const auto brace_close = block.find('}');
    const auto semi = block.find(';', brace_close);
    if (brace_close != std::string::npos && semi != std::string::npos)
    {
      const std::string instance = std::string(Trim(
          std::string_view(block).substr(brace_close + 1, semi - brace_close - 1)));
      if (!instance.empty())
        out_instances->push_back(instance);
    }

    // Members: everything between '{' and '}'.
    const auto brace_open = block.find('{');
    if (brace_open != std::string::npos && brace_close != std::string::npos &&
        brace_close > brace_open)
    {
      const std::string body = block.substr(brace_open + 1, brace_close - brace_open - 1);
      std::istringstream member_in(body);
      std::string member_line;
      while (std::getline(member_in, member_line))
      {
        const std::string_view m = Trim(member_line);
        if (m.empty())
          continue;
        // Derive the member name (last identifier before the ';').
        const auto sc = m.find(';');
        std::string_view decl = sc == std::string_view::npos ? m : m.substr(0, sc);
        decl = Trim(decl);
        size_t name_start = decl.size();
        while (name_start > 0 && (std::isalnum(static_cast<unsigned char>(decl[name_start - 1])) ||
                                  decl[name_start - 1] == '_'))
        {
          --name_start;
        }
        const std::string member_name(decl.substr(name_start));
        if (std::find(out_member_names->begin(), out_member_names->end(), member_name) !=
            out_member_names->end())
        {
          continue;  // already emitted (e.g. block appears in both stages)
        }
        out_member_names->push_back(member_name);
        out_members->emplace_back(m);
        out_typed_members->push_back({member_name, ClassifyMemberType(decl)});
      }
    }
    // Block declaration removed from output.
  }
  return out;
}
}  // namespace

TranslatedPass TranslateSlangPass(const SlangShaderSource& shader,
                                  const std::vector<std::string>& known_aliases,
                                  const std::vector<std::string>& lut_names, bool flip_clip_y)
{
  TranslatedPass result;

  // 1. Discover sampler declarations across both stages and assign each a Dolphin binding
  //    "slot". "Source" is always slot 0. Samplers declared with an explicit slang
  //    layout(binding = N) are deduplicated by N: crt-royale reuses the same N across
  //    mutually-exclusive #ifdef/#else branches, so distinct binding numbers -- not distinct
  //    textual names -- give the true sampler count. Samplers with no explicit binding are
  //    keyed by name.
  std::vector<std::string> sampler_names = {"Source"};
  std::map<int, size_t> slang_binding_to_slot;  // explicit slang binding -> slot index
  std::map<std::string, size_t> name_to_slot;   // sampler name -> slot index
  name_to_slot["Source"] = 0;

  const auto scan_stage = [&](const std::string& source) {
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line))
    {
      const std::string name = ExtractSamplerName(line);
      if (name.empty())
        continue;
      if (name_to_slot.count(name) != 0)
        continue;

      const int binding = ExtractSamplerBinding(line);
      if (binding >= 0)
      {
        const auto existing = slang_binding_to_slot.find(binding);
        if (existing != slang_binding_to_slot.end())
        {
          // A different name reusing an already-seen slang binding (dead #ifdef branch):
          // map this name onto the same slot; do not allocate a new one.
          name_to_slot[name] = existing->second;
          continue;
        }
      }

      const size_t slot = sampler_names.size();
      sampler_names.push_back(name);
      name_to_slot[name] = slot;
      if (binding >= 0)
        slang_binding_to_slot[binding] = slot;
    }
  };
  scan_stage(shader.vertex_source);
  scan_stage(shader.fragment_source);

  if (sampler_names.size() > SLANG_MAX_SAMPLERS)
  {
    result.ok = false;
    result.error = "pass references " + std::to_string(sampler_names.size()) +
                   " samplers; max is " + std::to_string(SLANG_MAX_SAMPLERS);
    return result;
  }

  // Assign binding indices.
  const auto binding_of = [&name_to_slot](const std::string& name) -> int {
    const auto it = name_to_slot.find(name);
    return it == name_to_slot.end() ? 0 : static_cast<int>(it->second);
  };

  // 2. Extract the RetroArch uniform blocks (push_constant "Push {} params" + "std140 UBO {}
  //    global") from both stages, merging their members into one Dolphin PSBlock. Each stage's
  //    source has the block declarations removed.
  std::vector<std::string> ubo_members;
  std::vector<std::string> ubo_member_names;
  std::vector<std::string> instances;
  std::vector<UboMember> typed_members;
  std::string vs_noblocks = ExtractUniformBlocks(shader.vertex_source, &ubo_members, &instances,
                                                 &ubo_member_names, &typed_members);
  std::string fs_noblocks = ExtractUniformBlocks(shader.fragment_source, &ubo_members, &instances,
                                                 &ubo_member_names, &typed_members);

  // Build the merged block declaration with a single instance name "params". Any other instance
  // name (e.g. "global") is aliased to it, so params.X, global.X, and macro-expanded IN.X all
  // resolve to the same block.
  std::string ubo_decl = "UBO_BINDING(std140, 1) uniform PSBlock {\n";
  for (const std::string& member : ubo_members)
  {
    ubo_decl += "  ";
    ubo_decl += member;
    ubo_decl += '\n';
  }
  ubo_decl += "} params;\n";
  for (const std::string& instance : instances)
  {
    if (instance != "params")
      ubo_decl += "#define " + instance + " params\n";
  }

  // Neutralize the HLSL-compat macros the shader redefines. Dolphin's backend header defines
  // `frac`/`lerp` as OBJECT-like (`#define lerp mix`) while RetroArch's compat_macros.inc
  // defines them FUNCTION-like (`#define lerp(a,b,c) mix(a,b,c)`); glslang errors on that
  // mismatch. #undef-ing them before the shader's own #define makes the redefinition harmless.
  // We only undef the names Dolphin's header actually defines AND that collide in kind -- the
  // type aliases (float2/3/4, uint2..) are object-like in both, so identical redefinition is
  // allowed and they must stay defined (our own emitted `out float4 ocol0` depends on them).
  static const char* const kCompatMacros[] = {"frac", "lerp"};
  std::string undefs;
  for (const char* macro : kCompatMacros)
    undefs += std::string("#undef ") + macro + "\n";

  // 3. Line-level rewrite of a stage. is_vertex controls attribute/varying handling.
  const auto rewrite_stage = [&](const std::string& source, bool is_vertex) {
    std::string out;
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line))
    {
      std::string_view view = line;
      if (!view.empty() && view.back() == '\r')
        view.remove_suffix(1);
      const std::string_view trimmed = Trim(view);

      // Sampler declaration -> SAMPLER_BINDING(<assigned>).
      const std::string sampler = ExtractSamplerName(trimmed);
      if (!sampler.empty())
      {
        const int binding = binding_of(sampler);
        out += "SAMPLER_BINDING(" + std::to_string(binding) + ") uniform " +
               std::string(SlangSamplerGlslType(SLANG_INPUT_TEXTURE_TYPE)) + " " + sampler + ";\n";
        continue;
      }

      // Fragment output -> FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;
      if (trimmed.find("out vec4 FragColor") != std::string_view::npos &&
          trimmed.find("layout(location") != std::string_view::npos)
      {
        out += "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n";
        continue;
      }

      const std::string_view decl = StripLineComment(trimmed);
      const bool has_location = StartsWith(decl, "layout(location");
      const std::string_view qualifier = has_location ? StorageQualifierAfterLayout(decl)
                                                       : std::string_view{};

      // Vertex attribute inputs (`layout(location=N) in ...`) are dropped: Dolphin utility draws
      // are attribute-less; the values the shader reads (Position/TexCoord) are synthesized
      // inside main() below.
      if (is_vertex && qualifier == "in")
        continue;

      // Varyings: layout(location = N) in/out ... -> VARYING_LOCATION(N) in/out ...
      if (has_location && !qualifier.empty())
      {
        const std::string t(decl);
        const auto eq = t.find('=');
        const auto close = t.find(')');
        if (eq != std::string::npos && close != std::string::npos && close > eq)
        {
          const std::string n =
              std::string(Trim(std::string_view(t).substr(eq + 1, close - eq - 1)));
          out += "VARYING_LOCATION(" + n + ")" + t.substr(close + 1) + "\n";
          continue;
        }
      }

      out += std::string(view);
      out += '\n';
    }

    if (is_vertex)
    {
      // Synthesize the vertex attributes the shader expects from the fullscreen-triangle
      // vertex id, so `MVP * Position` (MVP is identity) yields fullscreen clip coords and
      // TexCoord spans [0,1]. On Vulkan the clip-space Y is inverted (matching Dolphin's
      // fixed post-process vertex shader and pass-through pipeline); since MVP is identity we
      // bake the flip into Position.y.
      const std::string flip = flip_clip_y ? "  Position.y = -Position.y;\n" : "";
      const std::string inject =
          "  vec2 dolphin_fsq = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
          "  vec4 Position = vec4(dolphin_fsq * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);\n"
          "  vec2 TexCoord = dolphin_fsq;\n" +
          flip;
      const auto main_pos = out.find("void main");
      if (main_pos != std::string::npos)
      {
        const auto brace = out.find('{', main_pos);
        if (brace != std::string::npos)
          out.insert(brace + 1, "\n" + inject);
      }
    }

    // Body rewrites, applied before the shims are prepended so that the shims' own calls to the
    // real built-ins are not renamed into recursive calls to themselves.
    out = ReplaceWord(out, "FragColor", "ocol0");
    // Sampler function parameters -- the declarations above are already emitted as the array type.
    // Whole-word matching leaves `sampler2DArray`, `isampler2D` and `usampler2D` alone.
    const std::string sampler_type(SlangSamplerGlslType(SLANG_INPUT_TEXTURE_TYPE));
    out = ReplaceWord(out, "sampler2D", sampler_type);

    // Rename each shimmed built-in's call sites onto its helper. Order between built-ins does not
    // matter and cannot be made to matter: the match is whole-word and the prefix ends in `_`, a
    // word character, so `dolphin_texture` can never be re-matched as `texture`, and
    // `textureLod` was never a match for `texture` to begin with.
    std::string_view renamed;
    for (const SamplerShim& shim : SAMPLER_SHIMS)
    {
      if (shim.builtin == renamed)
        continue;  // two helpers of one built-in; the rename is per built-in
      renamed = shim.builtin;
      out = ReplaceWord(out, std::string(shim.builtin), ShimHelperName(shim.builtin),
                        /*function_names_only=*/true);
    }

    // Emit only the shims this stage reaches for -- see the note on SAMPLER_SHIMS, including why
    // this gate is a size optimization and not the thing that makes low GLSL versions work. The
    // trigger is the helper name in the renamed source, with comments stripped: the rename fires
    // inside comments too (IsFunctionNameUse reads no context), and a commented-out call is the
    // whole of `textureGather` in the fsr tree.
    const std::string gate_source = StripComments(out);
    std::string shims;
    for (const SamplerShim& shim : SAMPLER_SHIMS)
    {
      if (is_vertex && shim.fragment_only)
        continue;
      if (!ContainsWord(gate_source, ShimHelperName(shim.builtin)))
        continue;
      if (shim.version_guard.empty())
      {
        shims += shim.glsl;
      }
      else
      {
        shims += "#if ";
        shims += shim.version_guard;
        shims += '\n';
        shims += shim.glsl;
        shims += "#endif\n";
      }
    }
    if (!shims.empty())
      shims = "\n" + shims;
    return shims + out;
  };

  // Assemble: merged UBO block + #undef of compat macros, then the stage body.
  result.vertex_glsl = ubo_decl + undefs + rewrite_stage(vs_noblocks, /*is_vertex=*/true);
  result.fragment_glsl = ubo_decl + undefs + rewrite_stage(fs_noblocks, /*is_vertex=*/false);
  result.sampler_names = std::move(sampler_names);
  result.ubo_members = std::move(typed_members);
  result.ok = true;

  // known_aliases / lut_names are accepted for interface completeness and future
  // validation; sampler discovery already picks up whatever the shader references.
  (void)known_aliases;
  (void)lut_names;
  return result;
}

std::vector<u8> PackSlangUniforms(const std::vector<UboMember>& members,
                                  const UniformResolver& resolver)
{
  // std140: {align, size} in bytes for each supported type.
  const auto layout = [](UboMemberType t) -> std::pair<size_t, size_t> {
    switch (t)
    {
    case UboMemberType::Float:
      return {4, 4};
    case UboMemberType::Vec2:
      return {8, 8};
    case UboMemberType::Vec3:
      return {16, 12};
    case UboMemberType::Vec4:
      return {16, 16};
    case UboMemberType::Mat4:
      return {16, 64};
    case UboMemberType::Unknown:
    default:
      return {16, 16};  // conservative: treat as vec4
    }
  };
  const auto component_count = [](UboMemberType t) -> int {
    switch (t)
    {
    case UboMemberType::Float:
      return 1;
    case UboMemberType::Vec2:
      return 2;
    case UboMemberType::Vec3:
      return 3;
    case UboMemberType::Mat4:
      return 16;
    default:
      return 4;
    }
  };

  std::vector<u8> buffer;
  size_t offset = 0;
  for (const UboMember& member : members)
  {
    const auto [align, size] = layout(member.type);
    offset = (offset + align - 1) & ~(align - 1);
    if (buffer.size() < offset + size)
      buffer.resize(offset + size, 0);

    const int count = component_count(member.type);
    std::array<float, 16> values = {};
    if (!resolver || !resolver(member.name, values.data(), count))
      values.fill(0.0f);
    std::memcpy(buffer.data() + offset, values.data(), size);
    offset += size;
  }

  // std140 rounds the whole block up to a multiple of 16.
  const size_t rounded = (buffer.size() + 15) & ~static_cast<size_t>(15);
  buffer.resize(rounded, 0);
  return buffer;
}

std::unique_ptr<AbstractShader> CompileTranslatedVertex(const std::string& vertex_glsl,
                                                        const std::string& include_dir)
{
  // #include resolver rooted at the shader's own directory and the Sys shaders dir.
  ShaderIncluder includer(include_dir + DIR_SEP, File::GetSysDirectory() + SHADERS_DIR DIR_SEP);
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, vertex_glsl, &includer,
                                       "slang post-process vertex");
}

CompiledPassShaders CompileTranslatedPass(const TranslatedPass& pass,
                                          const std::string& include_dir)
{
  CompiledPassShaders out;
  if (!pass.ok)
    return out;

  // #include resolver rooted at the shader's own directory and the Sys shaders dir.
  ShaderIncluder includer(include_dir + DIR_SEP,
                          File::GetSysDirectory() + SHADERS_DIR DIR_SEP);

  out.vertex = CompileTranslatedVertex(pass.vertex_glsl, include_dir);
  if (!out.vertex)
    return {};

  out.pixel = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, pass.fragment_glsl, &includer,
                                            "slang post-process fragment");
  if (!out.pixel)
    return {};

  return out;
}
}  // namespace VideoCommon
