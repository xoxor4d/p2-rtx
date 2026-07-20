@echo off
setlocal
cd /d "%~dp0"

set "BRIDGE=bin\d3d9.dll"
set "PATCHED_BACKUP=portal2-rtx\portal2.exe.p2rtx"
set "VANILLA_BACKUP=portal2.exe.vanilla"

rem ---------------------------------------------------------------------------
rem  The mod loads because portal2.exe was patched to import p2-rtx.dll.
rem  enabled  = patched portal2.exe + active remix bridge (bin\d3d9.dll)
rem  disabled = vanilla portal2.exe + bridge renamed to bin\d3d9.dll.off
rem  A vanilla exe combined with an active bridge usually means a Steam update
rem  restored the original portal2.exe.
rem ---------------------------------------------------------------------------

set "EXE=vanilla"
findstr /m /c:"p2-rtx.dll" portal2.exe >nul 2>&1 && set "EXE=patched"

set "STATUS=unknown"
if "%EXE%"=="patched" if exist "%BRIDGE%" set "STATUS=enabled"
if "%EXE%"=="vanilla" if exist "%BRIDGE%.off" set "STATUS=disabled"
if "%EXE%"=="vanilla" if exist "%BRIDGE%" set "STATUS=partial (vanilla exe, active bridge - Steam update?)"
if "%EXE%"=="patched" if exist "%BRIDGE%.off" if not exist "%BRIDGE%" set "STATUS=partial (patched exe, inactive bridge)"

echo Current mod status: %STATUS%
echo.

if "%STATUS%"=="unknown" (
    echo Error: neither "%BRIDGE%" nor "%BRIDGE%.off" exists.
    echo Please re-install the mod ^(run the installer or extract the release zip again^).
    pause
    exit /b 1
)

if "%STATUS%"=="enabled" (
    set "QUESTION=Would you like to disable the mod? (y/n)"
    set "ACTION=disable"
) else (
    set "QUESTION=Would you like to enable the mod? (y/n)"
    set "ACTION=enable"
)

:PROMPT
set "CHOICE="
set /p CHOICE="%QUESTION% "
if not defined CHOICE goto :EXIT
if /i "%CHOICE%"=="y" goto :PROCESS
if /i "%CHOICE%"=="n" goto :EXIT
echo Please enter y or n
goto :PROMPT

:PROCESS
if "%ACTION%"=="enable" goto :ENABLE

rem --- disable -----------------------------------------------------------
rem the patched exe cannot run without p2-rtx.dll (hard import), so disabling
rem requires restoring the vanilla exe
if "%EXE%"=="patched" (
    if not exist "%VANILLA_BACKUP%" (
        echo Error: no vanilla backup "%VANILLA_BACKUP%" found.
        echo Use Steam: right-click Portal 2 -^> Properties -^> Installed Files -^>
        echo "Verify integrity of game files" to restore the vanilla portal2.exe,
        echo then run this script again to deactivate the remix bridge.
        pause
        exit /b 1
    )
    rem keep a copy of the patched exe so the mod can be re-enabled
    if not exist "portal2-rtx" mkdir "portal2-rtx"
    copy /y portal2.exe "%PATCHED_BACKUP%" >nul
    if errorlevel 1 goto :OPFAIL
    copy /y "%VANILLA_BACKUP%" portal2.exe >nul
    if errorlevel 1 goto :OPFAIL
)
if exist "%BRIDGE%" (
    rem the active bridge dll is authoritative - drop a leftover .off copy
    if exist "%BRIDGE%.off" del "%BRIDGE%.off" >nul 2>&1
    ren "%BRIDGE%" "d3d9.dll.off"
    if errorlevel 1 goto :OPFAIL
)
rem verify the resulting state before claiming success
findstr /m /c:"p2-rtx.dll" portal2.exe >nul 2>&1
if not errorlevel 1 goto :OPFAIL
if exist "%BRIDGE%" goto :OPFAIL
echo Mod has been disabled.
echo See the README's "How do I disable remix?" section for game cvars you
echo might want to reset ^(r_portal_stencil_depth 2, mat_fullbright 0, ...^).
goto :EXIT

:ENABLE
if "%EXE%"=="vanilla" (
    if not exist "%PATCHED_BACKUP%" (
        echo Error: no patched backup "%PATCHED_BACKUP%" found.
        echo Please re-install the mod ^(run the installer or extract the release zip again^).
        pause
        exit /b 1
    )
    copy /y portal2.exe "%VANILLA_BACKUP%" >nul
    if errorlevel 1 goto :OPFAIL
    copy /y "%PATCHED_BACKUP%" portal2.exe >nul
    if errorlevel 1 goto :OPFAIL
)
if exist "%BRIDGE%.off" if not exist "%BRIDGE%" (
    ren "%BRIDGE%.off" "d3d9.dll"
    if errorlevel 1 goto :OPFAIL
)
rem verify the resulting state before claiming success
findstr /m /c:"p2-rtx.dll" portal2.exe >nul 2>&1
if errorlevel 1 goto :OPFAIL
if not exist "%BRIDGE%" goto :OPFAIL
rem clean up a leftover .off copy so the status check stays unambiguous
if exist "%BRIDGE%.off" del "%BRIDGE%.off" >nul 2>&1
echo Mod has been enabled.
goto :EXIT

:OPFAIL
echo.
echo Error: a file operation failed or the result could not be verified.
echo ^(is the game still running? are portal2.exe or bin\d3d9.dll read-only?^)
echo Please close the game, check the files and run this script again.
pause
exit /b 1

:EXIT
echo.
pause
