# Render Studio

A standalone look-dev / rendering application built on **Unreal Engine 4.27**: the UE4
renderer and the UE4 node-based material editor, inside a simple early-2010s desktop 3D tool
UI. It follows Marmoset Toolbag's workflow (drop in a mesh, light it with an HDRI, tweak
materials, render a beauty shot or a turntable), but has **no baking features**.

> **Status (v0.2):** all of the application's C++ compiles against the UE 4.27 engine headers
> with Epic's clang 11 Linux toolchain, through UnrealBuildTool and UnrealHeaderTool, with zero
> errors and zero warnings. Both build targets have been checked. The application has **not
> been run yet**: that needs a full engine build and a GPU. Report anything that misbehaves on
> first launch.

```
+------------------------------------------------------------------------------------+
| File  Edit  Create  View  Environment  Material  Window  Help                      |
| [New][Open][Save][Import] [Undo][Redo] [Q][W][E][R] [Cube][Ball][Light] [Render]   |
+--------------+---------------------------------------------+-----------------------+
| SCENE  +Add  |                                             | MATERIALS             |
|  Shader Ball |                                             |  (o) MI_Gold          |
|  Cube        |            3D VIEWPORT                      |  (o) MI_Plastic       |
|  Point Light |   (HDRI sky, floor, sun, post FX,           | [New][Inst][Assign]   |
+--------------+    gizmo, UE4 viewport toolbar)             | [Preset v] [Edit...]  |
| PROPERTIES   |                                             | PARAMETERS            |
|  Transform   |                                             |  ALBEDO   [tex][###]  |
|  Static Mesh |                                             |  Roughness  ===o===   |
|  Materials   |                                             |  Metalness  o======   |
+--------------+---------------------------------------------+-----------------------+
| Saved Watch.rss                      Objects: 3   Selected: Cube   UE 4.27 / D3D11 |
+------------------------------------------------------------------------------------+
```

## Features

| Area | What you get |
|---|---|
| **Application** | `RenderStudio.exe` with its own icon, splash screen and window titled after the open scene. The level editor never appears. A packaging script produces a self-contained folder that runs without an Unreal install. |
| **Scenes** | New / Open / Save / Save As, recent scenes, and an unsaved-changes prompt (`*` in the title). Scenes are `.rss` JSON files that store objects, transforms, every property edited in the Properties panel, material assignments, the environment (HDRI, sun, sky, post FX), the camera and four camera bookmarks. |
| **Undo / redo** | `Ctrl+Z` / `Ctrl+Y` for add, delete, duplicate, rename, hide, gizmo moves, Properties panel edits and material assignments. |
| **Viewport** | UE4 deferred renderer through `FAdvancedPreviewScene`: HDRI sky + sky light, sun, floor shadow catcher, reflections, post-processing. Standard UE4 viewport toolbar (Lit/Unlit/Wireframe/buffer visualisation, show flags, exposure, FOV). Selection outline, move/rotate/scale gizmo. |
| **Camera** | Look-dev navigation without modifier keys: LMB orbit, MMB pan, RMB/wheel zoom. Front/back/left/right/top/perspective views (numpad). Four bookmarks (`Ctrl+1..4` store, `1..4` recall), saved with the scene. |
| **Scene objects** | Primitives (cube, sphere, cylinder, cone, plane) and point, spot and area lights. FBX / OBJ import, through the menu or by dropping files on the viewport. Rename, hide, duplicate, delete. |
| **Materials** | Library with live thumbnails. **New Material** creates an instance of the shared `M_StudioPBR` material (albedo map × colour, metalness, specular, roughness, normal map, emissive). **New Material Graph** makes one with its own node graph. 12 presets (plastics, rubber, clay, ceramic, car paint, chrome, gold, copper, silver, aluminium, iron). Parameter panel: sliders, colour pickers and texture slots, live on instances. **Node Editor** opens the real UE4 material editor. Drag a material onto an object in the viewport to assign it. |
| **Environment** | HDRI menu: the engine's sky cubemaps plus your imported ones. **Import HDRI** (or drop a `.hdr` on the viewport) turns an equirectangular panorama into the sky and image-based lighting. Full profile panel: sun and sky intensity and colour, rotation, floor, post FX. `L`/`K` + drag rotate the sun and sky. `T` turntable spins the lighting. |
| **Rendering** | **Render (F9):** offscreen render of the current camera at any size up to 8K, with presets (720p…4K, square, portrait, A4), 1–4× supersampling, 1–64 accumulated anti-aliasing samples, PNG/JPEG/BMP. Grid, gizmo and selection outline are left out. **Render Turntable** orbits the camera once around the scene and writes a numbered image sequence. **Screenshot (F12)** saves the viewport as it is. |

Out of scope by design: normal/AO/curvature baking, texture projection painting.

## How it is built

Render Studio is a UE4 project plus an editor plugin, compiled into its own executable. It does
not modify the engine.

The real UE4 material editor only exists in editor builds: the node graph, shader compiling for
new materials, and FBX/HDR import are all editor-only. So `RenderStudio.exe` is built as an
editor-type target with a **unique build environment**. That gives it a private copy of every
engine module (`RenderStudio-*.dll` next to the exe). At startup the plugin keeps the level
editor window hidden and opens Render Studio's own window instead.

```
RenderStudio.uproject
Build/Windows/Application.ico       exe icon         } generated by
Content/Splash/EdSplash.bmp         splash screen    } Scripts/generate_branding.py
Config/                             renderer defaults, no autosave, no background throttling
Source/
  RenderStudio.Target.cs            the standalone application (RenderStudio.exe)
  RenderStudioEditor.Target.cs      development target for UE4Editor
  RenderStudioGame/                 minimal primary module (required by UBT)
Plugins/RenderStudio/
  Resources/Style/                  bitmap skin (Scripts/generate_style_images.py)
  Source/RenderStudio/
    RenderStudioModule              application mode: hidden level editor, own window, quit on close
    RenderStudioScene               document: preview world, objects, selection, undo, materials, HDRIs
    RenderStudioSceneIO             .rss save / load and undo snapshots
    RenderStudioViewportClient      camera, picking, gizmo, offscreen final renders
    SRenderStudioMainFrame          menus, toolbar, dock layout, status bar, document workflow
    SRenderStudioRenderDialog       render settings, still and turntable output
    SRenderStudioViewport           viewport widget, drag & drop
    SRenderStudioOutliner           scene list
    SRenderStudioMaterialPanel      material library, presets, live parameter editor
    RenderStudioStyle / Commands / Settings / SRenderStudioPanel
Scripts/
  Package.bat / package.sh / package.py   build + package the standalone application
  Build.bat / build.sh, RenderStudio.bat / renderstudio.sh   development build / launch in UE4Editor
```

## Building the application (Windows)

1. Get UE 4.27 source and run its setup (one time):
   ```bat
   git clone https://github.com/AlexMercer-MA/UnrealEngine-4.27 C:\UnrealEngine-4.27
   cd C:\UnrealEngine-4.27
   Setup.bat
   ```
   Visual Studio 2019 with the "Game development with C++" workload is required.
   You do **not** need to build UE4Editor itself.
2. Build and package:
   ```bat
   Scripts\Package.bat C:\UnrealEngine-4.27
   ```
   This builds `RenderStudio.exe`, `ShaderCompileWorker` and Epic's small launcher, then
   assembles `Dist\RenderStudio\`. The first build compiles the engine for the application,
   which takes as long as a full editor build. Later builds are incremental. Add
   `--skip-build` to repackage only, or `--symbols` to include `.pdb` files.
3. Run `Dist\RenderStudio\RenderStudio.exe`. Copy the folder anywhere writable, e.g.
   `Documents` or another drive. Avoid `Program Files`: new materials, imported meshes and
   HDRIs are saved inside `RenderStudio\Content`. The first launch compiles the engine's shaders,
   which takes a few minutes, once.

Renders and scenes default to `Documents\Render Studio\`. Settings and logs go to
`%LOCALAPPDATA%\UnrealEngine\`.

How the package starts: `RenderStudio.exe` at the top is Epic's `BootstrapPackagedGame` launcher,
stamped with our icon. It installs the VC++ runtime if missing and then starts
`RenderStudio\Binaries\Win64\RenderStudio.exe`. `Engine\Build\InstalledProjectBuild.txt` tells
that executable which project to open, so no command line is needed. To open a scene directly:
`RenderStudio.exe -Scene="D:\Scenes\Watch.rss"`.

Linux works the same way with `Scripts/package.sh` (it produces `RenderStudio.sh` instead of
the launcher).

## Developing the plugin

Building the full application for every change is slow. For plugin work, build the
development target against a normal UE4Editor build:

```bat
Scripts\Build.bat C:\UnrealEngine-4.27          :: RenderStudioEditor target (needs UE4Editor built)
Scripts\RenderStudio.bat C:\UnrealEngine-4.27   :: UE4Editor ... -RenderStudio  (application mode)
```

Or open the project in the editor and use *Window → Render Studio*.

## Keyboard reference

| Key | Action |
|---|---|
| `Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | New / open / save / save as |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+I` / `Ctrl+Shift+I` | Import mesh / import HDRI |
| `F9` / `F12` | Render dialog / save viewport screenshot |
| `Q` `W` `E` `R` | Select / move / rotate / scale |
| `F` / `Home` | Frame selection / frame scene |
| Numpad `0` `1` `3` `7` | Perspective / front / right / top (`Ctrl` + `1`/`3`: back / left) |
| `Ctrl+1..4` / `1..4` | Store / recall camera bookmark |
| `Ctrl+D` / `Del` | Duplicate / delete |
| `G` / `T` | Grid / turntable lighting |
| `Ctrl+M` / `Ctrl+Shift+M` / `Ctrl+E` / `Ctrl+A` | New material / new material graph / node editor / assign |
| `L` / `K` + drag | Rotate sun / sky |

Drag materials from the Materials panel onto objects. Drop `.fbx` / `.obj` / `.hdr` files on
the viewport.

## Changing the look

Panel bitmaps and colours: edit `Scripts/generate_style_images.py` and the colours in
`RenderStudioStyle.cpp`. Icon and splash: edit `Scripts/generate_branding.py`. Re-run the
script, then rebuild.

## Known limitations

* Not run yet. Compile-checked only (see Status above).
* Undo covers the scene, not material parameter edits. Modified materials are saved together
  with the scene. When quitting, *Don't Save* discards them.
* Turntables are image sequences. Combine them into a video with any editor or `ffmpeg`.
* Renders have an opaque background. Transparent PNGs need the engine's alpha-propagation
  setting and are not exposed yet.
* `.exr` panoramas import as 2D textures, not cubemaps. Use `.hdr` for HDRIs.

## Roadmap

1. First run on Windows and fixes from it.
2. Multiple named cameras with safe frames / aspect lock in the viewport.
3. Turntable video export through the movie scene capture pipeline.
4. Transparent background renders, render passes (AO, normals, IDs).
5. A trimmed module set (drop editor modules the application never loads) to shrink the package.
