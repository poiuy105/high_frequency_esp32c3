# 快速开始指南

## 📋 前置要求

1. **Git Bash** - 已安装
2. **GitHub CLI (gh)** - 需要安装
3. **GitHub账号** - 用于创建仓库

## 🚀 快速开始（3步完成）

### 步骤1: 安装GitHub CLI

在Git Bash中运行：

```bash
winget install GitHub.cli
```

或者访问 https://cli.github.com/ 下载安装

### 步骤2: 运行设置脚本

在Git Bash中，进入项目目录后运行：

```bash
chmod +x setup_github_repo.sh
./setup_github_repo.sh
```

脚本会自动：
- ✅ 检查并登录GitHub
- ✅ 创建公开仓库
- ✅ 初始化Git并提交代码
- ✅ 推送到GitHub

### 步骤3: 等待编译完成

1. 访问 `https://github.com/<你的用户名>/high_frequency_esp32c3/actions`
2. 等待5-10分钟编译完成
3. 下载 `firmware-flash.zip`
4. 解压得到完整的bin文件

## 📥 下载编译好的固件

### 方法1: 使用自动下载脚本（推荐）

在项目目录下运行PowerShell脚本：

```powershell
.\download_firmware.ps1
```

脚本会自动：
- ✅ 检测GitHub仓库信息
- ✅ 查找最新成功编译的任务
- ✅ 下载merged.bin固件到 `downloaded_firmware` 目录

### 方法2: 手动下载

1. 访问 `https://github.com/<你的用户名>/high_frequency_esp32c3/actions`
2. 点击最新成功的workflow运行
3. 在页面底部的"Artifacts"部分，点击 `merged-firmware` 下载
4. 解压得到 `merged.bin` 文件

编译完成后，您会得到以下文件：

```
downloaded_firmware/
└── merged.bin              # 合并后的完整固件（包含bootloader、分区表和应用程序）
```

## 🔧 烧录固件到ESP32-C3

### 方法1: 使用esptool.py（推荐）

```powershell
esptool.py --chip esp32c3 --port COM3 write_flash 0x0 downloaded_firmware\merged.bin
```

注意：将COM3替换为您的实际串口号

### 方法2: 分别烧录各个文件

如果您有单独的文件，也可以分别烧录：

```bash
esptool.py --chip esp32c3 --port COM3 write_flash ^
  0x0 bootloader.bin ^
  0x8000 partition-table.bin ^
  0x10000 high_frequency.bin
```

注意：将COM3替换为您的实际串口号

## 💻 查看USB CDC日志

1. 连接ESP32-C3到电脑USB口
2. 打开设备管理器，找到新的COM端口
3. 使用串口工具（PuTTY、MobaXterm等）打开该端口
4. 波特率任意（USB虚拟串口不需要设置波特率）
5. 即可看到系统日志

## ⚙️ 首次配置

### WiFi配网

1. 手机连接2.4GHz WiFi
2. 打开微信小程序"乐鑫AirKiss"
3. 输入WiFi密码
4. 等待配网成功

### MQTT配置

目前MQTT服务器地址需要从NVS读取，您可以：

1. 修改 [nvs_param.c](file://e:\Espidf\high_frequency\nvs_param.c) 中的默认值
2. 重新编译烧录

例如：
```c
char mqtt_uri[64] = "mqtt://your-server.com";
char mqtt_user[32] = "your-username";
char mqtt_pass[32] = "your-password";
```

## 🎯 测试PWM输出

通过MQTT发送命令控制PWM：

```bash
# 开启PWM
mosquitto_pub -t "harmony/pwm/switch" -m "ON"

# 设置频率为100kHz
mosquitto_pub -t "harmony/pwm/freq" -m "100000"

# 设置占空比为50%（128/255）
mosquitto_pub -t "harmony/pwm/duty" -m "128"
```

## ❓ 常见问题

### Q: GitHub CLI安装失败？
A: 可以手动创建仓库：
1. 在GitHub网页上创建新仓库
2. 运行以下命令：
```bash
git init
git remote add origin https://github.com/用户名/high_frequency_esp32c3.git
git add .
git commit -m "Initial commit"
git branch -M main
git push -u origin main
```

### Q: 编译失败？
A: 检查：
- sdkconfig.defaults文件是否存在
- 网络连接是否正常
- 查看Actions页面的错误日志

### Q: USB CDC无法识别？
A: 
- Windows 10/11通常自动识别
- 尝试更换USB线（确保支持数据传输）
- 查看设备管理器是否有未知设备
- 可能需要安装ESP-IDF提供的USB驱动

### Q: 如何恢复出厂设置？
A: 两种方式：
1. 长按GPIO9按键5秒以上
2. 15秒内连续上电10次

## 📚 更多信息

详细文档请查看 [README.md](file://e:\Espidf\high_frequency\README.md)

## ✨ 下一步

- 配置您的MQTT服务器地址
- 测试WiFi配网功能
- 验证PWM输出
- 通过USB CDC查看调试日志

祝您使用愉快！🎉
