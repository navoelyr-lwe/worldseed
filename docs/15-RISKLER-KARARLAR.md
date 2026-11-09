# 15 — Sabit kararlar, riskler ve değişiklik yönetimi

Bu belge teknik soruları proje sahibine geri yöneltmek için değil, OpenHands'in uygulaması gereken kararları sabitlemek için vardır. Aşağıdaki kararlar 0.2 ürün planı için kapalıdır. OpenHands yeni kanıtla iyileştirme önerebilir; ancak kullanıcıya teknik soru sormaz ve kararları sessizce değiştirmez. Değişiklik önerisini gerekçesi/testiyle PR olarak sunar.

## ADR-001 — Dil ve GPU hesaplama (KAPALI)

**Karar:** Ürün ve motor kodu ISO C17. GPU compute API'si OpenCL 1.2 uyumlu alt kümesi; GPU kernel dili OpenCL C 1.2. OpenCL C, C99 tabanlı bir C lehçesidir ve ISO C17 ile aynı standart değildir; bu projede izin verilen tek GPU dili istisnasıdır.

**Yasak:** C++, CUDA, GLSL, HLSL, SPIR-V shader kaynakları, Vulkan/OpenGL/Direct3D render pipeline'ı, üçüncü taraf oyun/render/fizik/AI/ses runtime'ı.

**Uygulama:** CPU C17 yolu referans/fallback. GPU OpenCL compute öncelikle kendi raster/aydınlatma/görüntü işleme hattını hızlandırır. Dünya simülasyonunun otoritatif state'i CPU C17 tarafından belirlenir. GPU yoksa CPU'ya otomatik geçilir. OpenCL 2.x/3.x isteğe bağlı özellikleri zorunlu değildir.

**Gerekçe:** C tabanlı kernel dilini korurken farklı üreticilerin OpenCL uyumlu aygıtlarına erişmek; API/driver'ı sistemden almak ve ürün paketine oyun motoru/runtime eklememek. OpenCL 1.2 uyumluluk tabanı OpenCL 3.x cihazlarında da yaygın ortak taban sunar.

## ADR-002 — Varlık ve içerik stratejisi (KAPALI)

**Karar:** Motor kodu, asset compiler/validator, editör ve çalışma araçları C17'dir. Mesh, doku, rig, malzeme ve ses dosyaları içerik verisidir; programlama dili veya runtime dependency değildir.

İçerik kaynağı: İlk release'e giren bütün mesh, doku, materyal, rig, animasyon ve ses içerikleri kendi C17 varlık hattımızla özgün/prosedürel olarak üretilir veya proje içinde tanımlanır. Harici hazır model/texture/animation/audio asset paketi kullanılmaz. Harici executable, script, plugin, kütüphane, engine veya runtime kesinlikle yasaktır. Yalnızca kaynağı/lisansı belgelenmiş bilimsel parametre ve ölçüm verileri model kartlarında referans alınabilir; bunlar runtime varlık paketi değildir ve ürüne gömülmez. Varlıklar kendi sürümlü Worldseed paket biçimimize dönüştürülür. Bu karar kök `AGENTS.md`'deki kendi C17 varlık üretim ilkesiyle birebir aynıdır (bkz. ADR-008).

**Gerekçe:** C-only motor ve bağımsız runtime hedefini korurken sinematik kalite için gerekli içerik verisini tamamen özgün, üretilebilir ve denetlenebilir tutmak. Varlıkların kendi hattımızda üretilmesi hem lisans riskini ortadan kaldırır hem de içerik bütünlüğünü garanti eder.

## ADR-003 — İlk release ekosistemi ve türler (KAPALI)

**Karar:** Yaklaşık 2 km × 2 km referans dünya; ılıman karma orman/orman-çayır mozaiği; orman içi, çayır/açıklık, tatlı su kenarı, kayalık/yamaç habitatları. Dört tür: kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu. İlk release'te kuş uçuşu yoktur; uçan tür sonraki nesildir.

**Gerekçe:** Türler aynı iklim/ekosistem içinde birlikte bulunabilir; dört ayaklı memeli biyomekaniği ailesi kapsamı yönetilebilir tutarken farklı beden kütlesi, gait, diyet ve davranışları doğrular. Dört türün tamamı fiziksel hareket modeline sahip olur.

## ADR-004 — İlk referans donanım ve çözünürlük profilleri (KAPALI)

- Temel: 6 çekirdek sınıfı CPU, 16 GB RAM, 6 GB VRAM/OpenCL 1.2 GPU; 1080p/30 FPS dengeli hedef.
- Referans sinematik: 8 çekirdek sınıfı CPU, 32 GB RAM, 12 GB VRAM/OpenCL 1.2 GPU; golden scenes native 4K/30 FPS hedef.
- Üst seviye: 4K/60 FPS hedef.
- CPU-only fallback her zaman çalışır; 4K/30 FPS garantisi vermez.

Gerçek donanım modeli otomatik algılanır; kullanıcıdan teknik bilgi istenmez. Hedefler benchmark ile doğrulanır.

## ADR-005 — Dünya ve uzak alan simülasyonu (KAPALI)

Yakın/orta/uzak katmanlar kullanılır. Yakın: bireysel organizma ve ayrıntılı fizik; orta: basitleştirilmiş bireysel state ve davranış; uzak: tür/popülasyon, kaynak, enerji ve olay tabanlı toplulaştırılmış model. Katman geçişleri kütle/enerji/su/popülasyon/kimlik ve olay tutarlılığı testlerinden geçer. Görülmeyen alan silinmez.

## ADR-006 — Deterministiklik ve paralellik (KAPALI)

Bilimsel test ve replay için deterministik tek-yazar dünya state'i, sabit zaman adımları, sürümlü RNG stream ve sıralı event commit kullanılır. CPU paralelliği bağımsız işlerde deterministik reduction/order ile açılır. İlk sürümde GPU, otoritatif canlı/ekoloji kararlarını belirlemez; rendering/visual compute hızlandırır. Farklı cihazlar arası bit düzeyi simülasyon eşitliği vaat edilmez; aynı build/seed/makinede replay tutarlılığı şarttır.

## Kalan teknik riskler (karar değil, mühendislik işi)

| Risk | Etki | Azaltma / kanıt |
|---|---|---|
| 4K gerçek zamanlı kendi renderer'ımız | FPS hedefi tutmayabilir | Erken GPU compute spike, golden scene, p95 frame-time, profil ve optimizasyon |
| Yüksek kaliteli canlı/çevre varlıkları | Görüntü yapay/tekrarlı kalabilir | Golden asset set, anatomik referans, mesh/material/texture QA, lisans manifesti |
| Bilimsel modellerin belirsizliği | İnandırıcı ama yanlış dünya | Model kartı, D0-D4 kanıt seviyesi, referans veri ve belirsizlik açıklaması |
| Çok ölçekli simülasyon geçişi | Canlı/enerji/popülasyon sıçraması | Round-trip, korunum, unload/reload ve uzun süre testleri |
| GPU sürücü/özellik farklılığı | Bazı GPU'lar başarısız | OpenCL yetenek sorgulama, 1.2 baseline, CPU fallback, cihaz matrisi |
| Paralel hesapta tekrar üretilebilirlik | Replay farkı | Deterministik event commit, sabit seed ve replay diff |
| Self-hosted GUI güvenliği | Runner riski | Secretsiz, izole, disposable VM; allowlist; dış PR kodu çalıştırmama |
| Görsel estetik otomatik ölçümü | Teknik test estetiği kanıtlamaz | Golden scene + ölçüt listesi + bağımsız görsel evidence incelemesi; kullanıcı manuel testi yok |
| İçerik üretim kapasitesi | Özgün varlık hattı yavaş ilerleyebilir | Prosedürel üretimle başlama, sürümlü asset compiler/validator, `ASSET-MANIFEST.csv` ile üretici aracı/sürüm izi |

## Karar değiştirme yöntemi

Her değişiklik `docs/adr/ADR-NNN-*.md` olarak PR ile önerilir. PR; mevcut karar, yeni kanıt, alternatifler, performans/uyumluluk/lisans etkisi ve geriye dönük uyumluluk planını içerir. Ürün sahibine teknik tercih sorusu gönderilmez; OpenHands en güvenli ve C-only ilkesini koruyan seçeneği uygular. Ürün hedefini düşüren değişiklikler kabul edilmiş sayılmaz.
