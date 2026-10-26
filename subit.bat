@echo off

set REPO=C:\Users\anoni\Downloads\v67-github-build\v67-gh

cd /d "%REPO%"

:: fix workflow branch
if not exist ".github\workflows" mkdir ".github\workflows"

(
echo name: Build v67suite
echo.
echo on:
echo   push:
echo     branches: [ master ]
echo   workflow_dispatch:
echo.
echo jobs:
echo   build:
echo     runs-on: windows-latest
echo.
echo     steps:
echo       - name: Checkout repo
echo         uses: actions/checkout@v4
echo.
echo       - name: Setup MSVC
echo         uses: microsoft/setup-msbuild@v2
echo.
echo       - name: Download ImGui
echo         shell: powershell
echo         run: ^|
echo           Invoke-WebRequest -Uri "https://github.com/ocornut/imgui/archive/refs/tags/v1.90.4.zip" -OutFile "imgui.zip"
echo           Expand-Archive -Path "imgui.zip" -DestinationPath "."
echo           Rename-Item "imgui-1.90.4" "imgui"
echo.
echo       - name: Configure CMake
echo         run: cmake -B build -G "Visual Studio 17 2022" -A x64
echo.
echo       - name: Build Release
echo         run: cmake --build build --config Release
echo.
echo       - name: Upload artifact
echo         uses: actions/upload-artifact@v4
echo         with:
echo           name: v67suite-exe
echo           path: build/Release/v67suite.exe
) > ".github\workflows\build.yml"

git add -A
git commit -m "fix: workflow branch master + trigger build"
git push origin master

echo.
echo [OK] Subido. Ve a github.com/anonimanonimo6767-prog/asd pestaña Actions
pause