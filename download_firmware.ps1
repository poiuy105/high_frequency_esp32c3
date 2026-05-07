# ESP32-C3 固件下载脚本
# 用于从GitHub Actions下载编译好的merged.bin固件

param(
    [string]$RepoOwner = "",
    [string]$RepoName = "high_frequency_esp32c3",
    [string]$OutputDir = ".\downloaded_firmware"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "  ESP32-C3 固件下载工具" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# 检查是否安装了GitHub CLI
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Host "错误: 未检测到GitHub CLI (gh)" -ForegroundColor Red
    Write-Host "请先安装GitHub CLI: https://cli.github.com/" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Windows用户可以使用以下命令安装:" -ForegroundColor Yellow
    Write-Host "  winget install GitHub.cli" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

# 检查是否已登录GitHub
try {
    $authStatus = gh auth status 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "请先登录GitHub..." -ForegroundColor Yellow
        gh auth login
        Write-Host ""
    }
} catch {
    Write-Host "请先登录GitHub..." -ForegroundColor Yellow
    gh auth login
    Write-Host ""
}

# 如果未提供仓库所有者，尝试获取当前Git远程URL
if ([string]::IsNullOrEmpty($RepoOwner)) {
    try {
        $remoteUrl = git remote get-url origin 2>$null
        if ($remoteUrl -match "github.com[/:]([^/]+)/") {
            $RepoOwner = $matches[1]
            Write-Host "检测到仓库所有者: $RepoOwner" -ForegroundColor Green
        } else {
            Write-Host "无法从Git远程URL检测仓库所有者" -ForegroundColor Yellow
            $RepoOwner = Read-Host "请输入您的GitHub用户名"
        }
    } catch {
        Write-Host "未找到Git仓库或无法获取远程URL" -ForegroundColor Yellow
        $RepoOwner = Read-Host "请输入您的GitHub用户名"
    }
}

$fullRepo = "$RepoOwner/$RepoName"
Write-Host ""
Write-Host "正在检查仓库: $fullRepo" -ForegroundColor Cyan
Write-Host ""

# 检查最新的workflow运行状态
Write-Host "获取最新的workflow运行状态..." -ForegroundColor Cyan
try {
    $runs = gh run list --repo $fullRepo --limit 5 --json databaseId,status,conclusion,createdAt | ConvertFrom-Json
    
    if ($runs.Count -eq 0) {
        Write-Host "错误: 未找到任何workflow运行记录" -ForegroundColor Red
        Write-Host "请确保代码已推送到GitHub并触发了编译" -ForegroundColor Yellow
        exit 1
    }
    
    # 查找成功的运行
    $successfulRun = $runs | Where-Object { $_.conclusion -eq "success" } | Select-Object -First 1
    
    if (-not $successfulRun) {
        Write-Host "警告: 未找到成功完成的编译任务" -ForegroundColor Yellow
        Write-Host "最新的运行状态:" -ForegroundColor Yellow
        $runs | ForEach-Object {
            Write-Host "  - 状态: $($_.status), 结果: $($_.conclusion), 时间: $($_.createdAt)" -ForegroundColor Yellow
        }
        
        $choice = Read-Host "是否仍然尝试下载最新运行的产物？(y/n)"
        if ($choice -ne "y" -and $choice -ne "Y") {
            exit 0
        }
        $targetRun = $runs[0]
    } else {
        $targetRun = $successfulRun
        Write-Host "找到成功完成的编译任务 (ID: $($targetRun.databaseId))" -ForegroundColor Green
    }
} catch {
    Write-Host "错误: 无法获取workflow运行状态" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}

# 创建输出目录
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
    Write-Host "创建输出目录: $OutputDir" -ForegroundColor Green
}

# 下载merged firmware artifact
Write-Host ""
Write-Host "正在下载merged固件..." -ForegroundColor Cyan
try {
    # 先列出所有artifacts
    $artifacts = gh run view $targetRun.databaseId --repo $fullRepo --json artifacts | ConvertFrom-Json
    
    $mergedArtifact = $artifacts.artifacts | Where-Object { $_.name -eq "merged-firmware" } | Select-Object -First 1
    
    if (-not $mergedArtifact) {
        Write-Host "错误: 未找到merged-firmware artifact" -ForegroundColor Red
        Write-Host "可用的artifacts:" -ForegroundColor Yellow
        $artifacts.artifacts | ForEach-Object {
            Write-Host "  - $($_.name)" -ForegroundColor Yellow
        }
        exit 1
    }
    
    # 下载artifact
    Write-Host "下载artifact: $($mergedArtifact.name)" -ForegroundColor Cyan
    gh run download $targetRun.databaseId --repo $fullRepo --name "merged-firmware" --dir $OutputDir
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "=========================================" -ForegroundColor Green
        Write-Host "  下载完成！" -ForegroundColor Green
        Write-Host "=========================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "固件已保存到: $OutputDir\merged.bin" -ForegroundColor Green
        Write-Host ""
        Write-Host "您可以使用以下命令烧录固件:" -ForegroundColor Cyan
        Write-Host "esptool.py --chip esp32c3 --port COM3 write_flash 0x0 $OutputDir\merged.bin" -ForegroundColor White
        Write-Host ""
        Write-Host "注意: 请将COM3替换为您的实际串口号" -ForegroundColor Yellow
    } else {
        Write-Host "错误: 下载失败" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "错误: 下载过程中出现异常" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
