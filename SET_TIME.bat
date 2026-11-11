@echo off
rem Double-click me: sends this computer's time to the K10 over the USB cable.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0set_time.ps1"
