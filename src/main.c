#include <SDL3/SDL.h>

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

#include <dcimgui.h>
#include <dcimgui_impl_sdl3.h>
#include <dcimgui_impl_sdlgpu3.h>

#include "theme.h"

#define WINDOW_TITLE "Application"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

float _target_delta_ms = 16.6667f; // Default to 60Hz

SDL_GPUDevice *_gpu_device       = NULL;
SDL_Window *_window              = NULL;
SDL_GPUPresentMode _present_mode = SDL_GPU_PRESENTMODE_VSYNC;
float _scale_factor              = 1.0f;

ImGuiContext *_imgui_context = NULL;

static void
show_info(int fps, int ups)
{
  ImGuiIO *io = ImGui_GetIO();

  ImGui_SetNextWindowPos((ImVec2){ 0, 0 }, ImGuiCond_Always);
  ImGui_SetNextWindowSize((ImVec2){ io->DisplaySize.x, io->DisplaySize.y },
                          ImGuiCond_Always);

  ImGuiWindowFlags flags
      = ImGuiWindowFlags_NoTitleBar |            // no title
        ImGuiWindowFlags_NoResize |              // no sizing
        ImGuiWindowFlags_NoMove |                // no moving
        ImGuiWindowFlags_NoScrollbar |           // no scrollbars
        ImGuiWindowFlags_NoBringToFrontOnFocus | // no bring to front
        ImGuiWindowFlags_NoBackground;           // transparent bg (if you like)

  ImGui_Begin("Info", NULL, flags);
  {
    ImGui_Text("FPS: %d", fps);
    ImGui_Text("UPS: %d", ups);
  }
  ImGui_End();
}

static void
update_refresh_rate_(void)
{
  SDL_DisplayID display_id = SDL_GetDisplayForWindow(_window);
  if (display_id == 0) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Couldn't get display for window: %s",
                SDL_GetError());
  }

  const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display_id);
  if (mode == NULL) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Couldn't get display mode: %s",
                SDL_GetError());
  }

  _target_delta_ms = 1000.0 / (double)mode->refresh_rate;

  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
              "Updated display refresh rate: %.2f Hz (delta: %2.f ms)",
              mode->refresh_rate,
              _target_delta_ms);
}

SDL_AppResult
SDL_AppInit(void **appstate, int argc, char **argv)
{
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to initialise SDL: %s",
                 SDL_GetError());
    SDL_Quit();
  }

  _scale_factor = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

  // Create a window

  _window = SDL_CreateWindow(WINDOW_TITLE,
                             WINDOW_WIDTH * _scale_factor,
                             WINDOW_HEIGHT * _scale_factor,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
                                 | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (_window == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to create window: %s.",
                 SDL_GetError());
    SDL_Quit();
  }
  SDL_SetWindowPosition(
      _window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(_window);

  // Create a GPU device

  _gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV
                                        | SDL_GPU_SHADERFORMAT_DXIL
                                        | SDL_GPU_SHADERFORMAT_METALLIB,
                                    true,
                                    NULL);
  if (_gpu_device == NULL) {
    SDL_LogError(
        SDL_LOG_CATEGORY_GPU, "Failed to create device: %s", SDL_GetError());
    SDL_Quit();
  }

  // Claime the window for the GPU device

  if (!SDL_ClaimWindowForGPUDevice(_gpu_device, _window)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_GPU, "Failed to claim window:  %s", SDL_GetError());
    SDL_Quit();
  }

  // Set swapchain parameters

  if (SDL_WindowSupportsGPUPresentMode(
          _gpu_device, _window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
    _present_mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
  } else if (SDL_WindowSupportsGPUPresentMode(
                 _gpu_device, _window, SDL_GPU_PRESENTMODE_MAILBOX)) {
    _present_mode = SDL_GPU_PRESENTMODE_MAILBOX;
  }

  SDL_SetGPUSwapchainParameters(
      _gpu_device, _window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, _present_mode);

  // Setup ImGui
  CIMGUI_CHECKVERSION();
  _imgui_context = ImGui_CreateContext(NULL);
  ImGuiIO *io    = ImGui_GetIO();

  io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui_StyleColorsDark(NULL);
  ImGuiStyle *style = ImGui_GetStyle();
  ImGuiStyle_ScaleAllSizes(style, _scale_factor);
  style->FontScaleDpi = _scale_factor;

  cImGui_ImplSDL3_InitForSDLGPU(_window);

  ImGui_ImplSDLGPU3_InitInfo info
      = { .Device               = _gpu_device,
          .ColorTargetFormat    = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
          .MSAASamples          = SDL_GPU_SAMPLECOUNT_1,
          .SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
          .PresentMode          = _present_mode };
  cImGui_ImplSDLGPU3_Init(&info);

  // font_setup();
  theme_setup();

  return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppIterate(void *appstate)
{
  static Uint64 fps            = 0;
  static Uint64 ups            = 0;
  static Uint64 frame_count    = 0;
  static Uint64 update_count   = 0;
  static double lag_ms         = 0;
  static double last_ticks     = 0;
  static Uint64 accumulator_ms = 0; // Track time for FPS counter
  static bool first_frame      = true;

  if (first_frame) {
    last_ticks  = SDL_GetTicks();
    first_frame = false;
    return SDL_APP_CONTINUE;
  }

  Uint64 current_ticks = SDL_GetTicks();
  double elapsed_ms    = (double)(current_ticks - last_ticks);

  last_ticks = current_ticks;

  lag_ms += elapsed_ms;
  accumulator_ms += elapsed_ms;

  double delta_ms = _target_delta_ms;

  // Cap lag to avoid spiral of death (max  5 frames behind)
  double max_lag_ms = delta_ms * 5.0;

  if (lag_ms > max_lag_ms) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "High lag detected: %.2f ms, capping to %.2f ms",
                lag_ms,
                max_lag_ms);
    lag_ms = max_lag_ms;
  }

  while (lag_ms >= delta_ms) {
    {
      // --- Update here ---
      // `delta_ms` can be used for physics calculations if needed
    }

    lag_ms -= delta_ms;
    ++update_count;
  }

  double alpha_ms = (double)(lag_ms / delta_ms);

  cImGui_ImplSDLGPU3_NewFrame();
  cImGui_ImplSDL3_NewFrame();
  ImGui_NewFrame();

  {
    // --- Render here ---
    // `alpha_ms` can be used for rendering interpolation if needed
    show_info(fps, ups);

    static bool show_demo_window = true;
    ImGui_ShowDemoWindow(&show_demo_window);
  }

  ImGui_Render();

  ImDrawData *draw_data = ImGui_GetDrawData();
  const bool is_minimized
      = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer *cmd_buffer = SDL_AcquireGPUCommandBuffer(_gpu_device);
  if (cmd_buffer == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "AcquireGPUCommandBuffer failed: %s",
                 SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture *swapchain_texture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
          cmd_buffer, _window, &swapchain_texture, NULL, NULL)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "WaitAndAcquireGPUSwapchainTexture failed: %s",
                 SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (swapchain_texture != NULL && !is_minimized) {
    // upload vertex and index buffer
    cImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buffer);

    SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
        cmd_buffer,
        &(SDL_GPUColorTargetInfo){ .texture     = swapchain_texture,
                                   .cycle       = false,
                                   .load_op     = SDL_GPU_LOADOP_CLEAR,
                                   .store_op    = SDL_GPU_STOREOP_STORE,
                                   .clear_color = { 0, 0, 0, 1 } },
        1,
        NULL);

    cImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd_buffer, render_pass);

    SDL_EndGPURenderPass(render_pass);
  }

  SDL_SubmitGPUCommandBuffer(cmd_buffer);

  ++frame_count;

  // Calculate FPS and UPS every second
  if (accumulator_ms >= 1000) {
    fps = frame_count;
    ups = update_count;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "FPS: %lu, UPS: %lu", fps, ups);

    frame_count  = 0;
    update_count = 0;
    accumulator_ms -= 1000;
  }

  return SDL_APP_CONTINUE;

  return SDL_APP_CONTINUE;
}

SDL_AppResult
SDL_AppEvent(void *appstate, SDL_Event *event)
{
  cImGui_ImplSDL3_ProcessEvent(event);

  switch (event->type) {
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
    update_refresh_rate_();
    break;
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  default:
    break;
  }

  return SDL_APP_CONTINUE;
}

void
SDL_AppQuit(void *appstate, SDL_AppResult result)
{
  SDL_WaitForGPUIdle(_gpu_device);

  cImGui_ImplSDL3_Shutdown();
  cImGui_ImplSDLGPU3_Shutdown();

  ImGui_DestroyContext(_imgui_context);

  SDL_ReleaseWindowFromGPUDevice(_gpu_device, _window);
  SDL_DestroyWindow(_window);
  SDL_DestroyGPUDevice(_gpu_device);
}
