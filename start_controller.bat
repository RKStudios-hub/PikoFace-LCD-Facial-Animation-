@echo off
:: Set UTF-8 encoding for emojis
chcp 65001 >nul

:: Set console title
title 🤖 MochiMouth - Arduino Mouth Controller

:: Clear screen
cls

:: Change to script directory
cd /d "%~dp0"

:: Run Python script
python arduino_mouth_controller.py

:: Pause so window stays open
pause
