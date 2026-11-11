# Worldseed Engine — Proje Ana Planı

**GitHub deposu:** `navoelyr-lwe/worldseed`  
**Ürün adı:** Worldseed Engine  
**Belge sürümü:** 0.7.0  
**Durum:** Tam plan arşivi `docs/plan-bundle/` üzerinden SHA-256 doğrulamasıyla geri açıldı (22 dosya) ve kanonik `docs/` belgeleri yerleştirildi. Plan denetimi yapıldı; çelişkiler ve eksik kabul eşikleri düzeltildi. C17/OpenCL/CPU renderer/varlık/golden-scene için feasibility planları ve prototipleri hazırlandı. CI, dal koruması ve izole Windows GUI runner düzeni kuruldu. P1 C17 motor çekirdeği tamamlandı (arena/slot map/RNG/serialization/world context/event stream/snapshot + headless smoke ve benchmark); kanıtlar `docs/P1-MOTOR-CEKIRDEGI.md` içindedir. P2 render temeli kullanıcıya görünür ilk önizlemeye ulaştı: matematik (mat4/mat3), prosedürel mesh, sürümlü mesh/doku varlık formatları + doğrulayıcı, sahne sözleşmesi, referans CPU sahne renderer'ı (Lambert + Blinn-Phong, yarım-küre ambient, gradyan gökyüzü, sis, ton eşleme), görüntü metrikleri, otomatik golden-scene kanıtı ve `worldseed.exe` Windows uygulaması (Win32/GDI penceresinde CPU render sunumu); kanıtlar `docs/P2-RENDER-TEMELI.md` içindedir ve ilk Windows x64 önizlemesi `v0.1.0-preview` olarak yayımlandı. P3 ilk doğal çevreyi ekledi: kendi C17 kodumuzla deterministik prosedürel arazi (yükseklik/eğim/yüzey sınıfı), akarsu ağı ve su bütçesi, habitat haritası, bitki örtüsü yerleşimi ve güneş konumu/sıcaklık/rüzgâr/nem/yağış/sis içeren atmosfer modeli; bu katmanlar CPU referans renderer'ına bağlanır ve golden-scene kanıtı `terrain_day`, `terrain_dusk`, `terrain_weather` sahneleriyle genişletildi (kanıtlar `docs/P3-ILK-DOGAL-CEVRE.md` içindedir). P4 çevresel fizik çekirdeğini ekledi: kendi C17 kodumuzla yerçekimi, yarı-kapalı Euler entegratörü, yükseklik-alanı zemin teması, Coulomb sürtünmesi/restitüsyon, hava sürüklemesi ve kaldırma kuvveti, ve çevre modeline bağlı rüzgâr profili; `terrain_physics` golden sahnesi bir küre kayayı çözücüyle dinlenmeye bırakıp kamerayı gerçek dinlenme konumuna bakar (kanıtlar `docs/P4-ILK-CEVRESEL-FIZIK.md` ve `docs/physics/` model kartları içindedir). P5 referans canlı biyomekaniğini ekledi: kızıl geyik için kendi C17 artiküle bedeni (gövde zinciri, eklemler, kas-eşdeğeri motorlar, ayak temas sensörleri), kapalı-çevrim Raibert tarzı yürüyüş kontrolcüsü (düz/eğimli/engebeli zeminde ileri yürüyüş, dönüş komutu), hareket maliyeti (motor işi) ve `creature_walk` golden sahnesi; kanıtlar `docs/P5-REFERANS-CANLI-BIYOMEKANIGI.md` ve `docs/life/` model kartları içindedir. P6 organizma ve davranışı ekledi: kendi C17 algı modeli (menzil/görüş alanı, en yakın hedef), iç durum ve ihtiyaç homeostazı (enerji, hidrasyon, sıcaklık, yorgunluk, sağlık), fayda tabanlı davranış seçimi (dinlenme, besin/su arama, kaçma, gezinme), kısa olay hafızası ve uzamsal hafızayla sınırlı öğrenme ve `organism_forage` golden sahnesi; canlı dünya katmanlarından yerleştirilen kaynağı algılayıp seçtiği kaynağa yürür, kamera kararın onu götürdüğü gerçek konuma bakar; kanıtlar `docs/P6-ORGANIZMA-VE-DAVRANIS.md` ve yeni model kartları içindedir. P7 ekosistem ve temsilî türleri ekledi: kendi C17 kodumuzla dört temsilî tür (kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu) için ayrı fiziksel beden ve türe özgü yürüyüş, deterministik ajan tabanlı popülasyon/enerji modeli (bitki üretimi, otçul tüketimi, av-avcı enerji aktarımı, üreme, ölüm, popülasyon) ve `ecosystem` golden sahnesi; kamera hayatta kalan bireylerin simülasyonun bıraktığı gerçek konumlarına bakar; kanıtlar `docs/P7-EKOSISTEM.md`, `docs/life/MODEL-KARTI-EKOLOJI.md` ve `docs/life/TUR-KARTI-*.md` içindedir. P8 kalıcı ve çok ölçekli dünyayı ekledi: kendi C17 kodumuzla bölge geometrisi/kimliği, dünya host'u (odak çevresinde bağlı bölgelerin streaming'i, near/mid/far simülasyon seviyeleri ve her seviyenin kendi kadansı), kamera dışı dünya ilerlemesi, boşalt/yükle çevriminde durum korunumu, sıralı olay günlüğü (journal), sürümlü checkpoint kaydet/yükle ve SHA-256 özetli deterministik replay; uzak/orta seviye bölgeler için kendi toplam (aggregate) popülasyon vekili; ve `world_stream` golden sahnesi; kanıtlar `docs/P8-KALICI-DUNYA.md`, `docs/world/MODEL-KARTI-DUNYA-HOSTU.md` ve `docs/life/MODEL-KARTI-POPULASYON.md` içindedir. P9 sinematik yönetmen ve sesi ekledi: kendi C17 kodumuzla olay-tabanlı yönetmen (gerçek yaşam olaylarını gözler, çekim tipini planlar, kamerayı hız/ivme/dönüş sınırları içinde sürer, occlusion'da yalnız kamerayı yükseltir; dünyaya yazmaz) ve prosedürel ses manzarası (gerçek olaylar kendi C17 sinüs-osilatör sentezimizi tetikler, kayıtlı ses veya üçüncü taraf ses çalışma zamanı yoktur); `director_shot` ve `soundscape` golden sahneleri; `tools/ws_audio` gerçek WAV üretir ve determinizmi kanıtlar; kanıtlar `docs/P9-SINEMATIK-YONETMEN-VE-SES.md`, `docs/cinema/MODEL-KARTI-YONETMEN.md` ve `docs/audio/MODEL-KARTI-SES-MANZARASI.md` içindedir. P10 GPU ve performans ölçeklemeyi ekledi: donanım keşfi (`ws_hardware_probe`), platform thread katmanı (Win32/POSIX), deterministik çok çekirdekli iş zamanlayıcı (`ws_sched`), OpenCL 1.2 host (`ws_opencl`) ve OpenCL C 1.2 düz gölgeli rasterleme kernel'i (`kernels/ws_raster.cl`, derleme zamanında C string olarak gömülü, `-cl-std=CL1.2`); CPU referans rasterleyicisi ve GPU kernel'i aynı yığın üzerinde koşar ve tolerans içinde eşleşir (`diff_pixels=1/49152`, `max_channel_delta=1`), device-loss'ta backend kullanılamaz olur, GPU çağrısı temiz hata verir ve CPU görüntüsü bit-özdeş kalır, GPU cihazı yoksa CPU fallback hata üretmez; zamanlayıcı seri ve paralel sonucu bit-özdeş tutar (örnek `speedup=3.29x`, `identical=1`); GPU yalnız gerçek kazanç olduğunda kullanılır (ADR-001), bu makinedeki pocl CPU ICD yavaş olduğundan `verdict=use_cpu` çıkar; 4K/ölçekleme ölçümü `tools/ws_perf` ile alınır ve CI `opencl-cpu` işi pocl CPU ICD ile gerçek kernel derleme/çalıştırma kanıtı üretir; kanıtlar `docs/P10-GPU-VE-PERFORMANS.md` ve `docs/gpu/` model kartları içindedir. P11 ürünü kullanıcıya görünür canlı bir gözlem uygulamasına dönüştürdü: `worldseed.exe --live` (veya `--fullscreen`) pencerede gerçek zamanlı yaşayan dünyayı sunar; oturum P8 host, P7 odak ekosistemi, P9 yönetmen/ses ve renderer kamerasını sahiplenir ve gözlemci salt okunurdur. Görünüm kipleri (auto/free/follow/map), canlı inceleme, olay zaman çizelgesi, dünya haritası, istatistik paneli (demografi + popülasyon grafiği) ve kendi C17 bitmap fontumuzla çizilen HUD eklendi; Windows sunumu Win32/GDI ile yapılır (GDI render motoru değil). `--live-headless` gerçek kare zamanlaması ve dünya özeti, `--live-tour` ise görünüm/kayıt-yükle içeren deterministik bir tur çalıştırıp her adımda gerçek PPM kare + SHA-256 + manifest üretir ve iki tur kare-özdeş olur. Replay (`R`, `,`/`.`) kaydedilmiş gerçek dünya anlık görüntülerinden geçmişi yeniden gözlemler (`--live-replay` kanıt modu; en yeni örnek canlı dünyayı bit-özdeş yeniden üretir, çıkış canlı dünyayı bit-özdeş geri yükler). Tek adım (`.`) durakdayken dünyayı tam bir sabit tick ilerletir. `tools/ws_manifest` paket SHA-256 manifesti üretir. Açılışta dünya seçim ekranı gösterilir (`docs/17` "Ana deneyim"): yeni dünya oluştur, mevcut dünyayı yükle (kayıt varsa) veya örnek dünyayı aç; örnek dünya sabit tohumla bit-özdeş yeniden üretilir, yükleme checkpoint'i bit-özdeş geri yükler, seçim gözlemci salt-okunurdur (`--live-select` kanıt modu). Sürüm `0.17.0-preview`. Kanıtlar `docs/P11-URUNLESTIRME.md` içindedir. P12 render performansını ve görsel kaliteyi ele alır: CPU referans renderer'ı bit-özdeş kalacak şekilde 6.2x hızlandırıldı (başlıkta satır içi matematik yardımcıları, sRGB kodlama LUT'u, çok çekirdekli bant render; 1080p `terrain_day` 543.78 ms → ~88 ms, zamanlayıcı 3.22x, `identical=1`) ve ışığın bakış açısından ortografik derinlik haritasıyla yönlü gölge eşlemesi eklendi (3×3 PCF, yalnız doğrudan ışığı azaltır, opt-in; gölgeler kapalıyken çıktı bit-özdeş kalır). Yeni `shadows` golden sahnesi gölgeyi gösterir ve CI sahne listelerine eklendi; 18/18 test release ve ASan/UBSan altında geçer. Kanıtlar `docs/P12-RENDER-PERFORMANS-VE-KALITE.md` içindedir.
**Ana dil:** ISO C17; GPU compute için sabitlenmiş tek istisna OpenCL C 1.2 kernel dili.

> **Ürün ilkesi:** Dünya yaşar. Canlılar kendi fiziksel ve biyolojik kurallarıyla davranır. Sinematik gözlemci olayları yaratmaz; gerçekten yaşananları bulur ve gösterir.

## Plan belgeleri

Tam ürün planı 22 dosyadır. İçerik `docs/plan-bundle/RESTORE.md` yönergesiyle SHA-256 doğrulamalı biçimde geri açılır. Kanonik belgeler `docs/` altında tutulacaktır:

| Dosya | Bağlayıcı konu |
|---|---|
| `docs/00-URUN-SOZLESMESI.md` | Ürünün bitmiş hâli ve kapsam sözleşmesi |
| `docs/01-ILK-RELEASE-VE-KAPSAM.md` | İlk sürüm, sonraki nesiller ve kapsam dışı konular |
| `docs/02-BILIMSEL-MODELLEME.md` | Bilimsel model standardı ve doğrulama yöntemi |
| `docs/03-C17-MIMARI.md` | C17, modül sınırları, platform ve süreç mimarisi |
| `docs/04-DUNYA-VE-CEVRE.md` | Coğrafya, arazi, su, hava, bitki ve çevre |
| `docs/05-FIZIK-MOTORU.md` | Fizik ve sayısal yöntemler |
| `docs/06-CANLI-BIYOMEKANIK.md` | Canlı bedenleri, kas-iskelet ve hareket |
| `docs/07-BIYOLOJI-EKOLOJI.md` | Fizyoloji, davranış, öğrenme ve ekoloji |
| `docs/08-CIZICI-VE-VARLIK-HATTI.md` | Kendi 3D çizicimiz ve içerik/varlık üretimi |
| `docs/09-SINEMATIK-YONETMEN.md` | Gerçek olayları izleyen sinematik yönetmen |
| `docs/10-SES-KALICILIK-TEKRAR.md` | Ses, kayıt, tekrar oynatma ve kalıcılık |
| `docs/11-PERFORMANS-DONANIM.md` | CPU/GPU, çözünürlük ve performans bütçeleri |
| `docs/12-TEST-KABUL-CI.md` | Tam otomatik test, kanıt ve release kapıları |
| `docs/13-YOL-HARITASI.md` | Aşamalar, bağımlılıklar ve tamamlanma koşulları |
| `docs/14-OPENHANDS-CALISMA-KURALLARI.md` | OpenHands çalışma kuralları ve yetki sınırları |
| `docs/15-RISKLER-KARARLAR.md` | Sabit teknik kararlar ve riskler |
| `docs/16-ARASTIRMA-KAYNAKLARI.md` | Bilimsel ve teknik araştırma başlangıç kaynakları |
| `docs/17-KULLANICI-ARAYUZU.md` | Tam ekran gözlem, kamera ve kullanıcı akışları |
| `docs/18-GUVENLIK-LISANS-PAKET.md` | Dosya güvenliği, içerik lisansı ve Windows paketleme |
| `docs/PLAN-README.md` | Plan paketinin içindekiler ve belge önceliği |
| `docs/PLAN-MANIFEST.json` | Plan dosyalarının manifesti |
| `docs/PLAN-AGENTS-REFERENCE.md` | Plan paketindeki ajan yönergelerinin referans kopyası |

## Denetim, altyapı ve karar kayıtları

| Dosya | Konu |
|---|---|
| `docs/DENETIM-RAPORU.md` | Plan denetimi, çelişkiler ve yapılan düzeltmeler |
| `docs/P1-MOTOR-CEKIRDEGI.md` | P1 C17 motor çekirdeği durumu, modüller ve kanıtlar |
| `docs/P2-RENDER-TEMELI.md` | P2 sahne renderer, varlık hattı, golden scene durumu ve kanıtlar |
| `docs/P3-ILK-DOGAL-CEVRE.md` | P3 prosedürel arazi, su, habitat, bitki örtüsü ve atmosfer durumu ve kanıtlar |
| `docs/P4-ILK-CEVRESEL-FIZIK.md` | P4 çevresel fizik çekirdeği, sahne köprüsü ve kanıtlar |
| `docs/P5-REFERANS-CANLI-BIYOMEKANIGI.md` | P5 referans canlı biyomekaniği (artiküle beden, yürüyüş kontrolcüsü, sahne köprüsü) ve kanıtlar |
| `docs/P6-ORGANIZMA-VE-DAVRANIS.md` | P6 organizma ve davranış (algı, homeostaz, fayda tabanlı davranış, sınırlı öğrenme, sahne köprüsü) ve kanıtlar |
| `docs/P7-EKOSISTEM.md` | P7 ekosistem ve temsilî türler (popülasyon/enerji modeli, av-avcı, üreme/ölüm, `ecosystem` sahnesi) ve kanıtlar |
| `docs/P8-KALICI-DUNYA.md` | P8 kalıcı ve çok ölçekli dünya (bölge streaming, seviyeler, journal, checkpoint, replay, `world_stream` sahnesi) ve kanıtlar |
| `docs/P9-SINEMATIK-YONETMEN-VE-SES.md` | P9 sinematik yönetmen ve prosedürel ses (olay-tabanlı gözlemci, sınırlı kamera, ses manzarası, `director_shot`/`soundscape` sahneleri) ve kanıtlar |
| `docs/cinema/README.md` | P9 sinematik yönetmen model kartları dizini ve kanıt düzeyleri |
| `docs/cinema/MODEL-KARTI-YONETMEN.md` | Olay-tabanlı sinematik yönetmen model kartı |
| `docs/audio/README.md` | P9 ses manzarası model kartları dizini ve kanıt düzeyleri |
| `docs/audio/MODEL-KARTI-SES-MANZARASI.md` | Prosedürel ses manzarası model kartı |
| `docs/P10-GPU-VE-PERFORMANS.md` | P10 GPU ve performans ölçekleme (donanım keşfi, OpenCL C 1.2 kernel, CPU/GPU semantik karşılaştırma, device-loss fallback, zamanlayıcı, 4K ölçüm) ve kanıtlar |
| `docs/P11-URUNLESTIRME.md` | P11 ürünleştirme ve ilk release (canlı gözlem uygulaması, görünüm kipleri, HUD/inceleme/harita/istatistik, kalıcı ayarlar, replay, kaydet/yükle, deterministik tur, Windows paketleme, sürüm, kanıt manifesti) ve kanıtlar |
| `docs/P12-RENDER-PERFORMANS-VE-KALITE.md` | P12 render performansı (CPU render 6.2x hızlandırma, bit-özdeş) ve görsel kalite (yönlü gölge eşlemesi, `shadows` golden sahnesi) ve kanıtlar |
| `docs/gpu/README.md` | P10 GPU ve performans model kartları dizini ve kanıt düzeyleri |
| `docs/gpu/MODEL-KARTI-GPU-RASTER.md` | OpenCL C 1.2 düz gölgeli rasterleme kernel'i ve CPU semantik eşleşmesi model kartı |
| `docs/gpu/MODEL-KARTI-ZAMANLAYICI.md` | Çok çekirdekli iş zamanlayıcı (deterministik parallel-for) model kartı |
| `docs/life/README.md` | P5–P8 canlı, tür ve ekoloji model kartları dizini ve kanıt düzeyleri |
| `docs/life/MODEL-KARTI-REFERANS-TUR.md` | Referans tür (kızıl geyik) anatomi ve beden model kartı |
| `docs/life/MODEL-KARTI-YURUYUS-KONTROLU.md` | Raibert tarzı yürüyüş kontrolü model kartı |
| `docs/life/MODEL-KARTI-HAREKET-MALIYETI.md` | Hareket maliyeti (kas-eşdeğeri motor işi) model kartı |
| `docs/life/MODEL-KARTI-ALGI.md` | Duyusal algı model kartı (P6) |
| `docs/life/MODEL-KARTI-HOMEOSTAZ.md` | İç durum ve ihtiyaç homeostazı model kartı (P6) |
| `docs/life/MODEL-KARTI-DAVRANIS.md` | Fayda tabanlı davranış ve sınırlı öğrenme model kartı (P6) |
| `docs/life/MODEL-KARTI-EKOLOJI.md` | Ekosistem ve popülasyon dinamiği model kartı (P7) |
| `docs/life/MODEL-KARTI-POPULASYON.md` | Toplam bölge popülasyonu model kartı (P8) |
| `docs/life/TUR-KARTI-KIZIL-GEYIK.md` | Kızıl geyik tür kartı (P7) |
| `docs/life/TUR-KARTI-KIZIL-TILKI.md` | Kızıl tilki tür kartı (P7) |
| `docs/life/TUR-KARTI-AVRUPA-TAVSANI.md` | Avrupa tavşanı tür kartı (P7) |
| `docs/life/TUR-KARTI-YABAN-DOMUZU.md` | Yaban domuzu tür kartı (P7) |
| `docs/physics/README.md` | P4 fizik model kartları dizini ve kanıt düzeyleri |
| `docs/physics/MODEL-KARTI-RIJIT-CISIM.md` | Rijit cisim dinamiği ve entegratör model kartı |
| `docs/physics/MODEL-KARTI-TEMAS-SURTUNME.md` | Zemin teması, sürtünme ve restitüsyon model kartı |
| `docs/physics/MODEL-KARTI-RUZGAR-VE-ORTAM.md` | Rüzgâr, hava sürüklemesi ve kaldırma kuvveti model kartı |
| `docs/world/README.md` | P8 dünya host'u model kartları dizini ve kanıt düzeyleri |
| `docs/world/MODEL-KARTI-DUNYA-HOSTU.md` | Dünya host'u (streaming, seviyeler, journal, checkpoint, replay) model kartı (P8) |
| `docs/CI-VE-DAL-KORUMASI.md` | CI, dal koruması ve izole Windows GUI runner düzeni |
| `docs/MILESTONE-BOARD.md` | Aşama durumları ve açık blokajlar |
| `docs/adr/README.md` | Mimari karar kayıtları dizini |
| `docs/adr/ADR-007-yetki-devri-ve-release.md` | Yetki devri ve release yayımlama |
| `docs/adr/ADR-008-varlik-ve-icerik-uretimi.md` | Varlık ve içerik üretimi (özgün C17 hattı) |
| `docs/adr/ADR-009-ci-dal-korumasi-ve-runner.md` | CI, dal koruması ve izole GUI runner politikası |
| `docs/adr/ADR-010-kaynak-kodu-lisansi.md` | Kaynak kodu lisansı ve içerik menşei kaydı |
| `docs/adr/ADR-011-render-ve-varlik-formati.md` | Sahne sözleşmesi, CPU renderer ve ilk varlık formatı |
| `docs/feasibility/FEASIBILITY-CPU-RENDERER.md` | C17 tiled CPU renderer fizibilitesi |
| `docs/feasibility/FEASIBILITY-OPENCL.md` | OpenCL 1.2 / OpenCL C 1.2 ve CPU fallback fizibilitesi |
| `docs/feasibility/FEASIBILITY-ASSET-PIPELINE.md` | Kendi C17 varlık üretim hattı planı |
| `docs/feasibility/FEASIBILITY-GOLDEN-SCENE.md` | Otomatik golden-scene kanıt planı |

## Yeni ve bağımsız depo

Bu depo yeni ve bağımsız projedir. Eski `worldseed-engine` deposu değiştirilmez ve eski prototip kodu otomatik taşınmaz. Eski prototip yalnızca fikir/deneyim referansıdır; yeni mimari ve kod bu plan üzerinden kurulur.

## Uygulama ve kullanım (P11 önizlemesi)

`src/app` altındaki uygulama (Windows'ta `worldseed.exe`) iki modda çalışır:

**Canlı gözlem uygulaması (P11):** yaşayan dünyayı gerçek zamanlı sunar.

- `worldseed --live` — pencere modunda canlı dünya. Açılışta dünya seçim ekranı
  gösterilir: yeni dünya oluştur, mevcut dünyayı yükle veya örnek dünyayı aç
  (`docs/17` "Ana deneyim"); ok tuşları gezer, `Enter` açar, `M`/`Esc` vazgeçer.
- `worldseed --live --fullscreen` — tam ekran canlı dünya.
- Tuşlar: `Space` duraklat, `+`/`-` hız, `V` görünüm (auto/free/follow/map), `N`/`P`
  canlı seç, `G` istatistik, `O` ayarlar, `M` dünya seçimi, `T` yönlü gölge, `H` yardım,
  `[`/`]` saat, `S`/`L` kaydet/yükle, ok/`WASD` serbest kamera, `F11` tam ekran, `Esc` çıkış.
- Penceresiz kanıt (her makinede çalışır): `worldseed --live-headless 3 --size 1280x720`
  (kare zamanlaması + dünya özeti), `worldseed --live-tour cikti --size 1280x720`
  (deterministik tur: görünüm değiştir, canlı seç, kaydet→yükle; her adımda PPM kare + manifest)
  ve `worldseed --live-select cikti --size 1280x720` (dünya seçimi: yeni/örnek/yükle).
- `worldseed --version` — sürüm.

**Sabit referans sahneleri (P2):** CPU referans renderer'ının gösterimi.

- `worldseed --list` — sahne listesi.
- `worldseed --scene 5 --size 1280x720` — pencereyi açıp seçilen sahneyi gösterir.
- `worldseed --selftest` — tüm sahneleri render eder ve doğrular (`APP_SELFTEST_OK`).
- `worldseed --capture cikti.ppm --scene 0 --size 320x240` — kareyi PPM olarak yazar.
- `worldseed --headless --scene 0` — pencere açmadan render eder.

Windows'ta gerçek bir Win32 penceresi açılıp kare GDI ile sunulur (GDI yalnız sunum);
diğer platformlarda ve CI'da penceresiz çalışır.

## Öncelik sırası

1. `docs/00-URUN-SOZLESMESI.md` ürünün ne olduğunu tanımlar.
2. `docs/02-BILIMSEL-MODELLEME.md` bilimsel iddiaların nasıl sınanacağını tanımlar.
3. `docs/03-C17-MIMARI.md` ve `docs/15-RISKLER-KARARLAR.md` teknoloji kararlarını sınırlar.
4. `docs/13-YOL-HARITASI.md` geliştirme sırasını ve aşama kapılarını tanımlar.
5. Diğer belgeler ilgili alt sistemin teknik sözleşmesidir.

Çelişki durumunda önce ürün sözleşmesi, sonra bilimsel model standardı ve mimari karar kayıtları geçerlidir. OpenHands çelişkiyi kendi başına sessizce çözemez; ADR/PR üretir ve belgeleri günceller.

## Gerçekçi ürün vaadi

Bu plan, **bilimsel temelli ve kendi içinde tutarlı, çok ölçekli bir dijital ekosistem** hedefler. Dünya üzerindeki her atomun, molekülün, hücrenin ve bütün gezegenin aynı anda en yüksek ayrıntıda simüle edilmesi hedeflenmez; bu sonlu donanımda gerçekçi değildir. Her süreç, amacına uygun mekânsal ve zamansal ölçekte modellenir. Model varsayımları, hata sınırları ve doğrulama kanıtları açık tutulur.

4K/30 FPS, sabitlenen referans sinematik donanım profilinde doğrulanacak hedeftir; tüm bilgisayarlarda garanti değildir. Daha mütevazı donanımlar için 1080p/30 FPS dengeli profil, üst seviye donanım için 4K/60 FPS hedeflenir. Gerçek ölçüm olmadan başarı iddiası verilmez.

## Sabit teknik kararlar

- ISO C17: motor, UI, araçlar, testler ve ürün mantığı.
- OpenCL C 1.2: tek izinli GPU kernel dili; OpenCL 1.2 uyumlu compute API.
- Kendi C17 CPU renderer zorunlu referans/fallback; OpenCL compute GPU backend isteğe bağlı hızlandırma.
- Üçüncü taraf oyun/render/fizik/AI/audio runtime veya hazır mesh/texture/rig/animation/audio asset paketi yok; ilk release içerikleri kendi C17 varlık üretim hattımızla hazırlanır.
- C++, CUDA, GLSL, HLSL, SPIR-V, Vulkan/OpenGL/Direct3D render pipeline kullanılmaz.
- Win32/GDI yalnızca pencere ve tamamlanmış piksel tamponunun sunumu içindir.
- Dört ilk tür: kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu.
- İlk ekosistem: yaklaşık 2×2 km ılıman karma orman/orman-çayır mozaiği, dört bağlantılı habitat.
- Referans sinematik profil: 8 çekirdek sınıfı CPU, 32 GB RAM, 12 GB VRAM; native 4K/30 FPS hedefi. Temel profil: 1080p/30 FPS hedefi.
- Teknik sorular kullanıcıya yöneltilmez; OpenHands sabit kararları uygular.
- OpenHands tüm zorunlu yayın kapıları geçince milestone prerelease ve V1 tamamlandığında 1.0.0 stable yayımlar; her release için ayrıca kullanıcı onayı istenmez.
- Tüm ilk release varlıkları kendi C17 içerik hattımızla üretilir; yalnızca kaynak/lisansı belgelenmiş bilimsel ölçüm ve parametre verileri referans alınabilir.

## İlk OpenHands görevi

Root `AGENTS.md` ve `docs/plan-bundle/RESTORE.md` bağlayıcıdır. İlk görev:
1. Tam plan arşivini geri aç ve SHA-256 değerini doğrula.
2. Kanonik belgeleri `docs/` altına yerleştir.
3. Plan, bilimsel iddialar, C17/GPU kararı ve aşama bağımlılıklarında eksik/tutarsızlık denetimi yap.
4. CI, branch protection, PR güvenliği ve Windows GUI runner düzenini tasarla/kur.
5. Tek bir dokümantasyon/altyapı PR'ı aç.
6. Bu ilk görevde ürün özellikleri kodlama; PR birleştikten sonra P1'e geç.

Kullanıcıya teknik soru sorma; sabit kararları uygula. Her görünür ilerleme ve tamamlanma raporu Türkçe, sade ve aşağıdaki dört başlıkla verilir:
1. Ne yaptık?
2. Ne sonuç aldık?
3. Şimdi ne yapacağız?
4. Benden bir şey gerekiyor mu?
