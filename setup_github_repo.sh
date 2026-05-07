#!/bin/bash

echo "========================================="
echo "  ESP32-C3项目GitHub仓库设置"
echo "========================================="
echo ""

# 检查是否安装了GitHub CLI
if ! command -v gh &> /dev/null; then
    echo "错误: 未检测到GitHub CLI (gh)"
    echo "请先安装GitHub CLI: https://cli.github.com/"
    echo ""
    echo "Windows用户可以使用以下命令安装:"
    echo "  winget install GitHub.cli"
    echo ""
    exit 1
fi

# 检查是否已登录GitHub
if ! gh auth status &> /dev/null; then
    echo "请先登录GitHub..."
    gh auth login
    echo ""
fi

# 提示用户输入GitHub用户名
read -p "请输入您的GitHub用户名: " GITHUB_USER

# 仓库名称
REPO_NAME="high_frequency_esp32c3"

echo ""
echo "正在创建GitHub仓库: $GITHUB_USER/$REPO_NAME ..."
echo ""

# 使用GitHub CLI创建公开仓库
gh repo create "$GITHUB_USER/$REPO_NAME" --public --description="ESP32-C3 High Frequency PWM Controller with USB CDC and MQTT"

# 初始化Git（如果尚未初始化）
if [ ! -d ".git" ]; then
    echo "初始化Git仓库..."
    git init
fi

# 添加远程仓库
echo "添加远程仓库..."
git remote add origin "https://github.com/$GITHUB_USER/$REPO_NAME.git"

# 添加所有文件
echo "添加文件到Git..."
git add .

# 首次提交
echo "创建首次提交..."
git commit -m "Initial commit: ESP32-C3 PWM controller with USB CDC support

- WiFi AirKiss配网
- MQTT通信（鸿蒙/HA）
- LEDC高频PWM输出（10kHz-300kHz）
- USB CDC调试输出
- NVS参数存储
- 按键控制和快速重启恢复出厂
"

# 推送到GitHub
echo "推送到GitHub..."
git branch -M main
git push -u origin main

echo ""
echo "========================================="
echo "  完成！"
echo "========================================="
echo ""
echo "仓库地址: https://github.com/$GITHUB_USER/$REPO_NAME"
echo ""
echo "GitHub Actions将自动开始编译..."
echo "请访问以下页面查看编译状态:"
echo "https://github.com/$GITHUB_USER/$REPO_NAME/actions"
echo ""
echo "编译完成后，运行以下命令下载merged固件:"
echo "  .\download_firmware.ps1"
echo ""
echo "固件将保存到 downloaded_firmware/merged.bin"
echo ""
