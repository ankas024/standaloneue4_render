@echo off
rem Development build: compiles the project modules for UE4Editor (use Package.bat for the standalone app).
rem Usage: Scripts\Build.bat C:\UnrealEngine-4.27   (or set UE4_ROOT)
setlocal
set UE_ROOT=%~1
if "%UE_ROOT%"=="" set UE_ROOT=%UE4_ROOT%
if "%UE_ROOT%"=="" (
	echo Pass the engine root, e.g. Scripts\Build.bat C:\UnrealEngine-4.27
	exit /b 1
)
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" RenderStudioEditor Win64 Development -Project="%~dp0..\RenderStudio.uproject" -WaitMutex -FromMsBuild
