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

**Durum:** Tamamlandı. Kanıt: `docs/P10-GPU-VE-PERFORMANS.md`, `docs/gpu/`
(`MODEL-KARTI-GPU-RASTER.md`, `MODEL-KARTI-ZAMANLAYICI.md`),
`src/gpu/ws_opencl.*`, `kernels/ws_raster.cl`, `src/platform/ws_threads.*`,
`ws_hardware.*`, `src/core/ws_sched.*`, `tools/ws_gpu_check.c`, `tools/ws_perf.c`,
`tests/test_gpu.c` (4 test). Kabul ölçütleri: aynı yönde CPU ve OpenCL C 1.2 kernel
tolerans içinde eşleşir (`diff_pixels=1/49152`, `max_channel_delta=1`); device-loss
sonrası backend kullanılamaz olur, GPU çağrısı temiz hata verir ve CPU görüntüsü
bit-özdeş kalır; GPU cihazı yokluğu CPU fallback'tir; zamanlayıcı seri/paralel sonucu
bit-özdeş tutar; 4K ve ölçekleme ölçümü `tools/ws_perf` ile alınır. GPU yalnız gerçek
kazanç olduğunda kullanılır (ADR-001).

Not: GPU teknik kararı ADR-001 ile sabittir. P0/P2 feasibility spike yalnız OpenCL 1.2 cihaz keşfi, OpenCL C 1.2 kernel derleme, pixel-buffer sunumu, CPU karşılaştırması ve fallback davranışını kanıtlar; başka API/dil seçimi yapmaz.

## P11 — Ürünleştirme ve ilk release

**Çıktı:** tam ekran uygulama, ayarlar, dünya seçimi, canlı inceleme, harita/istatistik, save/load, paket, sürüm notu ve evidence manifest.

**Kabul:** `00-URUN-SOZLESMESI.md` bölüm 8 release şartlarının tamamı.

## P12 — Render performansı ve ürün görsel kalitesi

**Çıktı:** mevcut C17 CPU renderer korunarak tam sahne iş yükünün optimize edilmesi; OpenCL C 1.2 tam sahne compute yolunun aşamalı entegrasyonu; bitki/mesh LOD ve culling; altı ürün referans sahnesinde görsel kalite protokolü.

**Kabul:** native 3840×2160 referans sahnelerde en az 10 dakikalık ölçüm boyunca p95 frame-time ≤33,3 ms; gerçek GPU cihazı/driver bilgisi ile ölçüm; CPU fallback doğruluğu ve performans raporu; geometri/malzeme/ışık/gölge/atmosfer/hareket kalite kontrol listesi. Yalnız OpenCL kernel testi veya golden hash geçişi tam sahne performans/görsel kalite kabulü değildir.

**Durum:** Devam ediyor. P12 için mevcut benchmark ve bilinen sınırlamalar `P12-RENDER-PERFORMANS-VE-KALITE.md` içindedir.

## P13 — Üç boyutlu tür biyomekaniği

**Çıktı:** düzlemsel prototipin yerine bağımsız sağ/sol uzuvları olan 3B beden, eklem, temas, denge ve tür-özel hareket kontrolü. Kızıl geyik ilk yüksek ayrıntılı referans türdür.

**Kabul:** yürüme/koşu/dönüş/yokuş/zemin senaryoları; 3B temas ve denge testleri; enerji maliyeti ve hareket kıyasları; veri kaynağı/belirsizlikleri belgeli D3 referans tür doğrulaması. Diğer üç türün beden ve gait parametreleri ayrı doğrulanır.

**Durum:** Başladı (ilk dilim). Frontal düzlem (lateral) denge ve dönüş modeli
dört tür için eklendi ve CI'da kapılanıyor
(`docs/life/MODEL-KARTI-FRONTAL-DENGE.md`, `src/life/ws_lateral.*`,
`tools/ws_lateral_check.c`). Tam 3B bağımsız uzuv state'i, temas kuvveti,
3B denge/enerji D3 kıyası ve tür-özel 3B gait doğrulaması **açık**.

## P14 — Çok duyulu algı ve doğal davranış

**Çıktı:** görüş/örtülme, işitme, koku, iç durum, hafıza, öğrenme, sosyal bilgi ve habitat etkilerini birleştiren tür-özel algı/karar sistemi.

**Kabul:** algı menzili ve çevre etkileri için testler; koku plume/rüzgâr, ses mesafe/engel/maskeleme ve görüş çizgisi; çoklu seed/mevsim/tehdit/kaynak senaryolarında davranış ölçümü; öğrenme iddiaları için nicel önce/sonra karşılaştırması.

## P15 — 2 km habitat mozaiği ve ekolojik döngüler

**Çıktı:** dört bağlı habitatta bitki, kaynak, hayvan, toprak ve mevsim süreçlerini ilişkilendiren ekosistem. Odak alanı yüksek ayrıntısı ile uzak alan aggregate düzeyleri açıkça tanımlanır.

**Kabul:** 2.048 m × 2.048 m referans haritada dört habitatın dağılımı ve bağlantısı; türe/bitkiye özgü kaynak ve yaşam döngüleri; geyik örtü-yem dengesi, tilki alternatif diyet, tavşan warren/sosyal grup ve yaban domuzu mevsimsel omnivori/rooting; çoklu seed ve çok mevsimli uzun koşu; popülasyon/biomass/enerji/habitat kullanım raporu; parametre duyarlılığı ve belirsizlik analizi. Saha verisi olmayan sonuçlar kalibre edilmiş tahmin olarak sunulmaz.

## P16 — Doğal ses manzarası ve mekânsal ses

**Çıktı:** olay-tonu prototipinden çevresel ses katmanları, tür/davranış sesleri, fiziksel yayılım ve Windows canlı ses aygıtı çıkışına geçiş.

**Kabul:** rüzgâr/yaprak, yağmur, su ve habitat ambiyansı; gerçek simülasyon olaylarından gelen tür/etkinlik sesleri; mesafe/yön/engel/zemin/atmosfer etkileri; canlı çıkış ve aygıt kaybı dayanıklılığı; deterministik WAV tekrar testi; akustik parametrelerin kaynak ve geçerlilik alanı.

## P17 — Doğal gözlem ve sinematik yönetmen

**Çıktı:** tek olay/tek konu yönetmeninden çok aktörlü, ortam ve sakin yaşam gözlemini de içeren salt-okunur sinematik yönetmene geçiş.

**Kabul:** av-avcı, grup, anne-yavru ve türler arası kadraj; olay dışı doğal çevre planları; geniş/orta/yakın/takip planları; kamera sürekliliği/örtülme/ivme sınırları; her çekimin gerçek olay/snapshot kimliği ve zamanıyla eşleşmesi; ses ve replay zamanının tutarlılığı.

## P18 — Entegrasyon ve V1.0 kabulü

**Çıktı:** bütün alt sistemlerin gerçek Windows ürününde birlikte çalıştığı ilk kararlı sürüm.

**Kabul:** `00-URUN-SOZLESMESI.md` bölüm 8'in tüm maddeleri; dört bağlı habitat; dört türde doğrulanmış tür-özel 3B beden/hareket; çalışan çoklu tür ekolojisi; kamera dışı ilerleme/kalıcılık/replay; doğal ses ve canlı çıkış; referans sahne görsel kalite protokolü; native 4K/30 FPS referans performansı; otomatik bilimsel/uzun süre/GUI/paket/temiz kurulum kapıları. Hiçbir eksik test atlanarak veya performans eşiği düşürülerek 1.0.0 yayımlanmaz.

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
