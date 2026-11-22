# Build with portable Zig 0.13.0 and Python 3 (no game data is linked into this plugin).
param([string]$Zig = 'zig', [string]$Python = 'python', [switch]$RuntimeProbe)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    New-Item -ItemType Directory -Path 'out' -Force | Out-Null
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $PSScriptRoot 'out/zig-global-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $PSScriptRoot 'out/zig-local-cache'
    $compile = @('-target','arm-freestanding-eabihf','-mcpu=cortex_a15','-marm',
        '-mfloat-abi=hard','-mfpu=vfpv3-d16','-std=gnu11','-O2','-fPIC','-ffreestanding',
        '-fno-builtin','-ffp-contract=off','-fno-vectorize','-fno-slp-vectorize','-nostdlib','-fno-stack-protector',
        '-fvisibility=hidden','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
        '-Wall','-Wextra','-Wno-unused-function')
    if ($RuntimeProbe) { $compile += '-DPERSONAL_RUNTIME_PROBE' }
    & $Zig cc @compile -c src/plugin.c -o out/plugin.o
    if ($LASTEXITCODE) { throw 'Plugin compilation failed' }
    & $Zig cc -target arm-freestanding-eabihf -mcpu=cortex_a15 -marm -fPIC -nostdlib -c src/start.S -o out/start.o
    if ($LASTEXITCODE) { throw 'Start code compilation failed' }
    & $Zig cc -target arm-linux-musleabihf -mcpu=cortex_a15 -nostdlib -shared `
        '-Wl,-T,build/nro.ld' '-Wl,--hash-style=sysv' '-Wl,-z,max-page-size=0x1000' `
        '-Wl,-soname,personal_gathering.nro' out/start.o out/plugin.o -o out/personal_gathering.elf
    if ($LASTEXITCODE) { throw 'Plugin linking failed' }
    & $Python build/elf2nro.py out/personal_gathering.elf out/personal_gathering.nro
    if ($LASTEXITCODE) { throw 'NRO conversion failed' }
} finally { Pop-Location }
