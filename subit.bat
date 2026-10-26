@echo off
setlocal enabledelayedexpansion

:: CONFIG
set REPO_PATH=%~dp0
set BRANCH=main
set COMMIT_MSG=auto: update %date% %time%

:: NAV
cd /d "%REPO_PATH%"

:: CHECK GIT
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo [!] No git repo found. Initializing...
    git init
    git remote add origin YOUR_REMOTE_URL_HERE
)

:: STAGE ALL
git add -A

:: CHECK IF ANYTHING TO COMMIT
git diff --cached --quiet
if not errorlevel 1 (
    echo [~] Nothing to commit. Already up to date.
    pause
    exit /b 0
)

:: COMMIT
git commit -m "%COMMIT_MSG%"

:: PUSH
git push origin %BRANCH%
if errorlevel 1 (
    echo [!] Push failed. Trying with upstream set...
    git push --set-upstream origin %BRANCH%
)

echo.
echo [OK] Subido a GitHub: %BRANCH%
pause