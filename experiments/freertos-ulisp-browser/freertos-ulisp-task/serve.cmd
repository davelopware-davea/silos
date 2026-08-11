@echo off
setlocal

set "BUILD_DIR=%~dp0build"
set "FREEWISP_PORT=8765"

if not exist "%BUILD_DIR%\index.html" (
  echo FreeWisp browser build not found at:
  echo   %BUILD_DIR%
  echo Build freertos-ulisp-task before starting the server.
  exit /b 1
)

where python >nul 2>nul
if errorlevel 1 (
  echo Python was not found on PATH.
  exit /b 1
)

echo Serving FreeWisp at http://127.0.0.1:%FREEWISP_PORT%/
echo Press Ctrl+C to stop the server.
python -m http.server %FREEWISP_PORT% --bind 127.0.0.1 --directory "%BUILD_DIR%"
