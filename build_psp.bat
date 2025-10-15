@echo off
REM Build script for gpSP PSP standalone (Windows)

echo ================================================
echo   Building gpSP for PSP
echo ================================================
echo.

REM Check if PSPSDK is installed
where psp-gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: psp-gcc not found!
    echo Please install PSPSDK and add it to your PATH
    pause
    exit /b 1
)

echo PSPSDK found!
echo.

REM Clean previous build
echo Cleaning previous build...
make -f Makefile.psp clean
echo.

REM Build
echo Building gpSP...
make -f Makefile.psp

REM Check if build succeeded
if exist EBOOT.PBP (
    echo.
    echo ================================================
    echo   Build successful!
    echo ================================================
    echo.
    echo Output files:
    echo   - EBOOT.PBP (Copy this to ms0:/PSP/GAME/gpSP/^)
    echo.
    echo Installation:
    echo   1. Copy EBOOT.PBP to ms0:/PSP/GAME/gpSP/
    echo   2. Create ms0:/PSP/GAME/gpSP/ROMS/
    echo   3. Put your .gba ROMs in the ROMS folder
    echo   4. Run gpSP from the PSP Game menu
    echo.
    pause
) else (
    echo.
    echo ================================================
    echo   Build failed!
    echo ================================================
    echo.
    echo Please check the error messages above.
    pause
    exit /b 1
)

