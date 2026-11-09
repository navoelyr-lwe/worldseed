# Fizibilite — Otomatik golden-scene kanıtı

**Aşama bağı:** P2'den itibaren. **Karar kaydı:** `12-TEST-KABUL-CI.md`.

## Amaç

Sabit, sürümlü test sahnelerini otomatik üretip görsel regresyonu ve teknik kalite metriklerini kanıtlamak; estetik kaliteyi tek başına "kanıtlanmış" saymamak.

## Sahne kaydı (her golden scene için)

- Scene ID, commit SHA, world seed, üretici sürümü.
- Çözünürlük, kamera parametreleri, ışık/atmosfer preset'i.
- Backend (CPU/GPU) ve kalite preset'i.
- Beklenen metrikler: geometri/üçgen sayısı, aktif canlı/bitki, renk aralığı.

## Metrikler

- Teknik: SSIM/PSNR/piksel farkı (tolerans sabit ve gerekçeli), frame SHA-256.
- Kapsam: renk dağılımı, derinlik histogramı, boş/bozuk kare tespiti.
- Performans: p50/p95/p99 frame-time (`11-PERFORMANS-DONANIM.md`).

## Prototip (bu PR)

- `ws_smoke` referans kareyi PPM olarak üretir ve SHA-256 yazar.
- CI bu çıktıyı artifact olarak saklar; aynı girdi için deterministik parmak izi sağlar.

## Kalite protokolü

1. Görsel regresyon teknik hataları yakalar (yanlış geometri, kayıp doku, backend farkı).
2. Estetik kalite ayrı bir kontrol listesi ve referans görsel hedeflerle değerlendirilir.
3. Hiçbir aşamada "görüntü var" tek başına kalite kanıtı sayılmaz.

## Sınırlar

- Bu spike yalnız deterministik kare üretimini ve parmak izini kanıtlar.
- Gerçek golden scene seti (orman, çayır, su, yakın plan, yağmur/sis, gece) P2/P3'te eklenir.
- SSIM/PSNR araçları kendi C17 araçlarımızla yazılır; harici kütüphane eklenmez.

## P2 uygulaması

`ws_golden` aracı sürümlü sahne setini (orbit, lowlight, spheres) PPM olarak üretip manifeste kare SHA-256, üçgen sayısı ve kapsam yazar; `ws_imgdiff` kendi C17 metriklerimizle (ortalama fark, PSNR, SSIM) referans kareye karşı tolerans kapısını uygular. Detay `docs/P2-RENDER-TEMELI.md` içindedir. Bu belge tarihsel fizibilite kaydıdır.
