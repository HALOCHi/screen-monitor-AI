#!/bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "Пожалуйста, запустите скрипт через sudo: sudo ./install.sh"
  exit
fi

echo "--- Установка зависимостей ---"
if [ -f /etc/arch-release ] || [ -f /etc/manjaro-release ]; then
    # Для Manjaro/Arch
    pacman -Syu --noconfirm
    pacman -S --noconfirm base-devel cmake curl scrot grim spectacle gnome-screenshot
elif [ -f /etc/debian_version ] || [ -f /etc/astra_version ]; then
    # Для Astra Linux/Debian/Ubuntu
    apt-get update
    apt-get install -y build-essential cmake libcurl4-openssl-dev scrot gnome-screenshot
else
    echo "ОС не поддерживается скриптом. Установите зависимости вручную."
    exit 1
fi

echo "--- Сборка клиента ---"
mkdir -p build
cd build
cmake ..
make
if [ $? -ne 0 ]; then
    echo "Ошибка сборки!"
    exit 1
fi
cd ..

INSTALL_PATH="/usr/local/bin/screen_monitor"
cp build/screen_monitor $INSTALL_PATH
chmod +x $INSTALL_PATH

REAL_USER=${SUDO_USER:-$USER}
USER_HOME=$(getent passwd $REAL_USER | cut -d: -f6)

echo "--- Настройка демона для пользователя $REAL_USER ---"

CAT_SERVICE_PATH="/etc/systemd/system/screen-monitor.service"

cat <<EOF > $CAT_SERVICE_PATH
[Unit]
Description=Employee Monitoring Service
After=graphical.target

[Service]
ExecStart=$INSTALL_PATH
Restart=always
RestartSec=10
User=$REAL_USER
Group=$(id -gn $REAL_USER)
# Переменные окружения для доступа к графике
Environment=DISPLAY=:0
Environment=XDG_RUNTIME_DIR=/run/user/$(id -u $REAL_USER)
Environment=XDG_SESSION_TYPE=$(loginctl show-session $(loginctl | grep $REAL_USER | awk '{print $1}') -p Type --value)

[Install]
WantedBy=graphical.target
EOF

echo "--- Запуск службы ---"
systemctl daemon-reload
systemctl enable screen-monitor.service
systemctl restart screen-monitor.service

echo "------------------------------------------------"
echo "Установка завершена! Клиент работает в фоне."
echo "Проверить статус: systemctl status screen-monitor"
echo "Логи: journalctl -u screen-monitor -f"
