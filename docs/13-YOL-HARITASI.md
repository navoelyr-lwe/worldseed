# 13 — Aşamalı geliştirme yol haritası

Bu yol haritası takvim tahmini değildir. Her aşama bir öncekinin kabul kapısını geçmeden başlamaz; ancak tasarım/prototip araştırmaları bağımlı modüllerde paralel yürütülebilir. Her aşama ayrı task/PR'lara bölünür.

## P0 — Ürün ve bilimsel temel

**Çıktı:** tamamlanmış ürün sözleşmesi, model kartı standardı, teknoloji karar kayıtları, referans tür/habitat seçimi, benchmark planı ve görsel hedef seti.

**Kabul:** çelişki yok; C/GPU kararı açık; ilk release kapsamı, referans cihaz edinme yöntemi ve test planı tanımlı.

## P1 — C17 motor çekirdeği

**Çıktı:** build/platform katmanı, bellek/kimlik/koordinat/zaman/RNG, world context, event stream, snapshot API, headless executable.

**Kabul:** GCC/Clang/MSVC; sanitizer; deterministik seed; sınır/fuzz; temiz headless smoke.

## P2 — Render ve içerik temeli

**Çıktı:** CPU referans renderer, camera/geometry/material pipeline, frame capture, ilk C asset formatı/validator, golden scene harness.

**Kabul:** matematik/raster/derinlik testleri; sabit sahneler; CPU frame ve benchmark kanıtı. İlk görsel kalite prototipi.

## P3 — İlk doğal çevre

**Çıktı:** prosedürel terrain, habitat, su kaynakları, bitki yerleşimi, gökyüzü/gün-gece/atmosfer.

**Kabul:** aynı seed tekrarı; bölge sınırları; görsel referans sahne; çevresel durum testleri.

## P4 — Çevresel fizik

**Çıktı:** seçilmiş rijit cisim/temas, yüzey, rüzgâr ve su süreçleri.

**Kabul:** model kartları D2 veya hedeflenen D3; analitik/ölçüm kıyasları; uzun süre kararlılığı.

## P5 — Referans canlı biyomekaniği

**Çıktı:** tek referans tür; beden/eklem/kas eşdeğeri/denge/ayak teması/enerji maliyeti.

**Kabul:** tür kartı; referans hareket verisi; yürüme/koşu/dönüş/yokuş/zemin senaryoları; simülasyon ve görsel kanıt.

## P6 — Organizma ve davranış

**Çıktı:** algı, iç durum, ihtiyaç, hafıza, hedef, davranış ve sınırlı öğrenme.

**Kabul:** tek canlı headless davranış kabul deneyleri; kamera etkisizliği; çoklu seed; ölçülebilir öğrenme/karar raporu.

## P7 — Ekosistem ve temsilî türler

**Çıktı:** bitki/kaynak döngüsü, en az dört temsilî tür hedefi, besin ağı, etkileşim, yaşam döngüsü.

**Kabul:** çoklu tür testleri; kaynak/enerji dengesi; popülasyon deneyleri; uzun süreli headless evidence.

## P8 — Kalıcı ve çok ölçekli dünya

**Çıktı:** bağlı bölgeler, region streaming, near/mid/far simülasyon seviyeleri, checkpoint, event journal, replay.

**Kabul:** kamera dışı world progression; bölge unload/load; kaydet/yükle; replay karşılaştırması; uzun süre testi.

## P9 — Sinematik yönetmen ve ses

**Çıktı:** event-driven director, shot planner, tracking camera, free camera, soundscape, replay observation.

**Kabul:** yönetmen world state'i değiştiremez; sahte olay üretmez; gerçek event ID'leri çekim kaydına bağlı; kamera yumuşaklık/continuity testleri.

**Durum:** Tamamlandı (kullanıcıya görünür `director_shot` ve `soundscape`
sahneleri). Kanıt: `docs/P9-SINEMATIK-YONETMEN-VE-SES.md`,
`docs/cinema/MODEL-KARTI-YONETMEN.md`, `docs/audio/MODEL-KARTI-SES-MANZARASI.md`,
`src/cinema/ws_director.*`, `src/audio/ws_audio.*`, `src/life/ws_life_event.*`,
`tools/ws_audio.c`, `tests/test_cinema.c` (10 test), `tests/golden/director_shot.ppm`,
`tests/golden/soundscape.ppm`. Kabul ölçütleri: yönetmen dünyaya yazmaz
(yönetmenli/yönetmensiz koşu bit-özdeş olay özeti); yalnız gerçek olay kimliğine
bağlanır (sahte olay yok); kamera hız/ivme/dönüş sınırları ve kesmeler arası
süreklilik testleri geçer; ses olay-tabanlı ve deterministiktir.

## P10 — GPU ve performans ölçekleme

**Çıktı:** donanım keşfi, GPU compute/render spike sonucu, seçilmiş GPU backend, CPU fallback, multi-core work scheduler.

**Kabul:** aynı sahnede CPU/GPU semantic comparison; hata toleransları; GPU device loss recovery; 4K performance ölçümü.

Not: GPU teknik kararı ADR-001 ile sabittir. P0/P2 feasibility spike yalnız OpenCL 1.2 cihaz keşfi, OpenCL C 1.2 kernel derleme, pixel-buffer sunumu, CPU karşılaştırması ve fallback davranışını kanıtlar; başka API/dil seçimi yapmaz.

## P11 — Ürünleştirme ve ilk release

**Çıktı:** tam ekran uygulama, ayarlar, dünya seçimi, canlı inceleme, harita/istatistik, save/load, paket, sürüm notu ve evidence manifest.

**Kabul:** `00-URUN-SOZLESMESI.md` bölüm 8 release şartlarının tamamı.

## Bağımlılık prensibi

- Render ve sinematik prototip erken görünür olmalı; ancak fiziksel/biyolojik simülasyonla karıştırılmamalı.
- İlk referans canlı, diğer türlerden önce doğrulanmalı.
- Ekoloji birden fazla doğrulanmış organizma/bitki modeli olmadan tamamlandı sayılmaz.
- Director olgunlaşması gerçek event feed ve replay olmadan kabul edilmez.
- 4K release performansı GPU/asset/scene workload hazır olmadan vaat edilmez.


## Kullanıcıya teslim ve sürüm ritmi

- P0/P1 gibi henüz kullanıcıya anlamlı bir görüntü/işlev sunmayan altyapı aşamaları GitHub Actions artifact ve raporları üretir; bunlar ürün release'i diye adlandırılmaz.
- P2'den itibaren her kullanıcıya görünür milestone, aynı commit üzerinde Windows x64 paketleme + temiz ortam smoke test + tüm zorunlu CI + gerekli GUI/golden-scene kanıtlarından sonra prerelease olarak yayımlanır.
- Önerilen sürüm dizisi: `0.1.0-preview` (ilk çalışır görüntü), sonraki kullanıcıya görünür milestone'larda `0.2.0-preview`, `0.3.0-preview` ...; numara kapsam gerçekten hazır olduğunda artırılır.
- Her prerelease açıklaması sade Türkçe olarak nelerin geldiğini, nelerin henüz bulunmadığını, performans profilini, bilinen sınırlamaları ve kanıt bağlantılarını yazar.
- `1.0.0` yalnız ilk release sözleşmesinin bütün kapıları geçildiğinde yayımlanır.
