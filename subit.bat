@echo off
setlocal enabledelayedexpansion

set REPO_PATH=C:\Users\anoni\v67-github-build
set BRANCH=main
set COMMIT_MSG=auto: update %date% %time%

cd /d "%REPO_PATH%"

git init

git remote remove origin 2>nul
git remote add origin https://github.com/anonimanonimo6767-prog/asd.git

git add -A

git diff --cached --quiet
if not errorlevel 1 (
    echo [~] Nada que subir.
    pause
    exit /b 0
)

git commit -m "%COMMIT_MSG%"

git branch -M main

git push -u origin main

echo.
echo [OK] Subido.
pause