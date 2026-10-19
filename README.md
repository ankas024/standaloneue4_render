# Render Studio

A standalone look-dev / rendering application built on **Unreal Engine 4.27**: the UE4
renderer and the UE4 node-based material editor, inside a simple early-2010s desktop
3D tool UI. It follows Marmoset Toolbag's workflow (drop in a mesh, light it with an
HDRI, tweak materials, take a beauty shot), but has **no baking features**.

> Status: early skeleton (v0.1). The code targets the UE 4.27 API, but it has not been
> compiled or run yet. Expect a round of compile fixes on the first build.

```
+----------------------------------------------------------------------------------+
| File  Edit  Create  View  Material  Window  Help                                 |
| [New][Import][Save][Shot] | [Sel][Move][Rot][Scale] | [Cube][Sphere][Light] |... |
+-------------+------------------------------------------------+-------------------+
| SCENE  +Add |                                                | MATERIALS         |
|  Shader Ball|                                                |  (o) MI_Material  |
|  Cube       |              3D VIEWPORT                       |  (o) M_Material   |
|  Point Light|   (HDRI sky, floor, sun, post FX,              | [New][Inst][Assign]|
+-------------+    transform gizmo, UE4 viewport toolbar)      +-------------------+
| PROPERTIES  |                                                | PARAMETERS        |
|  Transform  |                                                |  ALBEDO  [tex][##]|
|  Static Mesh|                                                |  Roughness  ===o==|
|  Materials  |                                                |  Metalness  o=====|
+-------------+------------------------------------------------+-------------------+
| Ready                                Objects: 3   Selected: Cube   UE 4.27 / D3D11 |
+----------------------------------------------------------------------------------+
```

## Features (v0.1)

| Area | What you get |
|---|---|
| **UI shell** | Custom Slate skin: grey gradients, glossy buttons, orange selection. Menu bar, main toolbar, dockable/re-arrangeable panels (layout is saved), status bar. |
| **Viewport** | UE4 deferred renderer through `FAdvancedPreviewScene`: HDRI sky + sky light, sun, floor shadow catcher, reflection capture, post processing. Standard UE4 viewport toolbar (view modes like Lit/Unlit/Wireframe/Buffer visualization, show flags, exposure, FOV). |
| **Navigation** | Look-dev style without modifier keys: LMB orbit, MMB pan, RMB/wheel zoom, `F` frame selection, `Home` frame scene. `L`+drag moves the sun, `K`+drag spins the sky. |
| **Scene** | Add primitives (cube, sphere, cylinder, cone, plane) and point, spot and area lights. Import FBX/OBJ meshes, which keep their layout and are dropped onto the floor. Select, duplicate, delete, rename, hide. Move/rotate/scale gizmo (`W`/`E`/`R`, `Q` hides it). Selection outline. |
| **Properties** | UE4 details panel for the selected object (transform, mesh, materials, light settings), with game-only categories (physics, collision, replication, ...) hidden. |
| **Materials** | Material library with live thumbnails. **New** creates a PBR material (albedo map × colour, metalness, specular, roughness, normal map, emissive) **plus an instance** that gets assigned to the selection. **Parameters** panel: sliders, colour swatches and texture slots, with live updates on instances. **Edit Graph / Node Editor** opens the real UE4 material editor on the base material. |
| **Environment & Render** | The asset-viewer profile system: HDRI cubemap, sky/sun intensity and colour, sky rotation, floor/sky toggles, and the full UE4 post-process settings (bloom, exposure, tonemapper, AO, SSR, DOF, vignette, colour grading...). Also app settings for camera FOV, grid, screenshot resolution and folders. Profiles can be saved and switched. |
| **Turntable** | `T` slowly rotates the sky and the sun around the subject. |
| **Output** | `F12` saves a PNG of the viewport, with optional 2–4× supersampling (screenshot multiplier setting). |
| **Saving** | `Ctrl+S` saves the materials, instances and imported meshes (stored in `/Game/RenderStudio`). |

Out of scope by design: normal/AO/curvature baking, texture projection painting.
Not built yet: scene save/load, turntable video export, multiple cameras (see roadmap).

## How it is built

Render Studio is a **UE4 project with an editor plugin**, not a fork of the engine. It runs on
top of an unmodified UE 4.27 source build, such as
[AlexMercer-MA/UnrealEngine-4.27](https://github.com/AlexMercer-MA/UnrealEngine-4.27). It uses
the editor framework for the parts it reuses (renderer, asset pipeline, material editor,
details panels) and replaces the level editor UI with its own window.

```
RenderStudio.uproject
Config/                         look-dev friendly renderer defaults, fast startup map
Source/RenderStudioGame/        minimal primary module (required by UBT, intentionally empty)
Plugins/RenderStudio/
  Resources/Style/              bitmap skin (generated by Scripts/generate_style_images.py)
  Source/RenderStudio/
    RenderStudioModule          registers the app tab, -RenderStudio application mode
    RenderStudioStyle           the 2014-style Slate skin
    RenderStudioCommands        all actions + keyboard shortcuts
    RenderStudioScene           document: preview world, objects, selection, import, materials
    RenderStudioSettings        per-user app settings (FOV, grid, screenshots, content folder)
    RenderStudioViewportClient  camera, picking, gizmo, screenshots
    SRenderStudioViewport       viewport widget + UE4 viewport toolbar
    SRenderStudioMainFrame      menus, toolbar, dock layout, status bar, command handlers
    SRenderStudioOutliner       scene list
    SRenderStudioMaterialPanel  material library + live parameter editor
    SRenderStudioPanel          shared panel chrome / glossy button helper
Scripts/                        build + launch helpers (Windows .bat, Linux .sh)
```

## Building

1. Build UE 4.27 from source (one time):
   ```bat
   git clone https://github.com/AlexMercer-MA/UnrealEngine-4.27 C:\UnrealEngine-4.27
   cd C:\UnrealEngine-4.27
   Setup.bat
   GenerateProjectFiles.bat
   :: open UE4.sln and build "UE4" (Development Editor, Win64)
   ```
2. Build Render Studio:
   ```bat
   Scripts\Build.bat C:\UnrealEngine-4.27
   ```
   You can also right-click `RenderStudio.uproject`, choose *Switch Unreal Engine version*,
   pick your source build, then *Generate Visual Studio project files* and build
   `RenderStudioEditor` from the IDE.
3. Run:
   ```bat
   Scripts\RenderStudio.bat C:\UnrealEngine-4.27
   ```

Linux works the same way with `Scripts/build.sh` and `Scripts/renderstudio.sh`.

The first launch compiles the engine's shaders, which takes a while. Later launches are fast.

## Running modes

* **Application mode:** `UE4Editor RenderStudio.uproject -RenderStudio`. The level editor
  window is hidden, Render Studio opens as its own window, and closing it quits. Material
  editor windows open as separate windows.
* **Inside the editor:** open the project normally, then use *Window → Render Studio*.
  This mode is handy while developing the plugin itself.

## Keyboard reference

| Key | Action |
|---|---|
| `Ctrl+N` / `Ctrl+I` / `Ctrl+S` | New scene / import mesh / save all |
| `F12` | Save screenshot |
| `Q` `W` `E` `R` | Select / move / rotate / scale |
| `F` / `Home` | Frame selection / frame scene |
| `Ctrl+D` / `Del` | Duplicate / delete |
| `G` / `T` | Toggle grid / turntable |
| `Ctrl+M` / `Ctrl+E` / `Ctrl+A` | New material / open node editor / assign material |
| `L` / `K` + drag | Rotate sun / sky |

## Changing the skin

The look comes from a few small PNGs plus the colours in `RenderStudioStyle.cpp`. Edit
`Scripts/generate_style_images.py`, run it, and restart:

```
python Scripts/generate_style_images.py
```

## Roadmap

1. Scene documents: save/load the object list, transforms, material assignments and environment profile.
2. Camera presets, safe frames, aspect-ratio locked renders, and a "render" button with high sample counts.
3. Turntable / camera-path video export (via the movie scene capture pipeline).
4. A sky panel with a list of bundled HDRIs, plus light-from-HDRI extraction.
5. Material presets (metal, plastic, skin, glass...) and drag-and-drop of materials onto the viewport.
6. A trimmed standalone `Program` target (no level editor modules) once the feature set settles.
