# AGENTS.md — Worldseed Engine

Bu depo `navoelyr-lwe/worldseed` için bağlayıcı ajan talimatıdır. README ve proje planı arşivindeki `00`–`18` belgeleri ürün sözleşmesini tanımlar. Öncelik sırası: ürün sözleşmesi → bilimsel model standardı → sabit mimari kararları → alt sistem belgeleri → milestone yol haritası.

## Kullanıcı iletişimi

Proje sahibi yazılımcı değildir ve teknik tercihlerle uğraşmak istemez. Her görev başında/sonunda sade Türkçe ile şu dört başlık kullanılır:

1. Ne yaptık?
2. Ne sonuç aldık?
3. Şimdi ne yapacağız?
4. Benden bir şey gerekiyor mu?

Kod/log dökümü yok. Başarısızlıklar gizlenmez. Kullanıcıya normal derleme, test, aygıt bilgisi, ekran görüntüsü veya teknik karar işi verilmez. Kullanıcının rolü ürünün ilerleyen preview/stable release paketlerini görmek ve kullanmaktır.

## Teknik kararlar — değişmez

- Uygulama, motor, UI, araçlar, asset compiler/validator, test ve benchmark kodu ISO C17.
- GPU hesaplama tek istisnası: OpenCL 1.2 API uyumluluğu ve OpenCL C 1.2 kernel dili. OpenCL C, C99 tabanlı C lehçesidir; ISO C17 değildir.
- C++, CUDA, GLSL, HLSL, SPIR-V shader source, Vulkan/OpenGL/Direct3D renderer pipeline, üçüncü taraf oyun/render/fizik/AI/audio runtime yasaktır.
- Kendi C17 tiled CPU renderer zorunlu referans/fallback; OpenCL compute GPU backend isteğe bağlı hızlandırma. GPU yoksa CPU fallback.
- Windows Win32/GDI yalnız pencere/girdi ve hazır piksel tamponu sunumu içindir; GDI render motoru değildir.
- Varlıklar veri dosyalarıdır. Özgün, kamu malı/CC0 veya ticari kullanım izni net ve kaynak/lisansı kayıtlı içerik kullanılabilir. Harici executable, script, plugin, library veya engine kullanılamaz.
- İlk ekosistem 2×2 km ılıman karma orman/orman-çayır; habitatlar orman, çayır/açıklık, tatlı su kenarı ve kayalık/yamaç.
- İlk türler kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu. Dört türün tamamı fiziksel beden ve türe özgü hareket kontrol modeli kullanır. Kuş uçuşu sonraki sürüme bırakılır.
- Donanım profilleri, GPU kararı, asset ilkesi ve uzak alan modeli proje planındaki sabit kararlar belgesindedir.

## Teknik soru sorma yasağı

Kullanıcıya teknik tercih sorusu sorma. Sabit belgelerde yer alan kararı uygula. Ayrıntı eksikse C17, minimum bağımlılık, CPU fallback, deterministik simülasyon ve bilimsel olarak daha ihtiyatlı model varsayılanını seç. Karar değişikliği gerekiyorsa kaynak ve ölçümle ADR/PR üret; kullanıcıya seçenek listesi gönderme. Kullanıcı müdahalesi yalnız ücretli hizmet, gizli kimlik bilgisi, GitHub dışı idari yetki veya fiziksel erişim gibi gerçek dış engellerde istenebilir; önce otomatik ve ücretsiz yolları dene.

## Git, CI ve release

- `main` korumalı; doğrudan push yok; kısa ömürlü dal + PR.
- En fazla bir ana PR açık.
- Zorunlu kontrollerden biri fail/skipped/unknown ise merge yok.
- GitHub-hosted runner'larda PR build/unit/headless test. Self-hosted Windows GUI runner izole, secretsiz, allowlist'li; fork/harici PR kodu çalıştırılmaz.
- OpenHands kendi PR'ını tüm zorunlu test/kanıt kapıları geçince merge edebilir.
- Her kullanıcıya görünür milestone, Windows x64 paketi, temiz ortam smoke test, GUI/golden-scene ve performans kapıları geçince prerelease olur. Sadece belge/altyapı işleri ürün release'i sayılmaz.
- `1.0.0` ancak ürün sözleşmesindeki tüm koşullar geçince yayımlanır.
- Preview/stable release yayınlama yetkisi proje sahibi tarafından OpenHands'e devredilmiştir; ayrıca onay sorulmaz.

## Tamamlandı tanımı

Kod + otomatik test + gerçek çalışma kanıtı + benchmark (ilgiliyse) + doküman + bilinen sınırlama raporu olmadan görev tamamlanmış sayılmaz. GUI testi gerçek Windows penceresini çalıştırmalı. Ekran görüntüsü/video gerçek build ve seed ile ilişkilendirilmeli. Görsel regresyon estetik kaliteyi tek başına ispatlamaz; golden scene kalite protokolü de gerekir. Bilimsel model iddiası model kartı ve D0-D4 kanıt seviyesi olmadan yapılmaz.

## İlk görev

Yalnız plan/repo hazırlık denetimi: bütün proje planı belgelerini doğrula; tutarsızlıkları ve eksik kabul eşiklerini düzelt; OpenCL C 1.2, CPU/GPU renderer, asset pipeline ve golden scene için küçük feasibility planları hazırla; branch protection/CI/runner/release workflow tasarımını kur. Tek dokümantasyon/altyapı PR'ı aç. Bu ilk görevde büyük ürün özellikleri kodlama. Kullanıcıya teknik soru sorma. Bu PR merge edilince P1 kod geliştirmesine otomatik geç.
