$ErrorActionPreference = 'Stop'
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ tools were not found' }
Import-Module "$installation/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
$env:PATH = "C:/Program Files/LLVM/bin;$env:PATH"
foreach ($name in @('PATH', 'INCLUDE', 'LIB', 'LIBPATH', 'VSINSTALLDIR', 'VCToolsInstallDir', 'WindowsSdkDir', 'WindowsSDKVersion')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value) { "$name=$value" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8 }
}
"VCPKG_ROOT=$env:GITHUB_WORKSPACE/build/deps/vcpkg" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
"VCPKG_BINARY_SOURCES=clear;files,$env:GITHUB_WORKSPACE/.deps/binary-cache,readwrite" | Out-File -FilePath $env:GITHUB_ENV -Append -Encoding utf8
New-Item -ItemType Directory -Force "$env:GITHUB_WORKSPACE/.deps/binary-cache" | Out-Null
clang-cl --version
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
