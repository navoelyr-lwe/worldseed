# Worldseed Engine — Proje Ana Planı

**GitHub deposu:** `navoelyr-lwe/worldseed`  
**Ürün adı:** Worldseed Engine  
**Belge sürümü:** 0.2.0  
**Durum:** Tam plan arşivi `docs/plan-bundle/` üzerinden SHA-256 doğrulamasıyla geri açıldı (22 dosya) ve kanonik `docs/` belgeleri yerleştirildi. Plan denetimi yapıldı; çelişkiler ve eksik kabul eşikleri düzeltildi. C17/OpenCL/CPU renderer/varlık/golden-scene için feasibility planları ve prototipleri hazırlandı. CI, dal koruması ve izole Windows GUI runner düzeni kuruldu. İlk görev bir doküman/altyapı PR'ıdır; ürün release'i değildir.  
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
| `docs/CI-VE-DAL-KORUMASI.md` | CI, dal koruması ve izole Windows GUI runner düzeni |
| `docs/MILESTONE-BOARD.md` | Aşama durumları ve açık blokajlar |
| `docs/adr/README.md` | Mimari karar kayıtları dizini |
| `docs/adr/ADR-007-yetki-devri-ve-release.md` | Yetki devri ve release yayımlama |
| `docs/adr/ADR-008-varlik-ve-icerik-uretimi.md` | Varlık ve içerik üretimi (özgün C17 hattı) |
| `docs/adr/ADR-009-ci-dal-korumasi-ve-runner.md` | CI, dal koruması ve izole GUI runner politikası |
| `docs/adr/ADR-010-kaynak-kodu-lisansi.md` | Kaynak kodu lisansı ve içerik menşei kaydı |
| `docs/feasibility/FEASIBILITY-CPU-RENDERER.md` | C17 tiled CPU renderer fizibilitesi |
| `docs/feasibility/FEASIBILITY-OPENCL.md` | OpenCL 1.2 / OpenCL C 1.2 ve CPU fallback fizibilitesi |
| `docs/feasibility/FEASIBILITY-ASSET-PIPELINE.md` | Kendi C17 varlık üretim hattı planı |
| `docs/feasibility/FEASIBILITY-GOLDEN-SCENE.md` | Otomatik golden-scene kanıt planı |

## Yeni ve bağımsız depo

Bu depo yeni ve bağımsız projedir. Eski `worldseed-engine` deposu değiştirilmez ve eski prototip kodu otomatik taşınmaz. Eski prototip yalnızca fikir/deneyim referansıdır; yeni mimari ve kod bu plan üzerinden kurulur.

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
