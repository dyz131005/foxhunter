# 银狐木马专杀工具编译脚本
# 使用G++全静态链接编译

$ErrorActionPreference = "Stop"

$OutputExe = "银狐木马专杀工具.exe"
$SourceFiles = @("src/main.cpp", "src/App.cpp")
$ResourceFile = "src/resource.rc"
$IncludeDir = "include"
$Libs = @("shell32", "user32", "advapi32", "comctl32", "shlwapi", "gdi32")

Write-Host "开始编译 $OutputExe ..." -ForegroundColor Green

# 检查G++是否存在
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gpp) {
    Write-Error "未找到 g++ 编译器，请确保已安装 MinGW 并添加到 PATH 环境变量"
    exit 1
}

# 检查windres是否存在
$windres = Get-Command windres -ErrorAction SilentlyContinue
if (-not $windres) {
    Write-Error "未找到 windres 资源编译器，请确保已安装 MinGW 并添加到 PATH 环境变量"
    exit 1
}

Write-Host "G++ 版本:" -ForegroundColor Cyan
g++ --version | Select-Object -First 1

Write-Host "windres 版本:" -ForegroundColor Cyan
windres --version | Select-Object -First 1

# 编译资源文件
Write-Host "编译资源文件..." -ForegroundColor Yellow
$projectDir = Get-Location
$resourcePath = Join-Path $projectDir $ResourceFile
& windres -i $resourcePath -o "src/resource.o" --include-dir=$IncludeDir --output-format=coff --input-format=rc

if ($LASTEXITCODE -ne 0) {
    Write-Error "资源文件编译失败，错误代码: $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 编译命令
$compileArgs = @(
    "-static",
    "-static-libgcc",
    "-static-libstdc++",
    "-DUNICODE",
    "-D_UNICODE",
    "-I", $IncludeDir,
    "-Wl,--no-gc-sections"
) + $SourceFiles + @(
    "src/resource.o",
    "-o", $OutputExe
) + ($Libs | ForEach-Object { "-l$_" }) + @(
    "-mwindows"
)

Write-Host "编译参数: $($compileArgs -join ' ')" -ForegroundColor Gray

& g++ $compileArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "编译成功!" -ForegroundColor Green
    Write-Host "输出文件: $OutputExe" -ForegroundColor Cyan

    $fileInfo = Get-Item $OutputExe -ErrorAction SilentlyContinue
    if ($fileInfo) {
        $sizeInMB = [math]::Round($fileInfo.Length / 1MB, 2)
        Write-Host "文件大小: $sizeInMB MB" -ForegroundColor Cyan
    }
} else {
    Write-Error "编译失败，错误代码: $LASTEXITCODE"
    exit $LASTEXITCODE
}