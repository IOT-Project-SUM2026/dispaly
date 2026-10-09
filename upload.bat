@echo off
setlocal
REM ============================================================
REM Smart Display - build and upload in one step (Windows)
REM
REM Uses the arduino-cli that ships with Arduino IDE 2 and the
REM "cyd" profile in main\sketch.yaml, which downloads the exact
REM ESP32 board package and library versions on first use.
REM
REM   upload.bat          build + upload
REM   upload.bat clean    full rebuild (use after changing build_opt.h)
REM ============================================================

REM --- find arduino-cli ---
set "CLI=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
if not exist "%CLI%" set "CLI=%ProgramFiles%\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
if not exist "%CLI%" (
  where arduino-cli >nul 2>nul && set "CLI=arduino-cli"
)
if not exist "%CLI%" if not "%CLI%"=="arduino-cli" (
  echo [!] arduino-cli not found. Install Arduino IDE 2 from https://www.arduino.cc/en/software
  goto :fail
)

set "CLEAN="
if /i "%~1"=="clean" set "CLEAN=--clean"

cd /d "%~dp0main"

echo.
echo === Updating package indexes ===
"%CLI%" core update-index || goto :fail
"%CLI%" lib update-index || goto :fail

echo.
echo === Connected boards ===
"%CLI%" board list
set "PORT="
set /p PORT=Enter the board's COM port (e.g. COM5):
if "%PORT%"=="" (
  echo [!] No port given. If no port is listed, install the CH340 USB driver.
  goto :fail
)

echo.
echo === Building (first time downloads everything and takes several minutes) ===
"%CLI%" compile --profile cyd %CLEAN% . || goto :fail

echo.
echo === Uploading to %PORT% (hold BOOT on the board if it hangs on "Connecting...") ===
"%CLI%" upload --profile cyd -p %PORT% . || goto :fail

echo.
echo [OK] Upload finished.
set "MON="
set /p MON=Open the serial monitor? (y/N):
if /i "%MON%"=="y" (
  echo Press Ctrl+C to exit the monitor.
  "%CLI%" monitor -p %PORT% -c baudrate=115200
)
goto :end

:fail
echo.
echo [!] Failed - see the messages above, and the Troubleshooting section in README.md.
pause
exit /b 1

:end
pause
endlocal
