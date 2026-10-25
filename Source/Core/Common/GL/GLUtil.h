// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/GL/GLExtensions/GLExtensions.h"

class GLContext;

// Scratch texture unit, used to bind a texture without disturbing the other bindings. The name is
// historical: it does not actually avoid disturbing anything, because unit 10 is inside
// VideoCommon::MAX_PIXEL_SHADER_SAMPLERS (16) and so is a real sampler slot. Never
// glActiveTexture(GL_MUTABLE_TEXTURE_INDEX) directly -- call OGL::ActivateMutableTextureUnit(),
// which also drops OGLGfx's cached binding for the slot.
constexpr GLuint GL_MUTABLE_TEXTURE_UNIT = 10;
constexpr GLenum GL_MUTABLE_TEXTURE_INDEX = GL_TEXTURE0 + GL_MUTABLE_TEXTURE_UNIT;

namespace GLUtil
{
GLuint CompileProgram(const std::string& vertexShader, const std::string& fragmentShader);
void EnablePrimitiveRestart(const GLContext* context);
}  // namespace GLUtil
