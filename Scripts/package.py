"""Builds and packages Render Studio as a self-contained application folder.

    python Scripts/package.py --engine C:/UnrealEngine-4.27 [--output Dist] [--skip-build] [--symbols]

Result (Windows):

    Dist/RenderStudio/
        RenderStudio.exe                 launcher (starts RenderStudio/Binaries/Win64/RenderStudio.exe,
                                         installs the VC++ runtime first if it is missing)
        Engine/                          the parts of UE 4.27 the application needs at run time
            Build/InstalledProjectBuild.txt   -> tells the executable which project to open
        RenderStudio/
            RenderStudio.uproject, Config/, Content/, Plugins/RenderStudio/
            Binaries/Win64/RenderStudio.exe + RenderStudio-*.dll

Copy the folder anywhere writable (materials and imported meshes are stored in
RenderStudio/Content) and run RenderStudio.exe. No Unreal installation is needed.

Uses only the Python standard library. The Package.bat / package.sh wrappers run it with the
engine's bundled Python, so nothing has to be installed.
"""
import argparse
import glob
import json
import os
import shutil
import struct
import subprocess
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT_NAME = "RenderStudio"
TARGET = "RenderStudio"

ALL_PLATFORMS = {"Win32", "Win64", "Windows", "Mac", "Linux", "LinuxAArch64", "Unix", "Android", "IOS", "TVOS",
                 "HTML5", "Lumin", "HoloLens", "PS4", "XboxOne", "Switch"}
# Folder names that belong to a platform (e.g. DirectX lives in ThirdParty/Windows/DirectX)
PLATFORM_FOLDERS = {"Win64": {"Win64", "Windows"}, "Linux": {"Linux", "Unix"}}


def log(message):
    print("[package] " + message, flush=True)


# ---------------------------------------------------------------------------------------------
# Build


def build(engine_root, platform, configuration):
    """Builds the application and the shader compiler worker it launches at run time."""
    if platform == "Win64":
        build_script = [os.path.join(engine_root, "Engine", "Build", "BatchFiles", "Build.bat")]
    else:
        build_script = ["bash", os.path.join(engine_root, "Engine", "Build", "BatchFiles", "Linux", "Build.sh")]

    uproject = os.path.join(PROJECT_ROOT, PROJECT_NAME + ".uproject")
    targets = [
        [TARGET, platform, configuration, "-Project=" + uproject],
        ["ShaderCompileWorker", platform, "Development"],
    ]
    if platform == "Win64":
        targets.append(["BootstrapPackagedGame", platform, "Shipping"])

    for target in targets:
        log("Building " + " ".join(target[:3]))
        result = subprocess.call(build_script + target + ["-WaitMutex"])
        if result != 0:
            sys.exit("Build of %s failed (exit code %d)" % (target[0], result))


# ---------------------------------------------------------------------------------------------
# Staging helpers


def is_other_platform(rel_path, platform):
    parts = rel_path.replace("\\", "/").split("/")
    return any(p in ALL_PLATFORMS and p not in PLATFORM_FOLDERS[platform] for p in parts)


def copy_tree(src, dst, platform, exclude_dirs=(), exclude_ext=(), keep=None):
    """Copies src to dst, skipping other platforms' folders, excluded folder names and extensions."""
    if not os.path.isdir(src):
        return 0
    count = 0
    for dirpath, dirnames, filenames in os.walk(src):
        rel_dir = os.path.relpath(dirpath, src)
        dirnames[:] = [d for d in dirnames
                       if d not in exclude_dirs and not is_other_platform(os.path.join(rel_dir, d), platform)]
        for name in filenames:
            rel = os.path.normpath(os.path.join(rel_dir, name))
            if os.path.splitext(name)[1].lower() in exclude_ext:
                continue
            if keep and not keep(rel):
                continue
            target = os.path.join(dst, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(os.path.join(dirpath, name), target)
            count += 1
    return count


def write_text(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        f.write(text)


# ---------------------------------------------------------------------------------------------
# Launcher (Windows): Epic's BootstrapPackagedGame, pointed at our executable


def read_ico(path):
    with open(path, "rb") as f:
        data = f.read()
    _, _, count = struct.unpack_from("<HHH", data, 0)
    icons = []
    for i in range(count):
        width, height, colors, _, planes, bits, size, offset = struct.unpack_from("<BBBBHHII", data, 6 + i * 16)
        icons.append((width, height, colors, planes, bits, data[offset:offset + size]))
    return icons


def stamp_launcher(launcher, relative_exe, arguments, icon_path):
    """Same resources UAT writes when it stages a packaged game's launcher."""
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.BeginUpdateResourceW.restype = wintypes.HANDLE
    kernel32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
    kernel32.UpdateResourceW.restype = wintypes.BOOL
    kernel32.UpdateResourceW.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wintypes.WORD, ctypes.c_void_p, wintypes.DWORD]
    kernel32.EndUpdateResourceW.restype = wintypes.BOOL
    kernel32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]

    RT_ICON, RT_RCDATA, RT_GROUP_ICON = 3, 10, 14
    LANGUAGE = 1033
    ICON_GROUP_ID, EXEC_FILE_ID, EXEC_ARGS_ID = 101, 201, 202

    handle = kernel32.BeginUpdateResourceW(launcher, False)
    if not handle:
        raise OSError(ctypes.get_last_error(), "BeginUpdateResource failed for " + launcher)

    buffers = []

    def update(res_type, res_id, payload):
        buf = ctypes.create_string_buffer(payload, len(payload))
        buffers.append(buf)
        if not kernel32.UpdateResourceW(handle, res_type, res_id, LANGUAGE, buf, len(payload)):
            raise OSError(ctypes.get_last_error(), "UpdateResource failed")

    update(RT_RCDATA, EXEC_FILE_ID, (relative_exe + "\0").encode("utf-16-le"))
    update(RT_RCDATA, EXEC_ARGS_ID, (arguments + "\0").encode("utf-16-le"))

    if icon_path and os.path.isfile(icon_path):
        icons = read_ico(icon_path)
        group = struct.pack("<HHH", 0, 1, len(icons))
        for index, (width, height, colors, planes, bits, data) in enumerate(icons):
            update(RT_ICON, index + 1, data)
            group += struct.pack("<BBBBHHIH", width, height, colors, 0, planes, bits, len(data), index + 1)
        update(RT_GROUP_ICON, ICON_GROUP_ID, group)

    if not kernel32.EndUpdateResourceW(handle, False):
        raise OSError(ctypes.get_last_error(), "EndUpdateResource failed")


# ---------------------------------------------------------------------------------------------
# Target receipts (written by UnrealBuildTool next to each executable)


def read_receipt(binaries_dir, platform, configuration, target=TARGET):
    name = target if configuration == "Development" else "%s-%s-%s" % (target, platform, configuration)
    path = os.path.join(binaries_dir, name + ".target")
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        return json.load(f)


def stage_runtime_dependencies(receipt, engine, out_engine, out_project, binary_ext):
    """Copies $(EngineDir)/... and $(ProjectDir)/... runtime dependencies. Returns the missing ones."""
    missing = []
    for dependency in receipt.get("RuntimeDependencies", []):
        path = dependency["Path"]
        if path.startswith("$(EngineDir)/"):
            source_root, target_root, rel = engine, out_engine, path[len("$(EngineDir)/"):]
        elif path.startswith("$(ProjectDir)/"):
            source_root, target_root, rel = PROJECT_ROOT, out_project, path[len("$(ProjectDir)/"):]
        else:
            continue
        if os.path.splitext(rel)[1].lower() in binary_ext:
            continue
        matches = glob.glob(os.path.join(source_root, rel))
        if not matches:
            missing.append(path)
        for match in matches:
            if os.path.isfile(match):
                target = os.path.join(target_root, os.path.relpath(match, source_root))
                os.makedirs(os.path.dirname(target), exist_ok=True)
                shutil.copy2(match, target)
    return missing


# ---------------------------------------------------------------------------------------------
# Packaging


def package(engine_root, output_root, platform, configuration, symbols):
    engine = os.path.join(engine_root, "Engine")
    out = os.path.join(output_root, PROJECT_NAME)
    out_engine = os.path.join(out, "Engine")
    out_project = os.path.join(out, PROJECT_NAME)
    exe_suffix = ".exe" if platform == "Win64" else ""
    binary_ext = (".pdb",) if not symbols else ()
    build_junk = (".lib", ".exp", ".ilk", ".target", ".version_tmp")

    project_binaries = os.path.join(PROJECT_ROOT, "Binaries", platform)
    exe_name = TARGET + ("" if configuration == "Development" else "-%s-%s" % (platform, configuration)) + exe_suffix
    if not os.path.isfile(os.path.join(project_binaries, exe_name)):
        sys.exit("%s not found in %s - build the %s target first (or drop --skip-build)." % (exe_name, project_binaries, TARGET))

    if os.path.isdir(out):
        log("Removing previous package " + out)
        shutil.rmtree(out)

    # Application binaries: RenderStudio.exe plus its private copy of every engine / plugin module
    log("Application binaries")
    copy_tree(project_binaries, os.path.join(out_project, "Binaries", platform), platform,
              exclude_ext=binary_ext + build_junk)

    # Project
    log("Project files")
    shutil.copy2(os.path.join(PROJECT_ROOT, PROJECT_NAME + ".uproject"), out_project)
    copy_tree(os.path.join(PROJECT_ROOT, "Config"), os.path.join(out_project, "Config"), platform)
    copy_tree(os.path.join(PROJECT_ROOT, "Content"), os.path.join(out_project, "Content"), platform)
    plugin_src = os.path.join(PROJECT_ROOT, "Plugins", "RenderStudio")
    plugin_dst = os.path.join(out_project, "Plugins", "RenderStudio")
    os.makedirs(plugin_dst, exist_ok=True)
    shutil.copy2(os.path.join(plugin_src, "RenderStudio.uplugin"), plugin_dst)
    for folder in ("Resources", "Content", "Config"):
        copy_tree(os.path.join(plugin_src, folder), os.path.join(plugin_dst, folder), platform)
    copy_tree(os.path.join(plugin_src, "Binaries", platform), os.path.join(plugin_dst, "Binaries", platform), platform,
              exclude_ext=binary_ext + build_junk)

    # Engine run-time files. Module binaries come from the application build; the engine
    # supplies config, content (editor UI, fonts, default materials, ICU data), shader sources,
    # third party DLLs, plugin descriptors / content and the shader compiler worker.
    log("Engine config, content and shaders (this is the big part)")
    copy_tree(os.path.join(engine, "Config"), os.path.join(out_engine, "Config"), platform)
    copy_tree(os.path.join(engine, "Content"), os.path.join(out_engine, "Content"), platform)
    copy_tree(os.path.join(engine, "Shaders"), os.path.join(out_engine, "Shaders"), platform)

    receipt = read_receipt(project_binaries, platform, configuration)
    not_shared_binaries = lambda rel: not os.path.basename(rel).startswith("UE4Editor")

    if receipt:
        # Exactly what UnrealBuildTool recorded for this executable, like UAT does when staging
        log("Engine plugins enabled for the application")
        enabled = {p["Name"] for p in receipt.get("Plugins", []) if p.get("Enabled")}
        for descriptor in glob.glob(os.path.join(engine, "Plugins", "**", "*.uplugin"), recursive=True):
            if os.path.splitext(os.path.basename(descriptor))[0] in enabled:
                plugin_dir = os.path.dirname(descriptor)
                copy_tree(plugin_dir, os.path.join(out_engine, os.path.relpath(plugin_dir, engine)), platform,
                          exclude_dirs=("Source", "SourceArt", "Intermediate", "Documentation", "Tests"),
                          exclude_ext=binary_ext + build_junk, keep=not_shared_binaries)

        log("Runtime dependencies from the target receipt")
        missing = stage_runtime_dependencies(receipt, engine, out_engine, out_project, binary_ext)
        if missing:
            log("%d runtime dependencies were not found (listed in %s)" % (len(missing), os.path.join(output_root, "missing.txt")))
            write_text(os.path.join(output_root, "missing.txt"), "\n".join(missing) + "\n")
    else:
        log("No target receipt found: copying every engine plugin and third party library")
        copy_tree(os.path.join(engine, "Plugins"), os.path.join(out_engine, "Plugins"), platform,
                  exclude_dirs=("Source", "SourceArt", "Intermediate", "Documentation", "Tests"),
                  exclude_ext=binary_ext + build_junk, keep=not_shared_binaries)
        copy_tree(os.path.join(engine, "Binaries", "ThirdParty"), os.path.join(out_engine, "Binaries", "ThirdParty"), platform,
                  # CEF3: the application is built without the embedded web browser
                  exclude_dirs=("Mono", "CEF3"), exclude_ext=binary_ext)

    log("Shader compiler worker")
    engine_binaries = os.path.join(engine, "Binaries", platform)
    copy_tree(engine_binaries, os.path.join(out_engine, "Binaries", platform), platform,
              exclude_ext=binary_ext + build_junk,
              keep=lambda rel: os.path.basename(rel).startswith("ShaderCompileWorker"))
    scw_receipt = read_receipt(engine_binaries, platform, "Development", target="ShaderCompileWorker")
    if scw_receipt:
        stage_runtime_dependencies(scw_receipt, engine, out_engine, out_project, binary_ext)

    # Version + installed project marker: the executable opens this project with no arguments
    os.makedirs(os.path.join(out_engine, "Build"), exist_ok=True)
    shutil.copy2(os.path.join(engine, "Build", "Build.version"), os.path.join(out_engine, "Build", "Build.version"))
    write_text(os.path.join(out_engine, "Build", "InstalledBuild.txt"), "")
    write_text(os.path.join(out_engine, "Build", "InstalledProjectBuild.txt"), "%s/%s.uproject\n" % (PROJECT_NAME, PROJECT_NAME))

    relative_exe = os.path.join(PROJECT_NAME, "Binaries", platform, exe_name)
    if platform == "Win64":
        redist = os.path.join(engine, "Extras", "Redist", "en-us", "UE4PrereqSetup_x64.exe")
        if os.path.isfile(redist):
            target = os.path.join(out_engine, "Extras", "Redist", "en-us")
            os.makedirs(target, exist_ok=True)
            shutil.copy2(redist, target)

        bootstrap = os.path.join(engine_binaries, "BootstrapPackagedGame-Win64-Shipping.exe")
        launcher = os.path.join(out, TARGET + ".exe")
        if os.path.isfile(bootstrap):
            log("Launcher " + launcher)
            shutil.copy2(bootstrap, launcher)
            os.chmod(launcher, 0o755)
            stamp_launcher(launcher, relative_exe.replace("/", "\\"), "", os.path.join(PROJECT_ROOT, "Build", "Windows", "Application.ico"))
        else:
            log("BootstrapPackagedGame not built: start %s directly" % relative_exe)
    else:
        launcher = os.path.join(out, TARGET + ".sh")
        write_text(launcher, '#!/bin/sh\ncd "$(dirname "$0")"\nexec "./%s" "$@"\n' % relative_exe.replace("\\", "/"))
        os.chmod(launcher, 0o755)

    size = sum(os.path.getsize(os.path.join(d, f)) for d, _, files in os.walk(out) for f in files)
    log("Done: %s (%.1f GB)" % (out, size / 1e9))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", default=os.environ.get("UE4_ROOT"), help="UE 4.27 source build root (or set UE4_ROOT)")
    parser.add_argument("--output", default=os.path.join(PROJECT_ROOT, "Dist"))
    parser.add_argument("--platform", default="Win64" if os.name == "nt" else "Linux", choices=("Win64", "Linux"))
    parser.add_argument("--configuration", default="Development", choices=("Development", "DebugGame"))
    parser.add_argument("--skip-build", action="store_true", help="package what is already built")
    parser.add_argument("--symbols", action="store_true", help="include debug symbols (.pdb)")
    args = parser.parse_args()

    if not args.engine or not os.path.isfile(os.path.join(args.engine, "Engine", "Build", "Build.version")):
        sys.exit("Pass the UE 4.27 source build root with --engine (or set UE4_ROOT).")

    engine_root = os.path.abspath(args.engine)
    if not args.skip_build:
        build(engine_root, args.platform, args.configuration)
    package(engine_root, os.path.abspath(args.output), args.platform, args.configuration, args.symbols)


if __name__ == "__main__":
    main()
