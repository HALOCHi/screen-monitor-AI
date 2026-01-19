@echo off
set "EXE_NAME=screen_monitor.exe"
set "DEST_DIR=%APPDATA%\ScreenMonitor"
set "EXE_PATH=%DEST_DIR%\%EXE_NAME%"

echo --- Installing Screen Monitor ---

if not exist "%DEST_DIR%" mkdir "%DEST_DIR%"

copy "build\Release\%EXE_NAME%" "%EXE_PATH%" /Y

reg add "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run" /v "ScreenMonitor" /t REG_SZ /d "\"%EXE_PATH%\"" /f

echo --- Done! The app will start automatically at login. ---
pause