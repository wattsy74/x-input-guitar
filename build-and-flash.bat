@echo off
REM BGG XInput Firmware - Simple Build and Flash Script
REM Batch file version for Windows

echo.
echo 🎸 BGG XInput Firmware Builder
echo =================================

REM Configuration
set PICOTOOL=c:\tools\picotool\picotool.exe
set BUILD_DIR=build_pico
set FIRMWARE_ELF=%BUILD_DIR%\bgg_xinput_firmware.elf

REM Check prerequisites
if not exist "%PICOTOOL%" (
    echo ❌ Error: picotool not found at %PICOTOOL%
    echo Please install picotool or update the path in this script
    pause
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    echo ❌ Error: Build directory not found: %BUILD_DIR%
    echo Run 'cmake -B %BUILD_DIR%' first to create build directory
    pause
    exit /b 1
)

REM Build firmware
echo.
echo 🔨 Building firmware...
cd /d "%BUILD_DIR%"
ninja
if errorlevel 1 (
    echo ❌ Build failed!
    cd /d ..
    pause
    exit /b 1
)
cd /d ..

echo ✅ Build successful!

REM Check if firmware exists
if not exist "%FIRMWARE_ELF%" (
    echo ❌ Error: Firmware ELF not found: %FIRMWARE_ELF%
    pause
    exit /b 1
)

REM Show firmware size
for %%F in ("%FIRMWARE_ELF%") do set size=%%~zF
set /a size_kb=%size%/1024
echo 📦 Firmware size: %size_kb% KB

REM Check for device
echo.
echo 🔍 Checking for Pico device...
"%PICOTOOL%" info >nul 2>&1
if errorlevel 1 (
    echo ⚠️  No Pico device found in BOOTSEL mode
    echo.
    echo Please put your Pico in BOOTSEL mode:
    echo 1. Disconnect your Pico
    echo 2. Hold the BOOTSEL button
    echo 3. Connect USB while holding BOOTSEL
    echo 4. Release BOOTSEL button
    echo 5. Run this script again
    echo.
    pause
    exit /b 1
)

echo ✅ Found Pico device in BOOTSEL mode

REM Flash firmware
echo.
echo 🚀 Flashing firmware...
"%PICOTOOL%" load "%FIRMWARE_ELF%" -x
if errorlevel 1 (
    echo ❌ Flash failed!
    pause
    exit /b 1
)

echo.
echo ✅ Firmware flashed successfully!
echo 🎮 Device should now appear as XInput controller
echo.
echo 🎉 All done!
pause
