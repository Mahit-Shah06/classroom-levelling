@echo off
rem Package the Windows jam build. Run from a normal cmd window on Windows.
rem Needs: UE 5.7.4 (Epic Games Launcher) + Visual Studio 2022 with "Game development with C++".
rem Usage: tools\package_win.bat ["C:\Program Files\Epic Games\UE_5.7"]
setlocal
set ROOT=%~dp0..
set UE=%~1
if "%UE%"=="" set UE=C:\Program Files\Epic Games\UE_5.7
if not exist "%UE%\Engine\Build\BatchFiles\RunUAT.bat" (echo UE not found at "%UE%". Pass its path as the first argument. & exit /b 1)

rem 1) Template packs (mannequins, input) ship with the engine and are not in git.
for %%P in (Characters Input LevelPrototyping) do robocopy "%UE%\Templates\TemplateResources\High\%%P\Content" "%ROOT%\Content\%%P" /E /XC /XN /XO /NFL /NDL /NJH /NJS >nul

rem 2) Piper voices (Windows binary + voice model) are not in git either.
set P=%ROOT%\ThirdParty\Piper
if not exist "%P%\piper\piper.exe" (
  mkdir "%P%" 2>nul
  curl -sSL -o "%P%\w.zip" https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip || exit /b 1
  tar -xf "%P%\w.zip" -C "%P%" && del "%P%\w.zip"
)
if not exist "%P%\voice.onnx" curl -sSL -o "%P%\voice.onnx" https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/libritts_r/medium/en_US-libritts_r-medium.onnx
if not exist "%P%\voice.onnx.json" curl -sSL -o "%P%\voice.onnx.json" https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/libritts_r/medium/en_US-libritts_r-medium.onnx.json

rem 3) Build, cook, stage, pak.
call "%UE%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%ROOT%\ClassroomLevelling.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -prereqs -archive -archivedirectory="%ROOT%\Build" -nodebuginfo -utf8output || exit /b 1

rem 4) Put Piper next to the packaged project, then zip as TeamName_GameName.
robocopy "%P%" "%ROOT%\Build\Windows\ClassroomLevelling\ThirdParty\Piper" /E /NFL /NDL /NJH /NJS >nul
cd /d "%ROOT%\Build"
if exist Superrr_ClassroomLevelling.zip del Superrr_ClassroomLevelling.zip
tar -a -c -f Superrr_ClassroomLevelling.zip -C Windows .
echo Done: %ROOT%\Build\Superrr_ClassroomLevelling.zip
