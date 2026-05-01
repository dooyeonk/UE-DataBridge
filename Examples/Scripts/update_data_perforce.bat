@echo off
setlocal

set UE_CMD="C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set P4CLIENT=%P4CLIENT%
set SOURCE=%1
if "%SOURCE%"=="" set SOURCE=All

for /f "delims=" %%i in ('dir /b /s "%~dp0..\..\..\..\*.uproject" 2^>nul') do set PROJECT=%%i

if not defined PROJECT (
    echo ERROR: Could not find .uproject file
    exit /b 1
)

:: DryRun으로 대상 파일 파악
if /i "%SOURCE%"=="All" (
    %UE_CMD% "%PROJECT%" -run=DataBridgeUpdate -All -DryRun -unattended -nopause > "%TEMP%\db_dryrun.txt" 2>&1
) else (
    %UE_CMD% "%PROJECT%" -run=DataBridgeUpdate -SourceName=%SOURCE% -DryRun -unattended -nopause > "%TEMP%\db_dryrun.txt" 2>&1
)

:: Target 경로 추출 및 p4 checkout
for /f "tokens=2 delims=:" %%a in ('findstr "Target:" "%TEMP%\db_dryrun.txt"') do (
    set ASSET=%%a
    p4 edit !ASSET!
)

:: 실제 갱신
if /i "%SOURCE%"=="All" (
    %UE_CMD% "%PROJECT%" -run=DataBridgeUpdate -All -unattended -nopause
) else (
    %UE_CMD% "%PROJECT%" -run=DataBridgeUpdate -SourceName=%SOURCE% -unattended -nopause
)

if %ERRORLEVEL% neq 0 (
    echo ERROR: Commandlet failed
    p4 revert -a
    exit /b 1
)

:: p4 submit
p4 submit -d "Auto-update DataTable assets: %SOURCE%"

exit /b %ERRORLEVEL%
