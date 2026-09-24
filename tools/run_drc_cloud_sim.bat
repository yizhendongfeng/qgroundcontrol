@echo off
REM DGCS 云端 DRC 模拟器 —— 双击就能跑
REM 依赖：Python 3 + paho-mqtt（pip install paho-mqtt）
cd /d "%~dp0"
python drc_cloud_sim.py
if errorlevel 1 (
    echo.
    echo ---- 程序异常退出，错误信息见上 ----
    pause
)
