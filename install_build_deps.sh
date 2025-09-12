#!/bin/bash
# 自动安装 kde-oauth2-plugin 构建所需依赖
set -e

sudo apt-get update
sudo apt-get install -y \
  make cmake \
  g++ \
  qtbase5-dev qt5-qmake qtbase5-dev-tools \
  libkaccounts-dev libaccounts-glib-dev libaccounts-qt5-dev libsignon-qt5-dev \
  libkf5i18n-dev

echo "所有依赖已安装完毕。"
