@echo off
echo BGG XInput Firmware Builder
echo ============================

echo Building firmware...
cd build_pico
ninja
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build successful!
echo Firmware files created:
echo - bgg_xinput_firmware.uf2 (for flashing)
echo - bgg_xinput_firmware.elf (for debugging)
echo.

echo To flash firmware:
echo 1. Hold BOOTSEL button on Pico
echo 2. Connect USB while holding BOOTSEL
echo 3. Release BOOTSEL button
echo 4. Run: c:\tools\picotool\picotool.exe load bgg_xinput_firmware.elf -x
echo.

pause
