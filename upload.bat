@echo off
echo LumiJ Arduino Upload Script
echo ============================

echo.
echo Compiling for Stamp1 (M5Stamp)...
arduino-cli compile --fqbn esp32:esp32:M5Stamp-C3 .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Stamp1 compile failed!
    pause
    exit /b 1
)

echo.
echo Compiling for Dial (M5Dial)...
arduino-cli compile --fqbn esp32:esp32:M5Dial-C3 .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Dial compile failed!
    pause
    exit /b 1
)

echo.
echo Compiling for Stamp2 (M5Stamp)...
arduino-cli compile --fqbn esp32:esp32:M5Stamp-C3 .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Stamp2 compile failed!
    pause
    exit /b 1
)

echo.
echo All compilation successful!
echo.

echo Uploading to COM6 (Stamp1)...
arduino-cli upload --fqbn esp32:esp32:M5Stamp-C3 --port COM6 .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Stamp1 upload failed!
    pause
    exit /b 1
)

echo.
echo Uploading to COM7 (Dial)...
arduino-cli upload --fqbn esp32:esp32:M5Dial-C3 --port COM7 .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Dial upload failed!
    pause
    exit /b 1
)

echo.
echo Uploading to COM8 (Stamp2)...
arduino-cli upload --fqbn esp32:esp32:M5Stamp-C3 --port COM8 .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Stamp2 upload failed!
    pause
    exit /b 1
)

echo.
echo All uploads completed successfully!
echo.
echo To monitor serial output for each device:
echo.
echo   Stamp1: arduino-cli monitor -p COM6 --config baudrate=115200
echo   Dial:    arduino-cli monitor -p COM7 --config baudrate=115200
echo   Stamp2:  arduino-cli monitor -p COM8 --config baudrate=115200
echo.
echo Press Ctrl+C to stop monitoring
pause