# 固件下载使用说明

## 概述

本项目使用GitHub Actions在云端编译ESP32-C3固件，并自动生成merged.bin文件（包含bootloader、分区表和应用程序的完整固件）。

## 完整流程

### 1. 推送代码到GitHub

```bash
git add .
git commit -m "更新代码"
git push origin main
```

推送后，GitHub Actions会自动开始编译。

### 2. 等待编译完成

访问 `https://github.com/<你的用户名>/high_frequency_esp32c3/actions` 查看编译状态。

编译通常需要5-10分钟。

### 3. 下载固件

编译成功后，在项目根目录运行PowerShell脚本：

```powershell
.\download_firmware.ps1
```

脚本会：
- ✅ 自动检测GitHub仓库信息
- ✅ 查找最新成功编译的任务
- ✅ 下载merged.bin到 `downloaded_firmware` 目录

### 4. 烧录固件

使用esptool.py烧录：

```powershell
esptool.py --chip esp32c3 --port COM3 write_flash 0x0 downloaded_firmware\merged.bin
```

**注意：** 将COM3替换为您的实际串口号。

## 常见问题

### Q: 提示未安装GitHub CLI？

A: 安装GitHub CLI：
```powershell
winget install GitHub.cli
```

### Q: 如何查看可用的artifacts？

A: 访问GitHub Actions页面，点击具体的workflow运行，在底部可以看到所有artifacts。

### Q: merged.bin和单独的bin文件有什么区别？

A: merged.bin是完整的固件文件，包含了：
- bootloader (地址 0x0)
- partition table (地址 0x8000)
- 应用程序 (地址 0x10000)

使用merged.bin只需一条命令即可烧录所有组件，更加方便。

### Q: 编译失败怎么办？

A: 
1. 检查GitHub Actions页面的错误日志
2. 确认代码没有语法错误
3. 确认sdkconfig.defaults配置正确
4. 重新推送代码触发编译

## 技术细节

### GitHub Actions工作流程

1. **检出代码**: 使用actions/checkout@v3
2. **设置ESP-IDF环境**: 使用espressif/esp-idf-ci-action@v1
3. **编译项目**: idf.py build
4. **合并固件**: esptool merge_bin 生成merged.bin
5. **上传产物**: 使用actions/upload-artifact@v4上传多个artifacts

### 生成的Artifacts

- **firmware-bin**: 所有bin文件
- **firmware-flash-package**: 打包的zip文件
- **merged-firmware**: merged.bin单独文件（推荐使用）

### 下载脚本功能

`download_firmware.ps1` 脚本提供：
- 自动登录验证
- 智能仓库检测
- 成功编译任务查找
- 错误处理和友好提示
- 彩色输出显示

## 优势

✅ **无需本地编译环境**: 所有编译在GitHub云端完成  
✅ **一致性保证**: 每次编译环境完全相同  
✅ **版本追溯**: 每次编译都有记录可查  
✅ **简化流程**: 一键下载，一键烧录  
✅ **跨平台支持**: Windows/Mac/Linux均可使用  

## 下一步

下载并烧录固件后：
1. 连接ESP32-C3到电脑USB口
2. 通过USB CDC查看调试日志
3. 使用微信小程序进行AirKiss配网
4. 配置MQTT服务器地址
5. 测试PWM输出功能

详细使用说明请参考 [QUICK_START.md](QUICK_START.md)
