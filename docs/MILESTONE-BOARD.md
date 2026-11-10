# Milestone panosu

Bu pano `13-YOL-HARITASI.md` aşamalarının durumunu izler. Durum yalnız kanıtla ilerler: kod + otomatik test + gerçek çalışma kanıtı + (ilgiliyse) benchmark + doküman + bilinen sınırlama.

| Aşama | Kapsam | Durum | Kanıt |
|---|---|---|---|
| P0 | Ürün ve bilimsel temel | Tamamlanan denetim | `DENETIM-RAPORU.md`, ADR-007…010 |
| P1 | C17 motor çekirdeği | Tamamlandı (altyapı) | `docs/P1-MOTOR-CEKIRDEGI.md`, `src/core`, `src/world`, `tests/`, `ci.yml` |
| P2 | Render ve içerik temeli | Kullanıcıya görünür önizleme (sahne renderer, malzeme/ışık/doku, golden scene, Windows uygulaması) | `docs/P2-RENDER-TEMELI.md`, `src/render`, `src/app`, `tests/test_render.c`, `tests/golden/`, `v0.1.0-preview` |
| P3 | İlk doğal çevre | Tamamlandı (statik dünya; kullanıcıya görünür) | `docs/P3-ILK-DOGAL-CEVRE.md`, `src/world`, `src/core/ws_noise.c`, `src/render/ws_env_scene.c`, `src/render/ws_catalog.c`, `tests/test_world_env.c`, `tests/golden/` |
| P4 | Çevresel fizik | Çekirdek + sahne köprüsü tamamlandı (D2) | `docs/P4-ILK-CEVRESEL-FIZIK.md`, `docs/physics/`, `src/physics`, `src/render/ws_env_scene.c`, `tests/test_physics.c`, `tests/golden/terrain_physics.ppm` |
| P5 | Referans canlı biyomekaniği | Kullanıcıya görünür (artiküle beden, kapalı-çevrim yürüyüş kontrolcüsü, `creature_walk` sahnesi) | `docs/P5-REFERANS-CANLI-BIYOMEKANIGI.md`, `docs/life/`, `src/life`, `src/render/ws_env_scene.c`, `tests/test_life.c`, `tests/golden/creature_walk.ppm` |
| P6 | Organizma ve davranış | Kullanıcıya görünür (algı, homeostaz, fayda tabanlı davranış, sınırlı öğrenme, `organism_forage` sahnesi) | `docs/P6-ORGANIZMA-VE-DAVRANIS.md`, `docs/life/MODEL-KARTI-ALGI.md`, `MODEL-KARTI-HOMEOSTAZ.md`, `MODEL-KARTI-DAVRANIS.md`, `src/life/ws_perception.*`, `ws_organism.*`, `ws_agent.*`, `tests/test_agent.c`, `tests/golden/organism_forage.ppm` |
| P7 | Ekosistem ve temsilî türler | Kullanıcıya görünür (dört tür için fiziksel beden + türe özgü yürüyüş, popülasyon/üreme/ölüm, av-avcı, `ecosystem` sahnesi) | `docs/P7-EKOSISTEM.md`, `docs/life/MODEL-KARTI-EKOLOJI.md`, `docs/life/TUR-KARTI-*.md`, `src/life/ws_species.*`, `ws_ecology.*`, `tests/test_ecology.c`, `tests/golden/ecosystem.ppm` |
| P8 | Kalıcı ve çok ölçekli dünya | Planlandı | — |
| P9 | Sinematik yönetmen ve ses | Planlandı | — |
| P10 | GPU ve performans ölçekleme | Başlangıç (keşif) | `src/platform`, `FEASIBILITY-OPENCL.md` |
| P11 | Ürünleştirme ve ilk release | Planlandı | — |

## Açık blokajlar

- Windows GUI self-hosted runner (etiketler: `self-hosted, windows, x64, worldseed-gui`) kayıtlı ve çalışıyor. `gui-windows.yml` artık `main` push'unda ve aynı depodan gelen PR'larda (fork değil) otomatik tetiklenir; gerçek Win32 penceresi açılır, ekran görüntüsü alınır ve kanıt artefaktı yüklenir. İzole runner kanıtı: run 42717394314 başarılı (1536×864 gerçek pencere görüntüsü).
- Branch protection yönetici erişimi gerektirir (bkz. `CI-VE-DAL-KORUMASI.md`).

## Notlar

- İlk görev yalnız doküman/altyapı PR'ıdır; ürün release'i değildir.
- Kullanıcıya görünür ilk Windows paketi P2'den itibaren prerelease olarak yayımlanır.
- P5 kullanıcıya görünür `creature_walk` sahnesini ekledi: referans geyik kendi C17 fiziksel bedeni ve kapalı-çevrim yürüyüş kontrolcüsüyle yürür; kamera simülasyonun bıraktığı gerçek poza bakar. Model düzlemseldir (sagittal); lateral denge ve yaw torku sonraki dilimlerdedir.
- P6 kullanıcıya görünür `organism_forage` sahnesini ekledi: referans canlı dünya katmanlarından yerleştirilen besin/su kaynağını algılar, iç ihtiyaçlarına göre davranış seçer ve seçtiği kaynağa yürür; kamera kararın onu götürdüğü gerçek konuma bakar. Algı, homeostaz, fayda tabanlı davranış ve sınırlı öğrenme kendi C17 kodumuzdadır; davranış kameradan bağımsızdır.
- P2 ilk kullanıcıya görünür sürümü `v0.1.0-preview` olarak yayımlandı: `worldseed.exe` sabit referans sahnelerini CPU referans renderer'ıyla çizer; paket her sahnenin gerçek render çıktısını `preview/*.png` olarak içerir. Canlı simülasyon, fizik ve etkileşim henüz yoktur.
- P6 kullanıcıya görünür sürümü `v0.5.0-preview` olarak yayımlandı (P2→P5 için sırasıyla `v0.1.0`…`v0.4.0-preview`): paket `worldseed.exe` ve on iki sahnenin gerçek render çıktısını (`preview/*.png`, `organism_forage.png` dâhil) içerir; sha256 dosyası yayımlanır.
- P7 kullanıcıya görünür `ecosystem` sahnesini ekledi: dört temsilî tür (kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu) kendi C17 fiziksel bedenleri ve türe özgü yürüyüşleriyle çizilir; canlı başına popülasyon/enerji modeli, bitki üretimi, üreme/ölüm ve av-avcı eşleşmesi headless koşar; kamera hayatta kalan bireylerin simülasyonun bıraktığı gerçek konumlarına bakar. Ekoloji, dört tür ve besin ağı kendi C17 kodumuzdadır; sahne script edilmiş bir dizilim değildir.
