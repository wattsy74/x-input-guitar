# BGG XInput Firmware - Build and Flash Script
# PowerShell script to build and flash firmware to RP2040

param(
    [switch]$BuildOnly,
    [switch]$FlashOnly,
    [string]$Port,
    [switch]$Help
)

# Configuration
$PICOTOOL = "c:\tools\picotool\picotool.exe"
$BUILD_DIR = "build_pico"
$FIRMWARE_ELF = "$BUILD_DIR\bgg_xinput_firmware.elf"

function Show-Help {
    Write-Host @"
BGG XInput Firmware Builder

Usage:
    .\build-and-flash.ps1 [options]

Options:
    -BuildOnly      Only build the firmware (don't flash)
    -FlashOnly      Only flash existing firmware (don't build)
    -Port <port>    Specify serial port for flashing (optional)
    -Help           Show this help

Examples:
    .\build-and-flash.ps1                    # Build and flash
    .\build-and-flash.ps1 -BuildOnly         # Just build
    .\build-and-flash.ps1 -FlashOnly         # Just flash
    
Note: Device must be in BOOTSEL mode for flashing (hold BOOTSEL while connecting USB)
"@
}

function Test-Prerequisites {
    # Check if picotool exists
    if (!(Test-Path $PICOTOOL)) {
        Write-Error "picotool not found at $PICOTOOL"
        Write-Host "Please install picotool or update the path in this script"
        exit 1
    }
    
    # Check if build directory exists
    if (!(Test-Path $BUILD_DIR)) {
        Write-Error "Build directory not found: $BUILD_DIR"
        Write-Host "Run 'cmake -B $BUILD_DIR' first to create build directory"
        exit 1
    }
}

function Build-Firmware {
    Write-Host "🔨 Building firmware..." -ForegroundColor Yellow
    
    Push-Location $BUILD_DIR
    try {
        $result = ninja
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed!"
            exit 1
        }
        Write-Host "✅ Build successful!" -ForegroundColor Green
    }
    finally {
        Pop-Location
    }
    
    # Check if ELF file was created
    if (!(Test-Path $FIRMWARE_ELF)) {
        Write-Error "Firmware ELF not found: $FIRMWARE_ELF"
        exit 1
    }
    
    $size = (Get-Item $FIRMWARE_ELF).Length
    Write-Host "📦 Firmware size: $([math]::Round($size/1024, 2)) KB" -ForegroundColor Cyan
}

function Find-PicoDevice {
    # Try to detect Pico in BOOTSEL mode
    $devices = & $PICOTOOL info 2>$null
    if ($LASTEXITCODE -eq 0 -and $devices) {
        Write-Host "🔍 Found Pico device in BOOTSEL mode" -ForegroundColor Green
        return $true
    }
    return $false
}

function Flash-Firmware {
    Write-Host "🚀 Flashing firmware..." -ForegroundColor Yellow
    
    # Check if firmware exists
    if (!(Test-Path $FIRMWARE_ELF)) {
        Write-Error "Firmware not found: $FIRMWARE_ELF"
        Write-Host "Run with -BuildOnly first, or without -FlashOnly to build"
        exit 1
    }
    
    # Try to find device
    if (!(Find-PicoDevice)) {
        Write-Warning "No Pico device found in BOOTSEL mode"
        Write-Host "Please:"
        Write-Host "1. Disconnect your Pico"
        Write-Host "2. Hold the BOOTSEL button"
        Write-Host "3. Connect USB while holding BOOTSEL"
        Write-Host "4. Release BOOTSEL button"
        Write-Host "5. Run this script again"
        exit 1
    }
    
    # Flash the firmware
    Write-Host "📤 Loading firmware to device..."
    $result = & $PICOTOOL load $FIRMWARE_ELF -x
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Flash failed!"
        exit 1
    }
    
    Write-Host "✅ Firmware flashed successfully!" -ForegroundColor Green
    Write-Host "🎮 Device should now appear as XInput controller" -ForegroundColor Cyan
}

# Main script
if ($Help) {
    Show-Help
    exit 0
}

Write-Host "🎸 BGG XInput Firmware Builder" -ForegroundColor Magenta
Write-Host "=================================" -ForegroundColor Magenta

Test-Prerequisites

if (!$FlashOnly) {
    Build-Firmware
}

if (!$BuildOnly) {
    Flash-Firmware
}

Write-Host "🎉 All done!" -ForegroundColor Green
