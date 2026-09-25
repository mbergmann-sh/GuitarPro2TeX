@echo off
rem ---------------------------------------------------------------------------
rem  GuitarPROtoTeX-Convert - stage a release build for the Inno Setup installer
rem
rem  Called automatically after linking (see GuitarPROtoTeX-Convert.pro):
rem      install_stage.bat <exe dir> <installer dir> <project dir>
rem
rem  Result: <installer dir>\install_src  with the .exe, all DLLs and Qt plugin
rem  folders (after windeployqt), the examples, LICENSE and README - but without
rem  build files (*.o, moc_*.cpp, Makefiles, ...).
rem
rem  robocopy returns 0..7 on success and >= 8 on errors; this script always
rem  returns 0 on success, so make does not stop.
rem ---------------------------------------------------------------------------
setlocal
set "SRC=%~1"
set "INST=%~2"
set "PROJ=%~3"
if "%SRC%"=="" goto usage
if "%INST%"=="" goto usage
if "%PROJ%"=="" goto usage
rem the exe dir comes from make: relative ("release\" or "release/"), maybe with a
rem trailing separator - normalise it, because robocopy misreads  "path\"  (the
rem backslash escapes the closing quote)
set "SRC=%SRC:/=\%"
if "%SRC:~-1%"=="\" set "SRC=%SRC:~0,-1%"
for %%I in ("%SRC%") do set "SRC=%%~fI"
if not exist "%SRC%\gPro8toTeX.exe" (
    echo install_stage.bat: "%SRC%\gPro8toTeX.exe" not found
    exit /b 1
)
set "DST=%INST%\install_src"

if exist "%DST%" rmdir /s /q "%DST%"

robocopy "%SRC%" "%DST%" /E /NFL /NDL /NJH /NJS /NP ^
    /XF *.o *.obj *.cpp *.h *.rc *.res *.a Makefile* .qmake.stash object_script.* ^
    /XD .moc .obj .rcc .ui >nul
if errorlevel 8 goto fail

robocopy "%PROJ%\samples" "%DST%\samples" /E /NFL /NDL /NJH /NJS /NP ^
    /XF *.aux *.log *.mx1 *.mx2 *.toc *.out *.synctex.gz >nul
if errorlevel 8 goto fail

copy /y "%PROJ%\LICENSE"   "%DST%\LICENSE.txt" >nul || goto fail
copy /y "%PROJ%\README.md" "%DST%\README.md"   >nul || goto fail

echo Installer files staged in "%DST%"
exit /b 0

:usage
echo usage: install_stage.bat ^<exe dir^> ^<installer dir^> ^<project dir^>
exit /b 1

:fail
echo install_stage.bat: copying failed
exit /b 1
