# P1 — C17 motor çekirdeği (durum ve kanıt)

Bu belge `13-YOL-HARITASI.md` P1 aşamasının çıktılarını, kabul kapılarını ve kanıtlarını kaydeder. P1 henüz kullanıcıya görünür bir ürün değildir; yalnız motor çekirdeği ve altyapıdır.

## P1 çıktıları

- Build/platform katmanı (CMake, GCC/Clang/MSVC, sanitizer, uyarı-sıfır).
- Bellek: arena (bump) ayırıcı ve taşma kontrollü boyut çarpımı.
- Kimlik: generation-kontrollü handle ve slot map.
- Koordinat/matematik: `ws_vec2`, `ws_vec3`, `ws_aabb3` ve temel işlemler.
- Zaman: otorite sabit adımı (`WS_WORLD_FIXED_DT`) ve monotonic saat adaptörü.
- RNG: sürümlenmiş, akış kimlikli, deterministik SplitMix64 tabanlı üreteç.
- World context: tek otorite nesnesi; entity kaydı, olay akışı ve dünya RNG akışı.
- Event stream: sıralı, sabit yük boyutlu, halka tampon; taşma sayacı.
- Snapshot API: salt-okunur dünya görüntüsü ve artımlı olay okuma.
- Serialization: açık little-endian yazıcı/okuyucu (ham struct yok).
- Headless executable: `ws_smoke` (referans kare + kanıt) ve `ws_bench` (ölçüm).

## Modül haritası

| Dizin | Sorumluluk |
|---|---|
| `src/core` | sonuç kodları, matematik, handle, RNG, SHA-256, arena, slot map, log, serialization |
| `src/world` | world context, entity kaydı, olay akışı, snapshot |
| `src/render` | framebuffer, tiled CPU rasterizer |
| `src/platform` | dinamik kütüphane adaptörü, OpenCL keşfi, saat |
| `tools` | headless smoke ve benchmark araçları |
| `tests` | birim, davranış, determinizm ve fuzz testleri |

## Mimari kuralların uygulanması

- **Tek otorite:** `ws_world` dünya state'ini sahiplenir; gözlem katmanı yalnız `ws_snapshot` üzerinden okur ve dünyaya yazamaz.
- **Handle kimliği:** Uzun ömürlü referanslar pointer değil `ws_handle` (index + generation) kullanır; serbest bırakılan handle bir daha çözülemez.
- **Açık serileştirme:** Diske ham C struct yazılmaz; tüm alanlar `ws_writer`/`ws_reader` ile açık little-endian yazılır.
- **Taşma güvenliği:** Boyut çarpımları `ws_mul_size_checked` ile denetlenir; arena ve slot map sınır dışına yazmaz.
- **Global yok:** Motor kodunda genel değişken kullanılmaz; tüm state çağıran sahipliğindedir.
- **Platform sınırı:** Ortak çekirdek platform başlığı içermez; OS erişimi `src/platform` adaptörlerindedir.

## Kabul kapıları ve kanıtlar

| Kapı | Kanıt |
|---|---|
| GCC/Clang/MSVC derleme | `ci.yml` linux matrisi + windows-msvc işi |
| Sanitizer | ASan/UBSan `-fno-sanitize-recover=all` ile Debug işleri |
| Deterministik seed | `test_world_determinism`, `test_world_replay_identical`, `test_world_multi_seed` |
| Sınır/fuzz | `test_fuzz` (rastgele/kesik byte akışı, güvenlik kontrolleri) |
| Temiz headless smoke | `ws_smoke` → `WS_SMOKE_OK` + deterministik kare SHA-256 |
| Benchmark | `ws_bench` → `WS_BENCH_OK` + adım/RNG/raster ölçümleri |

## Ölçüm örnekleri (geliştirme makinesi, referans değil)

- world step: ~1,3×10⁸ adım/s
- RNG: ~1,3×10⁸ çekiliş/s
- Raster: 640×480 tek üçgen ~59 FPS, 1920×1080 tek üçgen ~8,6 FPS (optimize edilmemiş başlangıç)

Bu değerler yalnız temel çizgiyi kaydeder; ürün performans kapısı değildir ve GPU/çok çekirdek iş yoktur.

## Bilinen sınırlamalar

- Simülasyon alt sistemleri (fizik, canlı, ekoloji) henüz yok; `ws_world_step` yalnız zaman çizgisini ilerletir.
- Snapshot şu an sayaç düzeyindedir; render için sahne verisi P2'de eklenir.
- Kalıcılık (checkpoint/replay dosyası) P8'de; serialization katmanı bu adımın temelidir.
- CPU rasterizer optimize edilmemiştir; P2'de düzeltilir.
