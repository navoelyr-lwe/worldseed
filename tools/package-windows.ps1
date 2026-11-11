# Worldseed Engine — build the Windows x64 preview package.
#
# Release tooling only; it never ships inside the product. Runs on a Windows
# runner after the Release build. It renders the preview frames from the real
# app, drives the live tour, renders the soundscape, collects performance
# evidence, stages a portable folder, writes the evidence MANIFEST with our own
# C17 tool, and zips the result with a SHA-256 sidecar.
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$Version
)

$ErrorActionPreference = "Stop"

$exe       = "$BuildDir/src/app/Release/worldseed.exe"
$audioExe  = "$BuildDir/tools/Release/ws_audio.exe"
$perfExe   = "$BuildDir/tools/Release/ws_perf.exe"
$mfExe     = "$BuildDir/tools/Release/ws_manifest.exe"

foreach ($tool in @($exe, $audioExe, $perfExe, $mfExe)) {
    if (-not (Test-Path $tool)) { Write-Error "missing build output: $tool"; exit 1 }
}

New-Item -ItemType Directory -Force -Path evidence/preview | Out-Null
$scenes = "orbit","lowlight","spheres","materials","textured","sky","shadows","terrain_day","terrain_dusk","terrain_weather","terrain_physics","creature_walk","organism_forage","ecosystem","world_stream","director_shot","soundscape"
for ($i = 0; $i -lt $scenes.Count; $i++) {
    $s = $scenes[$i]
    & $exe --capture "evidence/preview/$s.ppm" --scene $i --size 1280x720
    & tools/ppm2png.ps1 -In "evidence/preview/$s.ppm" -Out "evidence/preview/$s.png"
}
& $exe --selftest --size 640x360 | Tee-Object -FilePath evidence/preview/selftest.txt

# P11 interactive application: drive the real live loop headless and a
# deterministic tour (view switches, selection, save/load, stats panel), then
# prove two tours agree frame-for-frame (the observer is read-only).
New-Item -ItemType Directory -Force -Path evidence/live/tour-a,evidence/live/tour-b | Out-Null
& $exe --live-headless 3 --size 1280x720 | Tee-Object -FilePath evidence/live/headless.txt
if (-not (Select-String -Path evidence/live/headless.txt -Pattern "LIVE_HEADLESS_OK" -Quiet)) { exit 1 }
& $exe --live-tour evidence/live/tour-a --size 1280x720 | Tee-Object -FilePath evidence/live/tour-a.txt
if (-not (Select-String -Path evidence/live/tour-a.txt -Pattern "LIVE_TOUR_OK" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a.txt -Pattern "saved=1 loaded=1 roundtrip=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "shadows_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "single_step_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "settings_menu_verified=1" -Quiet)) { exit 1 }
& $exe --live-tour evidence/live/tour-b --size 1280x720 | Out-Null
Get-ChildItem evidence/live/tour-a/step-*.ppm | ForEach-Object {
    $b = Join-Path "evidence/live/tour-b" $_.Name
    if ((Get-FileHash $_.FullName).Hash -ne (Get-FileHash $b).Hash) {
        Write-Error "live tour $($_.Name) not reproducible"; exit 1
    }
    & tools/ppm2png.ps1 -In $_.FullName -Out ($_.FullName -replace '\.ppm$', '.png')
}
# Distinct asset name: the release also ships the package MANIFEST.txt,
# and release asset names are case-insensitive.
Copy-Item evidence/live/tour-a/manifest.txt evidence/live/tour-a/tour-manifest.txt

# P9 soundscape: our own C17 synthesis, rendered from a real ecology run, so
# the preview ships a listenable WAV next to the frames.
& $audioExe "evidence/preview/soundscape.wav" 20260101 | Tee-Object -FilePath evidence/preview/audio.txt

# P10 performance evidence: hardware profile, CPU/GPU raster timing and
# reference-scene frame times.
& $perfExe --quick | Tee-Object -FilePath evidence/preview/perf.txt

$stage = "stage/worldseed"
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item $exe "$stage/worldseed.exe"
New-Item -ItemType Directory -Force -Path "$stage/preview" | Out-Null
Copy-Item evidence/preview/*.png "$stage/preview/"
Copy-Item evidence/preview/soundscape.wav "$stage/preview/"
Copy-Item evidence/preview/perf.txt "$stage/preview/"
New-Item -ItemType Directory -Force -Path "$stage/live" | Out-Null
Copy-Item evidence/live/tour-a/*.png "$stage/live/"
Copy-Item evidence/live/tour-a/manifest.txt "$stage/live/"
@(
    "Worldseed Engine preview (P11 - etkilesimli ayar menusu).",
    "Canli uygulama: worldseed.exe --live  (tam ekran: --fullscreen)",
    "Kalici ayarlar: seciminiz worldseed.cfg dosyasina yazilir ve sonraki acilista geri yuklenir;",
    "pencerede O tusu etkilesimli ayar menusunu acar (up/down gez, left/right degistir, enter uygula,",
    "B varsayilanlar, O/Esc kapat). Acik komut satiri bayragi dosyayi gecersiz kilar.",
    "Canli tuslar: O ayarlar, T yonlu golgeleri acar/kapatir (varsayilan kapali).",
    "Canli kanit (penceresiz): worldseed.exe --live-headless 3 --size 1280x720",
    "Sahneler: worldseed.exe --list  (shadows: yonlu golge eslemesi)",
    "Kanit: preview/*.png (CPU referans renderer ciktilari), live/*.png (canli uygulama turu),",
    "preview/perf.txt (donanim/olcum) ve manifest.txt (SHA-256) ile CI artefaktlari.",
    "CPU referans renderer bit-ozdes kalarak hizlandirildi; golgeler opt-in ve eklemeli.",
    "GPU yalniz gercek kazanc oldugunda kullanilir; OpenCL cihazi yoksa CPU fallback calisir."
) | Set-Content -Path "$stage/README.txt"

# Evidence manifest: every shipped artefact with its size and SHA-256, generated
# by our own C17 tool (docs/12, docs/18).
$mfFiles = @("$stage/worldseed.exe")
$mfFiles += Get-ChildItem "$stage/preview/*.png" | ForEach-Object { $_.FullName }
$mfFiles += Get-ChildItem "$stage/live/*.png" | ForEach-Object { $_.FullName }
$mfFiles += "$stage/preview/soundscape.wav"
$mfFiles += "$stage/preview/perf.txt"
& $mfExe --out "MANIFEST.txt" --version "$Version" @mfFiles
if ($LASTEXITCODE -ne 0) { exit 1 }
Copy-Item "MANIFEST.txt" "$stage/MANIFEST.txt"

Compress-Archive -Path "$stage/*" -DestinationPath "worldseed-windows-x64.zip" -Force
Get-FileHash worldseed-windows-x64.zip -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash)  worldseed-windows-x64.zip" } |
    Set-Content -Path worldseed-windows-x64.zip.sha256

Write-Output "PACKAGE_OK worldseed-windows-x64.zip"
