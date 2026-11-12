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
$qualityExe = "$BuildDir/tools/Release/ws_quality.exe"

foreach ($tool in @($exe, $audioExe, $perfExe, $mfExe, $qualityExe)) {
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

# P12 live-loop benchmark: the real product loop (world step + ecology + actors
# + camera + render + HUD) for a fixed wall-clock duration, with p50/p95/p99 and
# the per-render-path frame counters. This is the number the native-4K/30-FPS
# acceptance gate is about, distinct from the reference-scene micro-benchmark.
# It runs on the CPU path here so packaging works on any runner; the full
# native-4K GPU run is the Windows GUI workflow's gui-smoke artifact
# (evidence/gui/live/gpu-4k-live.txt).
New-Item -ItemType Directory -Force -Path evidence/live/bench | Out-Null
& $exe --live-bench 5 --live-no-settings --live-no-select --size 1280x720 `
    --live-bench-out evidence/live/bench | Tee-Object -FilePath evidence/live/bench.txt
if (-not (Select-String -Path evidence/live/bench.txt -Pattern "WS_LIVE_BENCH_OK" -Quiet)) { exit 1 }
if (-not (Test-Path evidence/live/bench/frames.csv)) { Write-Error "no live-bench frame evidence"; exit 1 }

& $exe --live-tour evidence/live/tour-a --size 1280x720 | Tee-Object -FilePath evidence/live/tour-a.txt
if (-not (Select-String -Path evidence/live/tour-a.txt -Pattern "LIVE_TOUR_OK" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a.txt -Pattern "saved=1 loaded=1 roundtrip=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "shadows_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "single_step_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "settings_menu_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "world_select_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "menu_click_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "camera_collision_verified=1" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/tour-a/manifest.txt -Pattern "tree_avoidance_verified=1" -Quiet)) { exit 1 }
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

# P11 world selection (docs/17 "Ana deneyim"): prove new / load / sample on the
# real build, then ship the evidence frames.
New-Item -ItemType Directory -Force -Path evidence/live/select | Out-Null
& $exe --live-select evidence/live/select --size 1280x720 | Tee-Object -FilePath evidence/live/select.txt
if (-not (Select-String -Path evidence/live/select.txt -Pattern "LIVE_SELECT_OK" -Quiet)) { exit 1 }
foreach ($k in @("can_load=1","sample_reproducible=1","load_restored=1","new_fresh=1","readonly=1")) {
    if (-not (Select-String -Path evidence/live/select/select-manifest.txt -Pattern $k -Quiet)) { exit 1 }
}
Get-ChildItem evidence/live/select/*.ppm | ForEach-Object {
    & tools/ppm2png.ps1 -In $_.FullName -Out ($_.FullName -replace '\.ppm$', '.png')
}

# Export (docs/17 "Dışa aktarma"): the live product writes a timestamped frame
# sequence plus the matching WAV. Prove it is reproducible across two runs, then
# ship a few exported frames and the exported soundscape.
New-Item -ItemType Directory -Force -Path evidence/live/export-a,evidence/live/export-b | Out-Null
& $exe --live-export evidence/live/export-a --size 1280x720 --live-export-seconds 1 | Tee-Object -FilePath evidence/live/export.txt
if (-not (Select-String -Path evidence/live/export.txt -Pattern "LIVE_EXPORT_OK" -Quiet)) { exit 1 }
if (-not (Select-String -Path evidence/live/export-a/export-manifest.txt -Pattern "audio_nonzero=1" -Quiet)) { exit 1 }
& $exe --live-export evidence/live/export-b --size 1280x720 --live-export-seconds 1 | Out-Null
Get-ChildItem evidence/live/export-a/frame-*.ppm | ForEach-Object {
    $b = Join-Path "evidence/live/export-b" $_.Name
    if ((Get-FileHash $_.FullName).Hash -ne (Get-FileHash $b).Hash) {
        Write-Error "live export $($_.Name) not reproducible"; exit 1
    }
}
if ((Get-FileHash evidence/live/export-a/soundscape.wav).Hash -ne (Get-FileHash evidence/live/export-b/soundscape.wav).Hash) {
    Write-Error "live export WAV not reproducible"; exit 1
}
# Convert a few exported frames to PNG for the package.
Get-ChildItem evidence/live/export-a/frame-*.ppm | Where-Object { $_.BaseName -match 'frame-000(00|10|20)$' } | ForEach-Object {
    & tools/ppm2png.ps1 -In $_.FullName -Out ($_.FullName -replace '\.ppm$', '.png')
}

# P9 soundscape: our own C17 synthesis, rendered from a real ecology run, so
# the preview ships a listenable WAV next to the frames.
& $audioExe "evidence/preview/soundscape.wav" 20260101 | Tee-Object -FilePath evidence/preview/audio.txt

# P10 performance evidence: hardware profile, CPU/GPU raster timing and
# reference-scene frame times.
& $perfExe --quick | Tee-Object -FilePath evidence/preview/perf.txt

# P12 product visual-quality checklist (docs/QUALITY-CHECKLIST.md): the six
# product reference scenes must pass the documented quality gate, not just a
# golden hash. The harness renders from the real catalog and reports metrics.
$qualityExe = "$BuildDir/tools/Release/ws_quality.exe"
if (-not (Test-Path $qualityExe)) { Write-Error "missing build output: $qualityExe"; exit 1 }
New-Item -ItemType Directory -Force -Path evidence/quality | Out-Null
& $qualityExe evidence/quality 20260101 | Tee-Object -FilePath evidence/quality/quality.txt
if (-not (Select-String -Path evidence/quality/quality.txt -Pattern "WS_QUALITY_OK" -Quiet)) { exit 1 }
# Every reference scene the harness ran must pass; the expected count is read
# from the harness's own "WS_QUALITY_OK scenes=N" report so it cannot go stale.
$qualityRaw = Get-Content evidence/quality/quality.txt -Raw
$qualityExpected = [int]([regex]::Match($qualityRaw, "WS_QUALITY_OK scenes=(\d+)").Groups[1].Value)
$qualityPassed = (Select-String -Path evidence/quality/quality.txt -Pattern "verdict=PASS" -AllMatches).Matches.Count
if ($qualityPassed -ne $qualityExpected) {
    Write-Error "quality: $qualityPassed/$qualityExpected scenes passed"; exit 1
}

# P12 product motion-quality checklist (docs/QUALITY-CHECKLIST.md): a still
# frame cannot cover motion, so the reference quadruped is driven through the
# gait controller and the run is gated on progress, speed, smoothness, balance,
# foot contact and energy.
$motionExe = "$BuildDir/tools/Release/ws_motion.exe"
if (-not (Test-Path $motionExe)) { Write-Error "missing build output: $motionExe"; exit 1 }
New-Item -ItemType Directory -Force -Path evidence/motion | Out-Null
& $motionExe evidence/motion 20260101 | Tee-Object -FilePath evidence/motion/motion.txt
if (-not (Select-String -Path evidence/motion/motion.txt -Pattern "WS_MOTION_OK" -Quiet)) { exit 1 }
# Every species the harness ran must pass. The expected count is read from the
# harness's own "WS_MOTION_OK scenes=N" report so this gate cannot go stale when
# the species set changes.
$motionRaw = Get-Content evidence/motion/motion.txt -Raw
$motionExpected = [int]([regex]::Match($motionRaw, "WS_MOTION_OK scenes=(\d+)").Groups[1].Value)
$motionPassed = (Select-String -Path evidence/motion/motion.txt -Pattern "verdict=PASS" -AllMatches).Matches.Count
if ($motionPassed -ne $motionExpected) {
    Write-Error "motion: $motionPassed/$motionExpected species passed"; exit 1
}

# P13 lateral balance and turning: the frontal-plane model is run straight and
# in a commanded turn for every species and gated on bounded sway/roll, steps
# taken, turn tracking and determinism.
$lateralExe = "$BuildDir/tools/Release/ws_lateral_check.exe"
if (-not (Test-Path $lateralExe)) { Write-Error "missing build output: $lateralExe"; exit 1 }
New-Item -ItemType Directory -Force -Path evidence/lateral | Out-Null
& $lateralExe evidence/lateral 20260101 | Tee-Object -FilePath evidence/lateral/lateral.txt
if (-not (Select-String -Path evidence/lateral/lateral.txt -Pattern "WS_LATERAL_OK" -Quiet)) { exit 1 }
$lateralRaw = Get-Content evidence/lateral/lateral.txt -Raw
$lateralExpected = [int]([regex]::Match($lateralRaw, "WS_LATERAL_OK scenes=(\d+)").Groups[1].Value)
$lateralPassed = (Select-String -Path evidence/lateral/lateral.txt -Pattern "verdict=PASS" -AllMatches).Matches.Count
if ($lateralPassed -ne $lateralExpected) {
    Write-Error "lateral: $lateralPassed/$lateralExpected species passed"; exit 1
}

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
Copy-Item evidence/live/headless.txt "$stage/live/"
Copy-Item evidence/live/bench.txt "$stage/live/"
New-Item -ItemType Directory -Force -Path "$stage/live/bench" | Out-Null
Copy-Item evidence/live/bench/frames.csv "$stage/live/bench/"
New-Item -ItemType Directory -Force -Path "$stage/select" | Out-Null
Copy-Item evidence/live/select/*.png "$stage/select/"
Copy-Item evidence/live/select/select-manifest.txt "$stage/select/"
New-Item -ItemType Directory -Force -Path "$stage/export" | Out-Null
Copy-Item evidence/live/export-a/frame-*.png "$stage/export/"
Copy-Item evidence/live/export-a/soundscape.wav "$stage/export/"
Copy-Item evidence/live/export-a/export-manifest.txt "$stage/export/"
New-Item -ItemType Directory -Force -Path "$stage/quality" | Out-Null
Copy-Item evidence/quality/quality.csv "$stage/quality/"
Copy-Item evidence/quality/quality.txt "$stage/quality/"
Copy-Item evidence/motion/motion.csv "$stage/quality/"
Copy-Item evidence/motion/motion.txt "$stage/quality/"
Copy-Item evidence/lateral/lateral.csv "$stage/quality/"
Copy-Item evidence/lateral/lateral.txt "$stage/quality/"
@(
    "Worldseed Engine preview (P12 - CPU render performansi ve kalite kapisi).",
    "Canli uygulama: worldseed.exe --live  (tam ekran: --fullscreen)",
    "Acilista dunya secim ekrani gelir: YENI dunya olustur, MEVCUT dunyayi yukle",
    "(kayit varsa) veya ORNEK dunyayi ac. Ok tuslari imleci tasir, Enter secer,",
    "M/Esc vazgecer; canli gorunumde M ile yeniden acilir. Bir satira fareyle",
    "tiklamak imleci tasir, vurgulanan satira ikinci tiklama satiri uygular. Ornek",
    "dunya sabit tohumla bit-ozdes uretilir; secim ekrani gozlemci salt-okunurdur.",
    "Kalici ayarlar: seciminiz worldseed.cfg dosyasina yazilir ve sonraki acilista geri yuklenir;",
    "pencerede O tusu etkilesimli ayar menusunu acar (up/down gez, left/right degistir, enter uygula,",
    "B varsayilanlar, O/Esc kapat). Menude fare ile secim de ayni sekilde calisir.",
    "Canli tuslar: O ayarlar, M dunya secimi, T yonlu golgeleri acar/kapatir (varsayilan kapali).",
    "Canli kanit (penceresiz): worldseed.exe --live-headless 3 --size 1280x720",
    "  cikti p50/p95/p99 kare zamanlarini icerir (yayin performans kapisi).",
    "Sahneler: worldseed.exe --list  (shadows: yonlu golge eslemesi)",
    "Kanit: preview/*.png (CPU referans renderer ciktilari), live/*.png (canli uygulama turu),",
    "select/*.png (dunya secimi), export/*.png + export/soundscape.wav (disa aktarma: zaman",
    "damgali kare dizisi ve eslesen ses), preview/perf.txt (donanim/olcum) ve manifest.txt (SHA-256).",
    "CPU referans renderer bit-ozdes kalarak hizlandirildi; golgeler opt-in ve eklemeli.",
    "GPU yalniz gercek kazanc oldugunda kullanilir; OpenCL cihazi yoksa CPU fallback calisir."
) | Set-Content -Path "$stage/README.txt"

# Evidence manifest: every shipped artefact with its size and SHA-256, generated
# by our own C17 tool (docs/12, docs/18).
$mfFiles = @("$stage/worldseed.exe")
$mfFiles += Get-ChildItem "$stage/preview/*.png" | ForEach-Object { $_.FullName }
$mfFiles += Get-ChildItem "$stage/live/*.png" | ForEach-Object { $_.FullName }
$mfFiles += "$stage/live/headless.txt"
$mfFiles += "$stage/live/bench.txt"
$mfFiles += "$stage/live/bench/frames.csv"
$mfFiles += Get-ChildItem "$stage/select/*.png" | ForEach-Object { $_.FullName }
$mfFiles += Get-ChildItem "$stage/export/*.png" | ForEach-Object { $_.FullName }
$mfFiles += "$stage/export/soundscape.wav"
$mfFiles += "$stage/preview/soundscape.wav"
$mfFiles += "$stage/preview/perf.txt"
$mfFiles += "$stage/quality/quality.csv"
$mfFiles += "$stage/quality/quality.txt"
& $mfExe --out "MANIFEST.txt" --version "$Version" @mfFiles
if ($LASTEXITCODE -ne 0) { exit 1 }
Copy-Item "MANIFEST.txt" "$stage/MANIFEST.txt"

Compress-Archive -Path "$stage/*" -DestinationPath "worldseed-windows-x64.zip" -Force
Get-FileHash worldseed-windows-x64.zip -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash)  worldseed-windows-x64.zip" } |
    Set-Content -Path worldseed-windows-x64.zip.sha256

Write-Output "PACKAGE_OK worldseed-windows-x64.zip"
