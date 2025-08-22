@echo off
echo BGG XInput Firmware Flasher
echo ============================

cd build_pico

echo Checking for Pico device...
c:\tools\picotool\picotool.exe info > nul 2>&1
if errorlevel 1 (
    echo.
    echo No Pico device found in BOOTSEL mode!
    echo.
    echo Please:
    echo 1. Disconnect your Pico
    echo 2. Hold the BOOTSEL button
    echo 3. Connect USB while holding BOOTSEL
    echo 4. Release BOOTSEL button
    echo 5. Run this script again
    echo.
    pause
    exit /b 1
)

echo Found Pico device in BOOTSEL mode!
echo.
echo Flashing firmware...
c:\tools\picotool\picotool.exe load bgg_xinput_firmware.elf -x
if errorlevel 1 (
    echo Flash failed!
    pause
    exit /b 1
)

echo.
echo Firmware flashed successfully!
echo Device should now appear as XInput controller
echo.
pause
