@echo off
setlocal

if "%~1"=="" (
    echo Usage: %~nx0 COMx
    echo Example: %~nx0 COM6
    exit /b 1
)

set PORT=%~1
set FQBN=m5stack:esp32:m5stack_stamp_s3
set ADDURL=https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json

echo M5StampS3 Arduino Upload Script
echo ===============================
echo Port : %PORT%
echo FQBN : %FQBN%
echo.

echo Copying shared constants...
copy "..\shared\const.h" "const.h" /Y
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Failed to copy shared constants!
    pause
    exit /b 1
)
echo Shared constants copied successfully!
echo.

echo Updating board index...
arduino-cli core update-index --additional-urls %ADDURL%

echo.
echo Checking installed cores...
arduino-cli core list

echo.
echo Listing matching boards...
arduino-cli board listall M5StampS3 --additional-urls %ADDURL%

echo.
echo Compiling for M5StampS3...
arduino-cli compile --fqbn %FQBN% --additional-urls %ADDURL% .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Compile failed!
    echo.
    echo If m5stack:esp32 is not installed yet, run:
    echo arduino-cli core install m5stack:esp32 --additional-urls %ADDURL%
    pause
    exit /b 1
)

echo.
echo Uploading to %PORT%...
arduino-cli upload --fqbn %FQBN% --port %PORT% --additional-urls %ADDURL% .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Upload failed!
    pause
    exit /b 1
)

echo.
echo Upload completed successfully!
echo Waiting for Arduino to initialize (3 seconds)...
timeout /t 3 /nobreak >nul

echo.
echo To monitor serial output, run:
echo arduino-cli monitor -p %PORT% --config baudrate=115200

pause
endlocal