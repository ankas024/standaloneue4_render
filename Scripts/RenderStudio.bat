@echo off
rem Development launch: runs Render Studio inside UE4Editor in application mode (level editor hidden).
rem Usage: Scripts\RenderStudio.bat C:\UnrealEngine-4.27   (or set UE4_ROOT)
setlocal
set UE_ROOT=%~1
if "%UE_ROOT%"=="" set UE_ROOT=%UE4_ROOT%
if "%UE_ROOT%"=="" (
	echo Pass the engine root, e.g. Scripts\RenderStudio.bat C:\UnrealEngine-4.27
	exit /b 1
)
start "" "%UE_ROOT%\Engine\Binaries\Win64\UE4Editor.exe" "%~dp0..\RenderStudio.uproject" -RenderStudio
