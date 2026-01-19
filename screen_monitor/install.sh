#!/bin/bash

if [ "$EUID" -ne 0 ]; then
  echo "Нужно запустить скрипт через sudo: sudo ./install.sh"
  exit 1
fi

echo "--- 1. Настройка репозиториев и окружения ---"
if ! grep -q "repository-extended" /etc/apt/sources.list; then
    echo "Попытка добавить базовые репозитории Astra Linux для установки зависимостей..."
fi

apt-get update

echo "--- 2. Установка системных зависимостей ---"
apt-get install -y build-essential g++ cmake libcurl4-openssl-dev scrot imagemagick

echo "--- 3. Сборка клиента (C++) ---"
rm -rf build
mkdir build
cd build

export CXX=/usr/bin/g++

cmake ..
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "Ошибка компиляции. Проверьте логи выше"
    exit 1
fi
cd ..

echo "--- 4. Установка бинарного файла ---"
INSTALL_PATH="/usr/local/bin/screen_monitor"
cp build/screen_monitor $INSTALL_PATH
chmod +x $INSTALL_PATH

REAL_USER=${SUDO_USER:-$USER}
USER_ID=$(id -u $REAL_USER)
GROUP_ID=$(id -g $REAL_USER)

echo "--- 5. Создание системного демона (Systemd) ---"
SERVICE_FILE="/etc/systemd/system/screen-monitor.service"

DETECTED_DISPLAY=$(who | grep "($REAL_USER)" | grep -o '(:[0-9])' | head -n 1 | tr -d '()')
if [ -z "$DETECTED_DISPLAY" ]; then
    DETECTED_DISPLAY=":0"
fi

cat <<EOF > $SERVICE_FILE
[Unit]
Description=AI Employee Monitoring Client
After=graphical.target network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$REAL_USER
Group=$GROUP_ID
ExecStart=$INSTALL_PATH
Restart=always
RestartSec=30

# Переменные для доступа к иксам (скриншотам)
Environment=DISPLAY=$DETECTED_DISPLAY
Environment=XDG_RUNTIME_DIR=/run/user/$USER_ID
# Чтобы curl не падал при отсутствии интернета сразу
Environment=CURL_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt

[Install]
WantedBy=graphical.target
EOF

echo "--- 6. Запуск службы ---"
systemctl daemon-reload
systemctl enable screen-monitor.service
systemctl restart screen-monitor.service

echo "------------------------------------------------"
echo "Установка завершена успешно!"
echo "Пользователь: $REAL_USER"
echo "Смотреть лог демона (статус скрина и отправки на сервер): journalctl -u screen-monitor -f"
echo "------------------------------------------------"