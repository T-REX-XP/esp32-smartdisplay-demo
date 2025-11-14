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
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://python.org
    pause
    exit /b 1
)

REM Check if required packages are installed
echo Checking dependencies...
pip show flask >nul 2>&1
if errorlevel 1 (
    echo Installing required packages...
    pip install -r requirements.txt
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
python esp32_simulator_webui_test.py COM3 115200 --format msgpack --web-port 5000

pause