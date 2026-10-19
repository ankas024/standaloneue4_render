@echo off
rem Builds RenderStudio.exe and packages it into Dist\RenderStudio (a self-contained folder).
rem Usage: Scripts\Package.bat C:\UnrealEngine-4.27 [--skip-build] [--symbols]   (or set UE4_ROOT)
rem The first build compiles the engine for this application: expect the time of a full editor build.
setlocal
set UE_ROOT=%~1
if "%UE_ROOT%"=="" set UE_ROOT=%UE4_ROOT%
if "%UE_ROOT%"=="" (
	echo Pass the engine root, e.g. Scripts\Package.bat C:\UnrealEngine-4.27
	exit /b 1
)
set PYTHON="%UE_ROOT%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not exist %PYTHON% set PYTHON=python
%PYTHON% "%~dp0package.py" --engine "%UE_ROOT%" %2 %3 %4
