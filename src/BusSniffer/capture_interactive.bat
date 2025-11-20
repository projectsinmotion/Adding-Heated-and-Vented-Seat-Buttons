@echo off
echo ========================================
echo Interactive Bus Sniffer Capture
echo ========================================
echo.
echo This script lets you send commands to the ESP32
echo and automatically saves all output.
echo.
cd /d "%~dp0"
python serial_capture_interactive.py
pause
