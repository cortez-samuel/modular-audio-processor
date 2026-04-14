@echo off
REM ──────────────────────────────────────────────────────────────────────────
REM  build_exe.bat
REM  Builds a single portable exe for the Feather RP2040 Controller
REM
REM  Requirements:
REM    Python 3.10+  (add to PATH)
REM    pip install -r requirements.txt
REM
REM  Output:
REM    dist\FeatherController.exe   ← copy this anywhere, run directly
REM ──────────────────────────────────────────────────────────────────────────

echo Installing dependencies...
py -m pip install -r requirements.txt

echo.
echo Building portable exe...
py -m PyInstaller ^
    --onefile ^
    --windowed ^
    --name FeatherController ^
    --hidden-import soundfile ^
    --hidden-import numpy ^
    --hidden-import serial ^
    --hidden-import serial.tools.list_ports ^
    --collect-data soundfile ^
    feather_controller.py

echo.
echo ────────────────────────────────────────────────────
echo  Done!  Your portable exe is at:
echo    dist\FeatherController.exe
echo ────────────────────────────────────────────────────
pause
