@echo off
setlocal

set UE_CMD="C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

:: Find .uproject (assumes this script is in Plugins\DataBridge\Examples\Scripts\)
for /f "delims=" %%i in ('dir /b /s "%~dp0..\..\..\..\*.uproject" 2^>nul') do set PROJECT=%%i

if not defined PROJECT (
    echo ERROR: Could not find .uproject file
    exit /b 1
)

set SOURCE=%1
if "%SOURCE%"=="" set SOURCE=All

if /i "%SOURCE%"=="All" (
    %UE_CMD% "%PROJECT%" -run=DataBridgeUpdate -All -unattended -nopause
) else (
    %UE_CMD% "%PROJECT%" -run=DataBridgeUpdate -SourceName=%SOURCE% -unattended -nopause
)

exit /b %ERRORLEVEL%
