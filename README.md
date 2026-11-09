# Worldseed Engine — Proje Ana Planı

**GitHub deposu:** `navoelyr-lwe/worldseed`  
**Ürün adı:** Worldseed Engine  
**Belge durumu:** 0.2.0 — yeni `navoelyr-lwe/worldseed` deposuna yerleştirilen bağlayıcı başlangıç planı. İlk OpenHands görevi plan/CI uygulanabilirlik denetimidir; henüz ürün kodu yoktur.  
**Belge sürümü:** 0.2.0  
**Ana dil ilkesi:** Ürün ve araçlar ISO C17; GPU compute için tek izinli istisna OpenCL C 1.2. Karar kapalıdır.

> **Ürün ilkesi:** Dünya yaşar. Canlılar kendi fiziksel ve biyolojik kurallarıyla davranır. Sinematik gözlemci olayları yaratmaz; gerçekten yaşananları bulur ve gösterir.

## Bu paket ne içerir?

| Dosya | Bağlayıcı konu |
|---|---|
| `00-URUN-SOZLESMESI.md` | Ürünün bitmiş hâli ve kapsam sözleşmesi |
| `01-ILK-RELEASE-VE-KAPSAM.md` | İlk sürüm, sonraki nesiller ve kapsam dışı konular |
| `02-BILIMSEL-MODELLEME.md` | Bilimsel model standardı ve doğrulama yöntemi |
| `03-C17-MIMARI.md` | C17, modül sınırları, platform ve süreç mimarisi |
| `04-DUNYA-VE-CEVRE.md` | Coğrafya, arazi, su, hava, bitki ve çevre |
| `05-FIZIK-MOTORU.md` | Fizik ve sayısal yöntemler |
| `06-CANLI-BIYOMEKANIK.md` | Canlı bedenleri, kas-iskelet ve hareket |
| `07-BIYOLOJI-EKOLOJI.md` | Fizyoloji, davranış, öğrenme ve ekoloji |
| `08-CIZICI-VE-VARLIK-HATTI.md` | Kendi 3D çizicimiz ve içerik/varlık üretimi |
| `09-SINEMATIK-YONETMEN.md` | Gerçek olayları izleyen sinematik yönetmen |
| `10-SES-KALICILIK-TEKRAR.md` | Ses, kayıt, tekrar oynatma ve kalıcılık |
| `11-PERFORMANS-DONANIM.md` | CPU/GPU, çözünürlük ve performans bütçeleri |
| `12-TEST-KABUL-CI.md` | Tam otomatik test, kanıt ve release kapıları |
| `13-YOL-HARITASI.md` | Aşamalar, bağımlılıklar ve tamamlanma koşulları |
| `14-OPENHANDS-CALISMA-KURALLARI.md` | OpenHands çalışma kuralları ve yetki sınırları |
| `15-RISKLER-KARARLAR.md` | Sabit teknik kararlar ve riskler |
| `16-ARASTIRMA-KAYNAKLARI.md` | Bilimsel ve teknik araştırma başlangıç kaynakları |
| `17-KULLANICI-ARAYUZU.md` | Tam ekran gözlem, kamera ve kullanıcı akışları |
| `18-GUVENLIK-LISANS-PAKET.md` | Dosya güvenliği, içerik lisansı ve Windows paketleme |
| `AGENTS.md` | OpenHands için bağlayıcı repo yönergeleri |

## Repo hazırlık notu

Bu depo yeni ve bağımsız projedir. İlk commit yalnızca ürün planı ve ajan kurallarını içerir; motor kodu yoktur. Eski `worldseed-engine` deposu değiştirilmez ve eski prototip kodu otomatik taşınmaz. İlk OpenHands görevi belge/uygulanabilirlik, CI ve güvenli geliştirme düzenini denetlemektir.

## Öncelik sırası

1. `00-URUN-SOZLESMESI.md` ürünün ne olduğunu tanımlar.
2. `02-BILIMSEL-MODELLEME.md` bilimsel iddiaların nasıl sınanacağını tanımlar.
3. `03-C17-MIMARI.md` ve `15-RISKLER-KARARLAR.md` teknoloji kararlarını sınırlar.
4. `13-YOL-HARITASI.md` geliştirme sırasını ve aşama kapılarını tanımlar.
5. Diğer belgeler ilgili alt sistemin teknik sözleşmesidir.

Çelişki durumunda önce ürün sözleşmesi, sonra bilimsel model standardı ve mimari karar kayıtları geçerlidir. OpenHands çelişkiyi kendi başına sessizce çözemez; karar kaydı açar.

## Proje sahibi için önemli gerçeklik notu

Bu plan, **bilimsel temelli ve kendi içinde tutarlı, çok ölçekli bir dijital ekosistem** hedefler. Dünya üzerindeki her atomun, molekülün, hücrenin ve bütün gezegenin aynı anda en yüksek ayrıntıda simüle edilmesi hedeflenmez; bu sonlu donanımda gerçekçi değildir. Her süreç, amacına uygun mekânsal ve zamansal ölçekte modellenir. Model varsayımları, hata sınırları ve doğrulama kanıtları açık tutulur.

4K/30 FPS, sabitlenen referans sinematik donanım profilinde doğrulanacak hedeftir; tüm bilgisayarlarda garanti değildir. Daha mütevazı donanımlar için 1080p/30 FPS dengeli profil, üst seviye donanım için 4K/60 FPS hedeflenir. Gerçek ölçüm olmadan başarı iddiası verilmez.

Ürün/motor mantığı, kullanıcı arayüzü ve araçlar C17 ile yazılır. Tek GPU dili istisnası OpenCL C 1.2 kernel kaynağıdır. CMake, CI YAML, derleyici ve işletim sistemi API'leri geliştirme/platform altyapısıdır; motorun yerine geçen çalışma zamanı değildir.

## Sabit teknik kararlar

- ISO C17: motor, UI, araçlar, testler ve ürün mantığı.
- OpenCL C 1.2: tek izinli GPU kernel dili; OpenCL 1.2 uyumlu compute API.
- Kendi CPU renderer ve OpenCL compute renderer; Win32/GDI yalnızca piksel sunumu.
- Üçüncü taraf oyun/render/fizik/AI/audio runtime yok.
- Dört ilk tür: kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu.
- İlk ekosistem: yaklaşık 2×2 km ılıman karma orman/orman-çayır mozaiği, dört bağlantılı habitat.
- Referans sinematik profil: 8 çekirdek sınıfı CPU, 32 GB RAM, 12 GB VRAM; native 4K/30 FPS hedefi. Temel profil: 1080p/30 FPS hedefi.
- Teknik sorular kullanıcıya yöneltilmez; OpenHands sabit kararları uygular.
- Her kullanıcıya görünür milestone testlerden sonra prerelease; tüm V1 şartları geçince 1.0.0 stable.
