// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/OnScreenUI.h"

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <string_view>

#include "Common/BigPicture/PresentationState.h"
#include "Common/CommonPaths.h"
#include "Common/EnumMap.h"
#include "Common/FileUtil.h"
#include "Common/Image.h"
#include "Common/Profiler.h"
#include "Common/Timer.h"
#include "VideoCommon/BigPicture/InGameMenu.h"

#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"
#include "Core/Movie.h"
#include "Core/System.h"
#include "InputCommon/GCAdapter.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PerformanceMetrics.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoEvents.h"

#include <bit>
#include <inttypes.h>
#include <mutex>

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>

namespace VideoCommon
{
namespace
{
// Callers validate image bounds before uploading. Cache lifetimes remain with the graphics owner.
std::unique_ptr<AbstractTexture> UploadMenuImage(const ::BigPicture::LibraryImage& image,
                                                 std::string_view label)
{
  TextureConfig config;
  config.width = image.width;
  config.height = image.height;
  auto texture = g_gfx->CreateTexture(config, label);
  if (texture)
    texture->Load(0, image.width, image.height, image.width, image.rgba.data(), image.rgba.size());
  return texture;
}
}  // namespace

bool OnScreenUI::Initialize(u32 width, u32 height, float scale)
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);

  if (!IMGUI_CHECKVERSION())
  {
    PanicAlertFmt("ImGui version check failed");
    return false;
  }
  if (!ImGui::CreateContext())
  {
    PanicAlertFmt("Creating ImGui context failed");
    return false;
  }
  if (!ImPlot::CreateContext())
  {
    PanicAlertFmt("Creating ImPlot context failed");
    return false;
  }

  // Don't create an ini file. TODO: Do we want this in the future?
  ImGui::GetIO().IniFilename = nullptr;
  SetScale(scale);

  PortableVertexDeclaration vdecl = {};
  vdecl.position = {ComponentFormat::Float, 2, offsetof(ImDrawVert, pos), true, false};
  vdecl.texcoords[0] = {ComponentFormat::Float, 2, offsetof(ImDrawVert, uv), true, false};
  vdecl.colors[0] = {ComponentFormat::UByte, 4, offsetof(ImDrawVert, col), true, false};
  vdecl.stride = sizeof(ImDrawVert);
  m_imgui_vertex_format = g_gfx->CreateNativeVertexFormat(vdecl);
  if (!m_imgui_vertex_format)
  {
    PanicAlertFmt("Failed to create ImGui vertex format");
    return false;
  }

  // Font defaults
  ImGuiIO& io = ImGui::GetIO();
  m_imgui_textures.clear();

  // User supplied font
  std::string file = File::GetUserPath(D_LOAD_IDX) + "OSD_Font.ttf";

  bool font_exists = File::Exists(file);
  if (!font_exists)
  {
    // Default supplied font
    file = File::GetSysDirectory() + DIR_SEP + RESOURCES_DIR + DIR_SEP + "OSD_Font.ttf";
    font_exists = File::Exists(file);
  }

  if (font_exists)
  {
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF(file.c_str());
  }

  // Setup new font management behavior
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;

  if (!RecompileImGuiPipeline())
    return false;

  m_imgui_last_frame_time = Common::Timer::NowUs();
  m_ready = true;
  // Software can initialize the OSD resources but cannot draw the modal menu.
  ::BigPicture::GetPresentationState().SetRendererReady(g_gfx->SupportsUtilityDrawing());
  BeginImGuiFrameUnlocked(width, height);  // lock is already held

  return true;
}

OnScreenUI::~OnScreenUI()
{
  ::BigPicture::GetPresentationState().SetRendererReady(false);
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);

  if (ImGui::GetCurrentContext())
  {
    if (m_ready)
      ImGui::EndFrame();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
  }
  m_imgui_textures.clear();
}

bool OnScreenUI::RecompileImGuiPipeline()
{
  if (g_presenter->GetBackbufferFormat() == AbstractTextureFormat::Undefined)
  {
    // No backbuffer (nogui) means no imgui rendering will happen
    // Some backends don't like making pipelines with no render targets
    return true;
  }

  const bool linear_space_output =
      g_presenter->GetBackbufferFormat() == AbstractTextureFormat::RGBA16F;

  std::unique_ptr<AbstractShader> vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateImGuiVertexShader(), nullptr,
      "ImGui vertex shader");
  std::unique_ptr<AbstractShader> pixel_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateImGuiPixelShader(linear_space_output),
      nullptr, "ImGui pixel shader");
  if (!vertex_shader || !pixel_shader)
  {
    PanicAlertFmt("Failed to compile ImGui shaders");
    return false;
  }

  // GS is used to render the UI to both eyes in stereo modes.
  std::unique_ptr<AbstractShader> geometry_shader;
  if (g_gfx->UseGeometryShaderForUI())
  {
    geometry_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Geometry, FramebufferShaderGen::GeneratePassthroughGeometryShader(1, 1),
        nullptr, "ImGui passthrough geometry shader");
    if (!geometry_shader)
    {
      PanicAlertFmt("Failed to compile ImGui geometry shader");
      return false;
    }
  }

  AbstractPipelineConfig pconfig = {};
  pconfig.vertex_format = m_imgui_vertex_format.get();
  pconfig.vertex_shader = vertex_shader.get();
  pconfig.geometry_shader = geometry_shader.get();
  pconfig.pixel_shader = pixel_shader.get();
  pconfig.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  pconfig.depth_state = RenderState::GetNoDepthTestingDepthState();
  pconfig.blending_state = RenderState::GetNoBlendingBlendState();
  pconfig.blending_state.blend_enable = true;
  pconfig.blending_state.src_factor = SrcBlendFactor::SrcAlpha;
  pconfig.blending_state.dst_factor = DstBlendFactor::InvSrcAlpha;
  pconfig.blending_state.src_factor_alpha = SrcBlendFactor::Zero;
  pconfig.blending_state.dst_factor_alpha = DstBlendFactor::One;
  pconfig.framebuffer_state.color_texture_format = g_presenter->GetBackbufferFormat();
  pconfig.framebuffer_state.depth_texture_format = AbstractTextureFormat::Undefined;
  pconfig.framebuffer_state.samples = 1;
  pconfig.framebuffer_state.per_sample_shading = false;
  pconfig.usage = AbstractPipelineUsage::Utility;
  m_imgui_pipeline = g_gfx->CreatePipeline(pconfig);
  if (!m_imgui_pipeline)
  {
    PanicAlertFmt("Failed to create imgui pipeline");
    return false;
  }

  return true;
}

void OnScreenUI::BeginImGuiFrame(u32 width, u32 height)
{
  std::unique_lock<std::mutex> imgui_lock(m_imgui_mutex);
  BeginImGuiFrameUnlocked(width, height);
}

void OnScreenUI::BeginImGuiFrameUnlocked(u32 width, u32 height)
{
  m_backbuffer_width = width;
  m_backbuffer_height = height;

  const u64 current_time_us = Common::Timer::NowUs();
  const u64 time_diff_us = current_time_us - m_imgui_last_frame_time;
  const float time_diff_secs = static_cast<float>(time_diff_us / 1000000.0);
  m_imgui_last_frame_time = current_time_us;

  // Update I/O with window dimensions.
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize =
      ImVec2(static_cast<float>(m_backbuffer_width), static_cast<float>(m_backbuffer_height));
  io.DeltaTime = time_diff_secs;

  ProcessInput();

  ImGui::NewFrame();
}

void OnScreenUI::DrawImGui()
{
  ImDrawData* draw_data = ImGui::GetDrawData();
  if (!draw_data)
    return;

  g_gfx->SetViewport(0.0f, 0.0f, static_cast<float>(m_backbuffer_width),
                     static_cast<float>(m_backbuffer_height), 0.0f, 1.0f);

  // Uniform buffer for draws.
  struct ImGuiUbo
  {
    float u_rcp_viewport_size_mul2[2];
    float padding[2];
  };
  ImGuiUbo ubo = {{1.0f / m_backbuffer_width * 2.0f, 1.0f / m_backbuffer_height * 2.0f}};

  // Set up common state for drawing.
  g_gfx->SetPipeline(m_imgui_pipeline.get());
  g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_vertex_manager->UploadUtilityUniforms(&ubo, sizeof(ubo));

  for (int i = 0; i < draw_data->CmdListsCount; i++)
  {
    const ImDrawList* cmdlist = draw_data->CmdLists[i];
    if (cmdlist->VtxBuffer.empty() || cmdlist->IdxBuffer.empty())
      return;

    u32 base_vertex, base_index;
    g_vertex_manager->UploadUtilityVertices(cmdlist->VtxBuffer.Data, sizeof(ImDrawVert),
                                            cmdlist->VtxBuffer.Size, cmdlist->IdxBuffer.Data,
                                            cmdlist->IdxBuffer.Size, &base_vertex, &base_index);

    for (const ImDrawCmd& cmd : cmdlist->CmdBuffer)
    {
      if (cmd.UserCallback)
      {
        cmd.UserCallback(cmdlist, &cmd);
        continue;
      }

      g_gfx->SetScissorRect(g_gfx->ConvertFramebufferRectangle(
          MathUtil::Rectangle<int>(
              static_cast<int>(cmd.ClipRect.x), static_cast<int>(cmd.ClipRect.y),
              static_cast<int>(cmd.ClipRect.z), static_cast<int>(cmd.ClipRect.w)),
          g_gfx->GetCurrentFramebuffer()));
      g_gfx->SetTexture(0, reinterpret_cast<const AbstractTexture*>(cmd.GetTexID()));
      g_gfx->DrawIndexed(base_index, cmd.ElemCount, base_vertex);
      base_index += cmd.ElemCount;
    }
  }

  // Some capture software (such as OBS) hooks SwapBuffers and uses glBlitFramebuffer to copy our
  // back buffer just before swap. Because glBlitFramebuffer honors the scissor test, the capture
  // itself will be clipped to whatever bounds were last set by ImGui, resulting in a rather useless
  // capture whenever any ImGui windows are open. We'll reset the scissor rectangle to the entire
  // viewport here to avoid this problem.
  g_gfx->SetScissorRect(g_gfx->ConvertFramebufferRectangle(
      MathUtil::Rectangle<int>(0, 0, m_backbuffer_width, m_backbuffer_height),
      g_gfx->GetCurrentFramebuffer()));
}

// Create On-Screen-Messages
void OnScreenUI::DrawDebugText()
{
  if (Config::Get(Config::MAIN_MOVIE_SHOW_OSD))
  {
    // Position under the FPS display.
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x - ImGui::GetFontSize() * m_backbuffer_scale,
               80.f * m_backbuffer_scale),
        ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(5.0f * ImGui::GetFontSize() * m_backbuffer_scale,
                                               2.1f * ImGui::GetFontSize() * m_backbuffer_scale),
                                        ImGui::GetIO().DisplaySize);
    if (ImGui::Begin("Movie", nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
    {
      auto& movie = Core::System::GetInstance().GetMovie();
      if (movie.IsPlayingInput())
      {
        ImGui::Text("Frame: %" PRIu64 " / %" PRIu64, movie.GetCurrentFrame(),
                    movie.GetTotalFrames());
        ImGui::Text("Input: %" PRIu64 " / %" PRIu64, movie.GetCurrentInputCount(),
                    movie.GetTotalInputCount());
      }
      else if (Config::Get(Config::MAIN_SHOW_FRAME_COUNT))
      {
        ImGui::Text("Frame: %" PRIu64, movie.GetCurrentFrame());
        if (movie.IsRecordingInput())
          ImGui::Text("Input: %" PRIu64, movie.GetCurrentInputCount());
      }
      if (Config::Get(Config::MAIN_SHOW_LAG))
        ImGui::Text("Lag: %" PRIu64 "\n", movie.GetCurrentLagCount());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_INPUT_DISPLAY))
        ImGui::TextUnformatted(movie.GetInputDisplay().c_str());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_RTC))
        ImGui::TextUnformatted(movie.GetRTCDisplay().c_str());
      if (Config::Get(Config::MAIN_MOVIE_SHOW_RERECORD))
        ImGui::TextUnformatted(movie.GetRerecords().c_str());
    }
    ImGui::End();
  }

  if (g_ActiveConfig.bOverlayStats)
    g_stats.Display();

  if (!::BigPicture::GetPresentationState().IsModal())
  {
    if (const auto chat = GetNetPlayChatUI();
        chat && Config::Get(Config::GFX_SHOW_NETPLAY_MESSAGES))
      chat->Display();
    if (const auto golf = GetNetPlayGolfUI();
        golf && Config::Get(Config::NETPLAY_GOLF_MODE_OVERLAY))
      golf->Display();
  }

  if (g_ActiveConfig.bOverlayProjStats)
    g_stats.DisplayProj();

  if (g_ActiveConfig.bOverlayScissorStats)
    g_stats.DisplayScissor();

  const std::string profile_output = Common::Profiler::ToString();
  if (!profile_output.empty())
    ImGui::TextUnformatted(profile_output.c_str());

  if (GCAdapter::IsReadingAtReducedRate() && Config::Get(Config::MAIN_SHOW_ADAPTER_WARNING))
  {
    ImGui::TextWrapped(
      "Your GameCube Controller Adapter is reading inputs at a reduced rate.\n"
      "You can still play normally but you will experience higher input lag.\n"
      "This indicates a potential hardware or driver issue.\n"
      "\n"
      "If you're using a computer with an AMD Ryzen processor:\n"
      "Try connecting the black plug of your adapter to a USB 3.0/3.1 Gen 1 port on your motherboard.\n"
      "These ports are usually blue or say \"SS\" (SuperSpeed). USB 3.2 (light blue) or USB 2.0 (black) will NOT work.\n"
      "\n"
      "The recommended driver on Windows is WinUSB. If you're using another driver in Zadig try switching to WinUSB.\n"
      "\n"
      "You can turn this message off by going to \"Config\" and then \"Advanced\".\n"
      "Under \"Troubleshooting\", uncheck \"Show a message when inputs are being read at a reduced rate\".");

  }
}

void OnScreenUI::DrawChallengesAndLeaderboards()
{
  if (!Config::Get(Config::MAIN_OSD_MESSAGES))
    return;
#ifdef USE_RETRO_ACHIEVEMENTS
  auto& instance = AchievementManager::GetInstance();
  std::lock_guard lg{instance.GetLock()};
  const bool challenge_indicators_enabled = Config::Get(Config::RA_CHALLENGE_INDICATORS_ENABLED);
  const bool challenges_updated = instance.AreChallengesUpdated();
  const auto& challenges = instance.GetActiveChallenges();

  if (!challenge_indicators_enabled)
  {
    if (challenges_updated)
      instance.ResetChallengesUpdated();
    if (!m_challenge_texture_map.empty())
      m_challenge_texture_map.clear();
  }
  else if (challenges_updated || m_challenge_texture_map.size() != challenges.size())
  {
    if (challenges_updated)
      instance.ResetChallengesUpdated();
    m_challenge_texture_map.clear();
    for (const auto& name : challenges)
    {
      const auto& icon = instance.GetAchievementBadge(name, false);
      const u32 width = icon.width;
      const u32 height = icon.height;
      TextureConfig tex_config(width, height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                               AbstractTextureType::Texture_2DArray);
      auto res = m_challenge_texture_map.insert_or_assign(name, g_gfx->CreateTexture(tex_config));
      res.first->second->Load(0, width, height, width, icon.data.data(),
                              sizeof(u32) * width * height);
    }
  }

  float leaderboard_y = ImGui::GetIO().DisplaySize.y;
  if (challenge_indicators_enabled && !m_challenge_texture_map.empty())
  {
    float scale = ImGui::GetIO().DisplaySize.y / 1024.0;
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), 0,
                            ImVec2(1, 1));
    if (ImGui::Begin("Challenges", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
    {
      for (auto& [name, texture] : m_challenge_texture_map)
      {
        ImGui::Image(*texture.get(), ImVec2(static_cast<float>(texture->GetWidth()) * scale,
                                            static_cast<float>(texture->GetHeight()) * scale));
        ImGui::SameLine();
      }
    }
    leaderboard_y -= ImGui::GetWindowHeight();
    ImGui::End();
  }

  const auto& leaderboard_progress = instance.GetActiveLeaderboards();
  if (Config::Get(Config::RA_LEADERBOARD_TRACKER_ENABLED) && !leaderboard_progress.empty())
  {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, leaderboard_y), 0,
                            ImVec2(1.0, 1.0));
    ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Leaderboards", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
    {
      for (const auto& value : leaderboard_progress)
        ImGui::TextUnformatted(value.c_str());
    }
    ImGui::End();
  }
#endif  // USE_RETRO_ACHIEVEMENTS
}

void OnScreenUI::Finalize(bool draw_emulation_overlays)
{
  auto lock = GetImGuiLock();

  if (draw_emulation_overlays)
  {
    auto& perf_metrics = Core::System::GetInstance().GetPerfMetrics();
    perf_metrics.DrawImGuiStats(m_backbuffer_scale);
    DrawDebugText();
  }
  OSD::DrawMessages();
  if (draw_emulation_overlays)
    DrawChallengesAndLeaderboards();
  GetVideoEvents().host_ui_frame_event.Trigger();
  DrawNavigation();
  ImGui::Render();
  ::BigPicture::GetPresentationState().PublishCapture(
      m_input_epoch, ImGui::GetIO().WantCaptureKeyboard, ImGui::GetIO().WantCaptureMouse);
  ::BigPicture::GetPresentationState().FrameCompleted();

  // Check for font changes
  ImGuiStyle& style = ImGui::GetStyle();
  const int size = Config::Get(Config::MAIN_OSD_FONT_SIZE);
  if (size != style.FontSizeBase)
    style.FontSizeBase = static_cast<float>(size);

  // Create or update fonts.
  ImDrawData* draw_data = ImGui::GetDrawData();
  if (draw_data->Textures != nullptr)
    for (ImTextureData* tex : *draw_data->Textures)
      if (tex->Status != ImTextureStatus_OK)
        UpdateImguiTexture(tex);
}

void OnScreenUI::UpdateImguiTexture(ImTextureData* tex)
{
  if (tex->Status == ImTextureStatus_WantCreate)
  {
    // Create new font texture.
    IM_ASSERT(tex->TexID == ImTextureID_Invalid);
    IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

    TextureConfig font_tex_config(tex->Width, tex->Height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                                  AbstractTextureType::Texture_2DArray);
    std::unique_ptr<AbstractTexture> font_tex =
        g_gfx->CreateTexture(font_tex_config, "ImGui font texture");

    if (!font_tex)
    {
      PanicAlertFmt("Failed to create ImGui texture");
      return;
    }

    font_tex->Load(0, tex->Width, tex->Height, tex->Width, tex->Pixels,
                   sizeof(u32) * tex->Width * tex->Height);

    tex->SetTexID(static_cast<ImTextureID>(*font_tex.get()));
    // Keeps the texture alive.
    m_imgui_textures.push_back(std::move(font_tex));
    tex->SetStatus(ImTextureStatus_OK);
  }
  else if (tex->Status == ImTextureStatus_WantUpdates)
  {
    AbstractTexture* font_tex = reinterpret_cast<AbstractTexture*>(tex->GetTexID());

    if (!font_tex || tex->TexID == ImTextureID_Invalid)
    {
      PanicAlertFmt("ImGui texture not created before update");
      return;
    }

    for (const ImTextureRect& r : tex->Updates)
    {
      // Rect of texture that will be updated.
      const int x_offset = static_cast<int>(r.x);
      const int y_offset = static_cast<int>(r.y);
      const int width = static_cast<int>(r.w);
      const int height = static_cast<int>(r.h);

      // Create a staging texture to update the font texture with.
      TextureConfig font_tex_config(width, height, 1, 1, 1, AbstractTextureFormat::RGBA8, 0,
                                    AbstractTextureType::Texture_2DArray);
      std::unique_ptr<AbstractStagingTexture> stage =
          g_gfx->CreateStagingTexture(StagingTextureType::Upload, font_tex_config);

      const int src_pitch = width * tex->BytesPerPixel;

      // Write to staging texture.
      for (int y = 0; y < height; y++)
      {
        const MathUtil::Rectangle<int> rect_line = {0, y, width, y + 1};
        stage->WriteTexels(rect_line, tex->GetPixelsAt(x_offset, y_offset + y), src_pitch);
      }

      // Copy to font texture.
      const MathUtil::Rectangle<int> rect_staging = {0, 0, width, height};
      const MathUtil::Rectangle<int> rect_target = {x_offset, y_offset, width + x_offset,
                                                    height + y_offset};

      stage->CopyToTexture(rect_staging, font_tex, rect_target, 0, 0);
    }

    tex->SetStatus(ImTextureStatus_OK);
  }
  else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
  {
    AbstractTexture* font_tex = reinterpret_cast<AbstractTexture*>(tex->GetTexID());

    tex->SetTexID(ImTextureID_Invalid);

    m_imgui_textures.erase(
        std::find_if(m_imgui_textures.begin(), m_imgui_textures.end(),
                     [font_tex](auto& element) { return element.get() == font_tex; }));

    tex->Status = ImTextureStatus_Destroyed;
  }
}

std::unique_lock<std::mutex> OnScreenUI::GetImGuiLock()
{
  return std::unique_lock<std::mutex>(m_imgui_mutex);
}

void OnScreenUI::SetScale(float backbuffer_scale)
{
  ImGui::GetIO().DisplayFramebufferScale.x = backbuffer_scale;
  ImGui::GetIO().DisplayFramebufferScale.y = backbuffer_scale;

  // ScaleAllSizes scales in-place, so calling it twice will double-apply the scale
  // Reset the style first so that the scale is applied to the base style, not an already-scaled one
  ImGuiStyle& style = ImGui::GetStyle();
  style = {};
  style.FontScaleMain = backbuffer_scale;
  style.WindowRounding = 7.0f;
  style.ScaleAllSizes(backbuffer_scale);

  m_backbuffer_scale = backbuffer_scale;
}
void OnScreenUI::SetKeyMap(const DolphinKeyMap& key_map)
{
  ::BigPicture::GetPresentationState().SetKeyMap({key_map.begin(), key_map.end()});
}

void OnScreenUI::ProcessInput()
{
  static constexpr DolphinKeyMap dolphin_to_imgui_map = {
      ImGuiKey_Tab,       ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow,
      ImGuiKey_DownArrow, ImGuiKey_PageUp,    ImGuiKey_PageDown,   ImGuiKey_Home,
      ImGuiKey_End,       ImGuiKey_Insert,    ImGuiKey_Delete,     ImGuiKey_Backspace,
      ImGuiKey_Space,     ImGuiKey_Enter,     ImGuiKey_Escape,     ImGuiKey_KeypadEnter,
      ImGuiKey_A,         ImGuiKey_C,         ImGuiKey_V,          ImGuiKey_X,
      ImGuiKey_Y,         ImGuiKey_Z,         ImGuiKey_F11,        ImGuiKey_F1,
      ImGuiKey_F2,        ImGuiKey_F3,        ImGuiKey_F4,         ImGuiKey_F5,
      ImGuiKey_F6,
  };

  auto input = ::BigPicture::GetPresentationState().TakeInput();
  m_input_epoch = input.epoch;
  const bool mapping_changed =
      m_navigation.options.swap_confirm_back != input.navigation.options.swap_confirm_back;
  m_navigation = std::move(input.navigation);
  auto& io = ImGui::GetIO();
  const auto snapshot = ::BigPicture::GetPresentationState().GetSnapshot();
  const bool modal = snapshot && snapshot->mode != ::BigPicture::Mode::Desktop &&
                     ::BigPicture::GetPresentationState().IsModal();
  // ImGui hides the keyboard focus highlight after mouse input; the navigation toolkit shows
  // it again for keyboard and gamepad input.
  io.ConfigNavCursorVisibleAlways = false;
  io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
  io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
  if (modal)
  {
    if (!m_menu_font)
    {
      // Keep menu text sizes and fallback glyphs independent of diagnostic OSD settings.
      const auto fallback_paths = ::BigPicture::GetPresentationState().GetMenuFontPaths();
      auto font_path = File::GetUserPath(D_LOAD_IDX) + "OSD_Font.ttf";
      if (!File::Exists(font_path))
        font_path = File::GetSysDirectory() + RESOURCES_DIR "/BigPicture/Roboto-Regular.ttf";
      if (!File::Exists(font_path) && !fallback_paths.empty())
        font_path = fallback_paths.front();
      if (File::Exists(font_path))
      {
        static constexpr ImWchar prompt_ranges[] = {
            0x2198, 0x2199, 0x219c, 0x21a3, 0x21b0, 0x21b3, 0x21ba, 0x21bb, 0x21ce, 0x21ce, 0x21d0,
            0x21d3, 0x21e0, 0x21e8, 0x235d, 0x2367, 0x23f4, 0x23f7, 0x242e, 0x242f, 0x243a, 0x243c,
            0x2460, 0x2464, 0x246a, 0x246a, 0x2699, 0x2699, 0xe005, 0xf8ff, 0};
        ImFontConfig config;
        config.GlyphExcludeRanges = prompt_ranges;
        m_menu_font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 0, &config);
      }
      if (!m_menu_font)
        m_menu_font = io.Fonts->AddFontDefault();
      const auto prompt_path = File::GetSysDirectory() + RESOURCES_DIR "/BigPicture/promptfont.otf";
      if (File::Exists(prompt_path))
      {
        ImFontConfig config;
        config.MergeMode = true;
        config.DstFont = m_menu_font;
        io.Fonts->AddFontFromFileTTF(prompt_path.c_str(), 0, &config);
      }
      const auto awesome_path =
          File::GetSysDirectory() + RESOURCES_DIR "/BigPicture/fa-solid-900.ttf";
      if (File::Exists(awesome_path))
      {
        ImFontConfig config;
        config.MergeMode = true;
        config.DstFont = m_menu_font;
        io.Fonts->AddFontFromFileTTF(awesome_path.c_str(), 0, &config);
      }
      for (const auto& path : fallback_paths)
      {
        if (path == font_path || !File::Exists(path))
          continue;
        ImFontConfig config;
        config.MergeMode = true;
        config.DstFont = m_menu_font;
        io.Fonts->AddFontFromFileTTF(path.c_str(), 0, &config);
      }
    }
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    if (m_navigation.gamepad)
      io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  }
  if (input.reset || mapping_changed)
  {
    m_navigation_input.Reset();
    io.ClearInputKeys();
    io.ClearInputMouse();
    io.ClearEventsQueue();
    ImGui::SetWindowFocus(nullptr);
    m_navigation_toolkit.InvalidateFocus();
    m_pending_menu.DiscardHeld();
  }

  m_dolphin_to_imgui_map.clear();
  for (int dolphin_key = 0; dolphin_key <= static_cast<int>(DolphinKey::F6); dolphin_key++)
  {
    const int imgui_key = dolphin_to_imgui_map[DolphinKey(dolphin_key)];
    if (imgui_key >= 0)
    {
      if (static_cast<size_t>(dolphin_key) < input.key_map.size())
        m_dolphin_to_imgui_map[input.key_map[dolphin_key]] = imgui_key;
    }
  }
  for (const auto& event : input.events)
  {
    switch (event.type)
    {
    case ::BigPicture::InputEvent::Type::Key:
      if (event.down)
        m_navigation_input.SetSource(true);
      if (auto it = m_dolphin_to_imgui_map.find(event.key); it != m_dolphin_to_imgui_map.end())
        m_navigation_input.KeyEvent(static_cast<ImGuiKey>(it->second), event.down, true);
      [[fallthrough]];
    case ::BigPicture::InputEvent::Type::Text:
      if (!event.text.empty())
        io.AddInputCharactersUTF8(event.text.c_str());
      break;
    case ::BigPicture::InputEvent::Type::Navigation:
      if (modal && event.key < ::BigPicture::NAV_BUTTON_COUNT)
      {
        const auto key =
            BigPicture::GetNavigationKey(static_cast<::BigPicture::NavButton>(event.key),
                                         m_navigation.options.swap_confirm_back);
        if (key != ImGuiKey_None)
          m_navigation_input.KeyEvent(key, event.down, false);
        if (event.down)
          io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
      }
      break;
    case ::BigPicture::InputEvent::Type::MousePosition:
      io.AddMousePosEvent(event.x * io.DisplaySize.x, event.y * io.DisplaySize.y);
      break;
    case ::BigPicture::InputEvent::Type::MouseWheel:
      io.AddMouseWheelEvent(event.x, event.y);
      break;
    case ::BigPicture::InputEvent::Type::MouseLeave:
      io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
      break;
    case ::BigPicture::InputEvent::Type::MouseButtons:
      for (size_t i = 0; i < std::size(io.MouseDown); ++i)
        io.AddMouseButtonEvent(static_cast<int>(i), (event.buttons & (1u << i)) != 0);
      break;
    case ::BigPicture::InputEvent::Type::Focus:
      io.AddFocusEvent(event.down);
      break;
    case ::BigPicture::InputEvent::Type::Reset:
      m_navigation_input.Reset();
      io.ClearInputKeys();
      io.ClearInputMouse();
      io.ClearEventsQueue();
      ImGui::SetWindowFocus(nullptr);
      m_pending_menu.DiscardHeld();
      break;
    }
  }
}

void OnScreenUI::DrawNavigation()
{
  auto& presentation = ::BigPicture::GetPresentationState();
  const auto snapshot = presentation.GetSnapshot();
  if (!snapshot || snapshot->mode == ::BigPicture::Mode::Desktop || !presentation.IsModal())
  {
    m_navigation_toolkit.InvalidateFocus();
    m_menu_snapshot.reset();
    m_menu = {};
    m_pending_menu.Reset();
    return;
  }
  if (m_navigation_generation != snapshot->generation)
  {
    m_navigation_toolkit.Reset();
    m_state_textures = {};
    m_achievement_textures.clear();
    m_navigation_generation = snapshot->generation;
  }
  const auto shared_text = presentation.GetNavigationText();
  const auto& text = *shared_text;
  if (text.title.empty())
    return;
  auto options = m_navigation.options;
  const auto now = BigPicture::PendingMenuHold::Clock::now();
  if (m_menu_snapshot != snapshot || m_menu_text != shared_text || m_menu_options != options)
  {
    m_pending_menu.Replace(m_menu_snapshot.get(), m_menu, *snapshot, now);
    m_menu = BigPicture::BuildInGameMenu(*snapshot, text, options);
    m_menu_snapshot = snapshot;
    m_menu_text = shared_text;
    m_menu_options = options;
  }
  const auto& menu = m_pending_menu.Visible(*snapshot, m_menu, now);
  if (m_achievement_snapshot != menu.page.achievements)
  {
    m_achievement_snapshot = menu.page.achievements;
    m_achievement_images.clear();
    if (m_achievement_snapshot)
      for (const auto& subset : m_achievement_snapshot->subsets)
      {
        m_achievement_images.emplace("achievement:subset:" + std::to_string(subset.id),
                                     subset.badge);
        for (const auto& group : subset.groups)
          for (const auto& achievement : group.achievements)
            m_achievement_images.emplace("achievement:badge:" + std::to_string(achievement.id),
                                         achievement.badge);
      }
    std::erase_if(m_achievement_textures,
                  [&](const auto& entry) { return !m_achievement_images.contains(entry.first); });
  }
  if (menu.page.presentation == BigPicture::MenuPresentation::Settings &&
      m_navigation_toolkit.GetMemory(menu.page.id).item.empty())
  {
    if (const auto focus = presentation.GetSettingsFocus(menu.page.id))
    {
      BigPicture::FocusMemory memory;
      memory.item = focus->item;
      memory.selected = focus->selected;
      memory.scroll = focus->scroll;
      m_navigation_toolkit.RestoreMemory(menu.page.id, std::move(memory));
    }
  }
  ++m_cover_frame;
  m_cover_uploads = 0;
  if (snapshot->host.library && m_cover_revision != snapshot->host.library->catalog_revision)
  {
    m_cover_textures.clear();
    m_cover_revision = snapshot->host.library->catalog_revision;
  }
  const bool text_input =
      !snapshot->screens.empty() && snapshot->screens.back() == ::BigPicture::Screen::TextInput;
  const bool home = menu.page.id == "home";
  const bool settings = menu.page.presentation == BigPicture::MenuPresentation::Settings;
  const bool settings_text = menu.page.settings_text;
  const auto prompts = GetNavigationPrompts(menu.page, text, text_input);
  const auto result = m_navigation_toolkit.Draw(
      menu.page, options, prompts, m_menu_font,
      [&](const std::string& path) { return GetNavigationTexture(*snapshot, path); });
  if (settings)
  {
    const auto memory = m_navigation_toolkit.GetMemory(menu.page.id);
    presentation.SetSettingsFocus(menu.page.id, {memory.item, memory.selected, memory.scroll});
  }
  if (!snapshot->pending && snapshot->host.library && menu.page.id == "library" &&
      result.focused.starts_with("game:") &&
      result.focused.substr(5) != snapshot->host.library->selected_path)
  {
    ::BigPicture::NavigationRequest request{snapshot->generation, ::BigPicture::Action::Library};
    request.library_operation = ::BigPicture::LibraryOperation::Select;
    request.path = result.focused.substr(5);
    presentation.RequestNavigation(std::move(request), m_input_epoch);
  }
  bool typed_activation = false;
  if (settings_text && snapshot->host.settings_menu)
  {
    const auto send = [&](::BigPicture::SettingsOperation operation, std::string value = {}) {
      ::BigPicture::NavigationRequest request{snapshot->generation,
                                              ::BigPicture::Action::SettingsMenu};
      request.settings_menu = ::BigPicture::SettingsRequest{snapshot->host.settings_menu->revision,
                                                            operation, std::move(value)};
      presentation.RequestNavigation(std::move(request), m_input_epoch);
    };
    for (const auto c : ImGui::GetIO().InputQueueCharacters)
    {
      if (c < 32)
        continue;
      char buffer[5]{};
      ImTextCharToUtf8(buffer, c);
      send(::BigPicture::SettingsOperation::TextKey, buffer);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
      send(::BigPicture::SettingsOperation::TextErase);
    const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    if (m_navigation_input.IsKeyboard() && enter)
      send(::BigPicture::SettingsOperation::TextDone);
    typed_activation =
        m_navigation_input.IsKeyboard() && (enter || ImGui::IsKeyPressed(ImGuiKey_Space));
  }
  if (home && (ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
               ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false)))
  {
    presentation.RequestNavigation({snapshot->generation, ::BigPicture::Action::ShowLibrary},
                                   m_input_epoch);
    typed_activation = true;
  }
  if (result.fullscreen || (home && ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)))
  {
    presentation.RequestNavigation({snapshot->generation, ::BigPicture::Action::Fullscreen},
                                   m_input_epoch);
    typed_activation = true;
  }
  if (home && ImGui::IsKeyPressed(ImGuiKey_F1, false))
    presentation.RequestNavigation({snapshot->generation, ::BigPicture::Action::About},
                                   m_input_epoch);
  if (text_input)
  {
    const auto send = [&](::BigPicture::LibraryOperation operation, std::string value = {}) {
      ::BigPicture::NavigationRequest request{snapshot->generation, ::BigPicture::Action::Library};
      request.library_operation = operation;
      request.path = std::move(value);
      presentation.RequestNavigation(std::move(request), m_input_epoch);
    };
    for (const auto c : ImGui::GetIO().InputQueueCharacters)
    {
      if (c < 32)
        continue;
      char buffer[5];
      ImTextCharToUtf8(buffer, c);
      send(::BigPicture::LibraryOperation::TextKey, buffer);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
      send(::BigPicture::LibraryOperation::TextErase);
    const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                       ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    if (m_navigation_input.IsKeyboard() && enter)
      send(::BigPicture::LibraryOperation::TextDone);
    // Physical Space inserts text and Enter submits. Neither should also activate the
    // currently focused on-screen key. Controller Confirm still selects keys normally.
    typed_activation =
        m_navigation_input.IsKeyboard() && (enter || ImGui::IsKeyPressed(ImGuiKey_Space));
  }
  if (!typed_activation)
    if (const auto request = m_pending_menu.Activate(*snapshot, menu, result))
      presentation.RequestNavigation(*request, m_input_epoch);
}

std::string OnScreenUI::GetNavigationPrompts(const BigPicture::MenuPage& page,
                                             const ::BigPicture::NavigationText& text,
                                             bool text_input) const
{
  using Presentation = BigPicture::MenuPresentation;
  bool setting_options = false;
  if (page.presentation == Presentation::Settings)
  {
    const auto memory = m_navigation_toolkit.GetMemory(page.id);
    const auto& selected = memory.selected.empty() ? page.default_focus : memory.selected;
    setting_options = std::ranges::any_of(page.items, [&](const auto& item) {
      return item.id == selected && item.enabled && item.has_options;
    });
  }
  const bool home = page.id == "home";
  const bool pause = page.presentation == Presentation::Pause;
  if (m_navigation_input.IsKeyboard())
  {
    switch (page.presentation)
    {
    case Presentation::Pause:
      return text.pause_keyboard_prompts;
    case Presentation::States:
      return text.state_keyboard_hints;
    case Presentation::Achievements:
      return text.achievements_keyboard_hints;
    case Presentation::Library:
      return text.library_keyboard_hints;
    case Presentation::Settings:
      if (!page.increase.empty())
        return text.settings_adjust_keyboard_hints;
      return text.settings_keyboard_hints +
             (setting_options ? "   " + text.settings_keyboard_options : "");
    case Presentation::FilePicker:
      return text.picker_keyboard_hints;
    case Presentation::Launch:
      return text.start_keyboard_hints;
    default:
      if (home)
        return text.home_keyboard_prompts;
      return text_input || page.settings_text ? text.library_keyboard_prompts :
                                                text.keyboard_prompts;
    }
  }
  switch (page.presentation)
  {
  case Presentation::States:
    return BigPicture::GetStateControllerPrompts(m_navigation, text);
  case Presentation::Achievements:
    return BigPicture::GetAchievementsControllerPrompts(m_navigation, text);
  case Presentation::Library:
    return BigPicture::GetLibraryControllerPrompts(m_navigation, text);
  case Presentation::Settings:
    return BigPicture::GetSettingsControllerPrompts(m_navigation, text, setting_options,
                                                    !page.increase.empty());
  case Presentation::FilePicker:
  case Presentation::Launch:
    return BigPicture::GetLaunchControllerPrompts(m_navigation, text,
                                                  page.presentation == Presentation::FilePicker);
  default:
    return BigPicture::GetControllerPrompts(m_navigation, text, home, pause);
  }
}

ImTextureID OnScreenUI::GetNavigationTexture(const ::BigPicture::Snapshot& snapshot,
                                             const std::string& path)
{
  if (path.starts_with("achievement:"))
  {
    const auto found = m_achievement_images.find(path);
    if (found == m_achievement_images.end() || !found->second)
      return ImTextureID{};
    const auto& badge = found->second;
    if (!badge->width || !badge->height || badge->width > 512 || badge->height > 512 ||
        badge->rgba.size() != static_cast<std::size_t>(badge->width) * badge->height * 4)
      return ImTextureID{};
    if (!m_achievement_textures.contains(path) && m_achievement_textures.size() >= 64)
    {
      auto oldest = std::ranges::min_element(
          m_achievement_textures, {}, [](const auto& entry) { return entry.second.last_frame; });
      if (oldest->second.last_frame == m_cover_frame)
        return ImTextureID{};
      m_achievement_textures.erase(oldest);
    }
    auto& cached = m_achievement_textures[path];
    if (cached.image != badge)
    {
      auto texture = UploadMenuImage(*badge, "Achievement badge");
      if (!texture)
        return ImTextureID{};
      cached = {badge, std::move(texture)};
    }
    cached.last_frame = m_cover_frame;
    return reinterpret_cast<ImTextureID>(cached.texture.get());
  }
  if (path.starts_with("state:"))
  {
    std::size_t index;
    const auto [end, error] = std::from_chars(path.data() + 6, path.data() + path.size(), index);
    if (error != std::errc{} || end != path.data() + path.size() ||
        index >= m_state_textures.size())
      return ImTextureID{};
    const auto& preview = snapshot.host.state_slots[index].preview;
    auto& cached = m_state_textures[index];
    if (!preview || !preview->width || !preview->height || preview->width > 320 ||
        preview->height > 240 || preview->rgba.size() != preview->width * preview->height * 4)
      return ImTextureID{};
    if (cached.image != preview)
    {
      auto texture = UploadMenuImage(*preview, "State preview");
      if (!texture)
        return ImTextureID{};
      cached = {preview, std::move(texture)};
    }
    return reinterpret_cast<ImTextureID>(cached.texture.get());
  }
  if (path.starts_with("home:"))
  {
    const auto upload = [&](const ::BigPicture::LibraryImage& image) {
      TextureConfig config;
      config.width = image.width;
      config.height = image.height;
      const bool menu_icon = path != "home:avatar" && path != "home:logo";
      if (menu_icon)
        config.levels = std::bit_width(std::max(image.width, image.height));
      auto texture = g_gfx->CreateTexture(config, "Big Picture home");
      if (texture)
      {
        auto pixels = image.rgba;
        // The monochrome SVGs have white coverage. Keep transparent texels white
        // too, avoiding dark fringes when filtering straight-alpha artwork.
        if (menu_icon)
          for (std::size_t i = 0; i < pixels.size(); i += 4)
            pixels[i] = pixels[i + 1] = pixels[i + 2] = 255;
        u32 width = image.width, height = image.height;
        for (u32 level = 0; level < config.levels; ++level)
        {
          texture->Load(level, width, height, width, pixels.data(), pixels.size());
          if (level + 1 == config.levels)
            break;
          const u32 next_width = std::max(1u, width / 2);
          const u32 next_height = std::max(1u, height / 2);
          std::vector<u8> next(next_width * next_height * 4, 255);
          for (u32 y = 0; y < next_height; ++y)
            for (u32 x = 0; x < next_width; ++x)
            {
              unsigned alpha = 0;
              for (u32 dy = 0; dy < 2; ++dy)
                for (u32 dx = 0; dx < 2; ++dx)
                  alpha += pixels[(std::min(y * 2 + dy, height - 1) * width +
                                   std::min(x * 2 + dx, width - 1)) *
                                      4 +
                                  3];
              next[(y * next_width + x) * 4 + 3] = static_cast<u8>((alpha + 2) / 4);
            }
          pixels = std::move(next);
          width = next_width;
          height = next_height;
        }
      }
      return texture;
    };
    if (path == "home:avatar")
    {
      const auto& avatar = snapshot.host.achievements_avatar;
      if (!avatar)
        return ImTextureID{};
      if (!m_home_avatar || m_home_avatar->width != avatar->width ||
          m_home_avatar->height != avatar->height || m_home_avatar->rgba != avatar->rgba)
      {
        m_home_textures[path] = upload(*avatar);
        m_home_avatar = avatar;
      }
    }
    else if (!m_home_textures.contains(path))
    {
      std::string filename;
      if (path == "home:logo")
        filename = "dolphin_logo.png";
      else if (path == "home:media-cdrom" || path == "home:start-game" ||
               path == "home:applications-system" || path == "home:exit" ||
               path == "home:start-file" || path == "home:drive-cdrom" || path == "home:start-bios")
        filename = "BigPicture/" + path.substr(5) + ".png";
      else
        return ImTextureID{};
      std::string bytes;
      Common::UniqueBuffer<u8> pixels;
      u32 width = 0, height = 0;
      auto& texture = m_home_textures[path];
      if (File::ReadFileToString(File::GetSysDirectory() + RESOURCES_DIR "/" + filename, bytes) &&
          Common::LoadPNG(std::span(reinterpret_cast<const u8*>(bytes.data()), bytes.size()),
                          &pixels, &width, &height) &&
          width && height && width <= 1024 && height <= 1024)
      {
        ::BigPicture::LibraryImage image;
        image.width = width;
        image.height = height;
        image.rgba.assign(pixels.data(), pixels.data() + width * height * 4);
        texture = upload(image);
      }
    }
    return reinterpret_cast<ImTextureID>(m_home_textures[path].get());
  }
  return snapshot.host.library ? GetLibraryCover(*snapshot.host.library, path) : ImTextureID{};
}

ImTextureID OnScreenUI::GetLibraryCover(const ::BigPicture::LibrarySnapshot& library,
                                        const std::string& path)
{
  if (!m_cover_textures.contains(path) &&
      m_cover_textures.size() >= ::BigPicture::MAX_DECODED_COVERS)
  {
    auto oldest = std::ranges::min_element(m_cover_textures, {},
                                           [](const auto& item) { return item.second.last_frame; });
    // Draw data retains texture pointers until DrawImGui; never evict a texture used this frame.
    if (oldest->second.last_frame == m_cover_frame)
      return {};
    m_cover_textures.erase(oldest);
  }
  auto& cached = m_cover_textures[path];
  cached.last_frame = m_cover_frame;
  const u64 now = Common::Timer::NowUs();
  if (!library.missing_covers.contains(path) &&
      (cached.last_request_us == 0 || now - cached.last_request_us >= 1000000))
  {
    const auto snapshot = ::BigPicture::GetPresentationState().GetSnapshot();
    ::BigPicture::NavigationRequest request{snapshot->generation, ::BigPicture::Action::Library};
    request.library_operation = ::BigPicture::LibraryOperation::Cover;
    request.library_revision = library.catalog_revision;
    request.path = path;
    ::BigPicture::GetPresentationState().RequestNavigation(std::move(request), m_input_epoch);
    cached.last_request_us = now;
  }
  const auto it = library.covers.find(path);
  if (!cached.texture && m_cover_uploads < 2 && it != library.covers.end())
  {
    const auto& image = *it->second;
    if (image.width && image.height && image.width <= ::BigPicture::MAX_COVER_WIDTH &&
        image.height <= ::BigPicture::MAX_COVER_HEIGHT &&
        image.rgba.size() == static_cast<std::size_t>(image.width) * image.height * 4)
    {
      ++m_cover_uploads;
      cached.texture = UploadMenuImage(image, "Big Picture cover");
    }
  }
  return reinterpret_cast<ImTextureID>(cached.texture.get());
}

void OnScreenUI::SetKey(u32 key, bool is_down, const char* chars)
{
  auto& state = ::BigPicture::GetPresentationState();
  const auto snapshot = state.GetSnapshot();
  state.Enqueue({::BigPicture::InputEvent::Type::Key, snapshot ? snapshot->generation : 0, key,
                 is_down, chars ? chars : ""});
}

void OnScreenUI::SetMousePos(float x, float y)
{
  auto& state = ::BigPicture::GetPresentationState();
  const auto snapshot = state.GetSnapshot();
  // Positions are fractions of the surface; this API takes backbuffer pixels.
  state.Enqueue({::BigPicture::InputEvent::Type::MousePosition,
                 snapshot ? snapshot->generation : 0,
                 0,
                 false,
                 {},
                 x / static_cast<float>(std::max(1u, m_backbuffer_width)),
                 y / static_cast<float>(std::max(1u, m_backbuffer_height))});
}

void OnScreenUI::SetMousePress(u32 button_mask)
{
  auto& state = ::BigPicture::GetPresentationState();
  const auto snapshot = state.GetSnapshot();
  state.Enqueue({::BigPicture::InputEvent::Type::MouseButtons,
                 snapshot ? snapshot->generation : 0,
                 0,
                 false,
                 {},
                 0,
                 0,
                 button_mask});
}

}  // namespace VideoCommon
