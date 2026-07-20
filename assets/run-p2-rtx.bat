@echo off
setlocal
cd /d "%~dp0"

set "PATCHED_BACKUP=portal2-rtx\portal2.exe.p2rtx"

rem ---------------------------------------------------------------------------
rem  Self-heal: Steam updates and "verify integrity of game files" restore the
rem  vanilla portal2.exe, which removes the p2-rtx.dll import and silently
rem  disables the mod. Detect that here and restore the patched exe before
rem  launching. (the patched exe contains the string "p2-rtx.dll", vanilla
rem  does not)
rem ---------------------------------------------------------------------------

findstr /m /c:"p2-rtx.dll" portal2.exe >nul 2>&1
if errorlevel 1 goto :heal

rem portal2.exe is patched - keep a copy so it can be restored after the next
rem Steam update
if not exist "portal2-rtx" mkdir "portal2-rtx"
fc /b portal2.exe "%PATCHED_BACKUP%" >nul 2>&1
if errorlevel 1 copy /y portal2.exe "%PATCHED_BACKUP%" >nul
goto :launch

:heal
rem a renamed bridge dll means the user disabled the mod with toggle-p2-rtx.bat
rem on purpose - launch the vanilla game instead of undoing that choice
if exist "bin\d3d9.dll.off" if not exist "bin\d3d9.dll" (
    echo [p2-rtx] The mod is disabled ^(toggle-p2-rtx.bat^) - starting the vanilla game.
    echo [p2-rtx] Run toggle-p2-rtx.bat to re-enable the mod.
    goto :launch
)
if not exist "%PATCHED_BACKUP%" (
    echo [p2-rtx] portal2.exe does not load p2-rtx.dll and no patched backup was found.
    echo [p2-rtx] Please re-install the mod ^(run the installer or extract the release zip again^).
    pause
    exit /b 1
)
echo [p2-rtx] A Steam update restored the vanilla portal2.exe - restoring the p2-rtx patched exe ...
copy /y portal2.exe portal2.exe.vanilla >nul
copy /y "%PATCHED_BACKUP%" portal2.exe >nul
findstr /m /c:"p2-rtx.dll" portal2.exe >nul 2>&1
if errorlevel 1 (
    echo [p2-rtx] Failed to restore the patched portal2.exe. Please re-install the mod.
    pause
    exit /b 1
)
echo [p2-rtx] Done. ^(the vanilla exe was saved as portal2.exe.vanilla^)

:launch
START portal2.exe -insecure -steam -novid -disable_d3d9_hacks -limitvsconst -softparticlesdefaultoff -disallowhwmorph -no_compressed_verts -nogamepadui +mat_phong 1 %*
