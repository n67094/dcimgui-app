# How to update dcimgui and dear imgui

**dcimgui**

Sources files where generate by [dcimgui](https://github.com/dearimgui/dear_bindings).

This extra command is needed to generate the SDL_GPU3 backend bindings:

```
python dear_bindings.py --backend --include ../imgui/imgui.h -o dcimgui_impl_sdlgpu3 ../imgui/backends/imgui_impl_sdlgpu3.h
```

**Dear ImGui**

Sources files where taken from [dear imgui](https://github.com/ocornut/imgui) don't forget the sdl3 gpu backend sources files.
