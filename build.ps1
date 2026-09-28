# build.ps1 - builds MSX Space Invaders ROMs with SDCC
# Output: bin\invaders.rom (full, MSX1/MSX2) and bin\invaders-cf2000.rom (lite)
param([switch]$Clean)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$src  = Join-Path $root "src"
$bin  = Join-Path $root "bin"
$obj  = Join-Path $root "obj"

if ($Clean) {
    if (Test-Path $obj) { Remove-Item -Recurse -Force $obj }
    if (Test-Path $bin) { Remove-Item -Recurse -Force $bin }
}
New-Item -ItemType Directory -Force $bin | Out-Null

# pick up a default SDCC install even if PATH was not refreshed
$sdccDefault = "C:\Program Files\SDCC\bin"
if (-not (Get-Command sdcc -ErrorAction SilentlyContinue) -and (Test-Path $sdccDefault)) {
    $env:Path += ";$sdccDefault"
}

foreach ($tool in "sdcc", "sdasz80", "makebin") {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found in PATH. Install SDCC 4.2+ (https://sdcc.sourceforge.net)."
    }
}

$modules = "vdp", "psg", "input", "data", "game"

function Build-Rom {
    param([string]$Name, [string[]]$Defines)

    $o = Join-Path $obj $Name
    New-Item -ItemType Directory -Force $o | Out-Null
    Write-Host "=== $Name ===" -ForegroundColor Cyan

    $crt0 = Join-Path $o "crt0.rel"
    & sdasz80 -g -o $crt0 (Join-Path $src "crt0.s")
    if ($LASTEXITCODE) { throw "sdasz80 failed" }

    $rels = @($crt0)
    foreach ($m in $modules) {
        $rel = Join-Path $o "$m.rel"
        & sdcc -mz80 -c --opt-code-speed @Defines -o $rel (Join-Path $src "$m.c")
        if ($LASTEXITCODE) { throw "sdcc failed on $m.c" }
        $rels += $rel
    }

    $ihx = Join-Path $o "$Name.ihx"
    & sdcc -mz80 --no-std-crt0 --code-loc 0x4010 --data-loc 0xC000 -o $ihx @rels
    if ($LASTEXITCODE) { throw "link failed" }

    $tmp = Join-Path $o "$Name.raw"
    & makebin -s 49152 $ihx $tmp
    if ($LASTEXITCODE) { throw "makebin failed" }

    $bytes = [IO.File]::ReadAllBytes($tmp)
    $rom = [byte[]]$bytes[16384..49151]                 # 0x4000..0xBFFF = 32KB

    # trim to 16KB if the upper page is unused (max compatibility);
    # makebin pads unused space with 0xFF
    $upperUsed = $false
    for ($i = 16384; $i -lt 32768; $i++) {
        if ($rom[$i] -ne 0 -and $rom[$i] -ne 0xFF) { $upperUsed = $true; break }
    }
    if (-not $upperUsed) { $rom = [byte[]]$rom[0..16383] }

    $out = Join-Path $bin "$Name.rom"
    [IO.File]::WriteAllBytes($out, $rom)
    Write-Host ("{0}: {1} KB" -f $out, ($rom.Length / 1024)) -ForegroundColor Green
}

Build-Rom -Name "invaders"        -Defines @()
Build-Rom -Name "invaders-cf2000" -Defines @("-DGAME_LITE")

Write-Host "Done." -ForegroundColor Green
