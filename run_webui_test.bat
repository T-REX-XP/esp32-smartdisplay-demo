@echo off
echo ========================================
echo ESP32 Simulator Web UI - Full Hardware Mode
echo ========================================
echo.
echo Same functionality as esp32_simulator.py
echo with user-friendly web interface!
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    if exist "%USERPROFILE%\.platformio\penv\Scripts\python.exe" (
        set "PYTHON_CMD=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
        goto python_found
    )

    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://python.org
    pause
    exit /b 1
)

set "PYTHON_CMD=python"

:python_found

REM Check if required packages are installed
echo Checking dependencies...
"%PYTHON_CMD%" -m pip show flask >nul 2>&1
if errorlevel 1 (
    echo Installing required packages...
    "%PYTHON_CMD%" -m pip install -r requirements.txt
    if errorlevel 1 (
        echo ERROR: Failed to install dependencies
        pause
        exit /b 1
    )
)

echo.
echo Starting ESP32 Simulator Web UI (Full Hardware Mode)...
echo Web interface will be available at: http://localhost:5000
echo.
echo This connects to REAL ESP32 hardware via serial!
echo.
echo Press Ctrl+C to stop the server
echo.

REM Run the full hardware web UI with default Windows settings
"%PYTHON_CMD%" esp32_simulator_webui.py COM3 115200 --format json --web-port 5000

pause