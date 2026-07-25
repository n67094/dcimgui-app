# DCImGui

Update sources:

`dcimgui` files sources where generate by [dcimgui](https://github.com/dearimgui/dear_bindings).

`imgui` files sources where taken from [dear imgui](https://github.com/ocornut/imgui) don't forget the sdl3 gpu backend sources files.

**NOTE:** "imgui_impl_sdlgpu3.h/.cpp" files where generate using the following commands (make sure to clone imgui next to dear_bindings).

```
python dear_bindings.py --backend --include ../imgui/imgui.h -o dcimgui_impl_sdlgpu3 ../imgui/backends/imgui_impl_sdlgpu3.h 
```

