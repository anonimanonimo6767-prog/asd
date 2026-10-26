@echo off
setlocal enabledelayedexpansion

set REPO_PATH=C:\Users\anoni\Downloads\v67-github-build\v67-gh
set BRANCH=master
set COMMIT_MSG=auto: update %date% %time%

cd /d "%REPO_PATH%"
if errorlevel 1 (
    echo [!] Ruta no encontrada: %REPO_PATH%
    pause
    exit /b 1
)

git remote remove origin 2>nul
git remote add origin https://github.com/anonimanonimo6767-prog/asd.git

git add -A

git diff --cached --quiet
if not errorlevel 1 (
    echo [~] Nada que subir, todo igual.
    pause
    exit /b 0
)

git commit -m "%COMMIT_MSG%"

git branch -M master

git push -u origin master

echo.
echo [OK] Subido a GitHub: %BRANCH%
pause