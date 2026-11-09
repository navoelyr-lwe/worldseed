# Fizibilite — C17 tiled CPU renderer

**Aşama bağı:** P2 (P0/P2 feasibility spike). **Karar kaydı:** ADR-001.

## Amaç

Kendi C17 CPU rasterizer'ımızın zorunlu referans/fallback yolu olarak çalıştığını, bağımlılık eklemeden ve deterministik biçimde kanıtlamak. Bu belge bir plan ve ilk prototip raporudur; görsel kalite iddiası taşımaz.

## Kapsam (spike)

1. Doğrusal RGBA8 framebuffer ve güvenli bellek yönetimi.
2. Tiled (karo tabanlı) üçgen rasterizasyonu, Gouraud interpolasyon.
3. Derinlik tamponu ile yakın-öncelik testi.
4. PPM çıktısı ve SHA-256 parmak izi (headless kanıt).
5. Aynı sahnenin farklı karo boyutlarında bit düzeyinde aynı sonucu vermesi.

## Uygulanan prototip (bu PR)

- `src/render/ws_framebuffer.{h,c}`: framebuffer, güvenli boyut çarpımı, PPM.
- `src/render/ws_raster.{h,c}`: tiled raster, derinlik testi, istatistik sayaçları.
- `tools/ws_smoke.c`: 320×240 referans kare üretir ve SHA-256 yazar.
- `tests/test_world.c`: karo determinizmi, derinlik reddi, sınır güvenliği testleri.

## Kanıt

- GCC ve Clang (Debug/Release, ASan/UBSan) derleme ve test geçişi: CI `linux-matrix`.
- `ws_smoke` çıktısı: `WS_SMOKE_OK` ve frame SHA-256.
- Karo boyutu 8 ve 16 için bit düzeyinde aynı çıktı (unit test).

## Sınırlar ve sonraki iş

- Prototip yalnız düz renkli üçgen çizer; malzeme, doku, ışık, gölge yok.
- PBR/HDR, gölge haritası, atmosfer, su, bitki LOD P2'de ayrı görevlerdir.
- Performans iddiası yok; p95 frame-time ölçümü gerçek renderer ile yapılır.

## Karar

CPU rasterizer yolu bağımlılık eklemeden uygulanabilir. Referans/fallback rolü korunur. Hiçbir görsel kalite veya FPS iddiası bu spike ile yapılmaz.
