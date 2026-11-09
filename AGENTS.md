# AGENTS.md — Worldseed Engine

Bu depo `navoelyr-lwe/worldseed` için bağlayıcı ajan talimatıdır. Tam ürün planı `docs/plan-bundle/RESTORE.md` ve geri açılacak `docs/00-URUN-SOZLESMESI.md`…`docs/18-GUVENLIK-LISANS-PAKET.md` belgelerindedir. README başlangıç haritasıdır. Öncelik sırası: ürün sözleşmesi → bilimsel model standardı → sabit mimari kararları → alt sistem belgeleri → milestone yol haritası.

## Kullanıcı iletişimi

Proje sahibi yazılımcı değildir ve teknik tercihlerle uğraşmak istemez. Her görev başında/sonunda sade Türkçe ile şu dört başlık kullanılır:

1. Ne yaptık?
2. Ne sonuç aldık?
3. Şimdi ne yapacağız?
4. Benden bir şey gerekiyor mu?

Kod/log dökümü yok. Başarısızlıklar gizlenmez. Kullanıcıya normal derleme, test, aygıt bilgisi, ekran görüntüsü veya teknik karar işi verilmez. Kullanıcının rolü ürünün ilerleyen preview/stable release paketlerini görmek ve kullanmaktır.

## İlk görev — bağlayıcı öncelik

1. `docs/plan-bundle/RESTORE.md` içindeki yönergeyle 16 aktarım parçasını birleştir, SHA-256 değerini doğrula ve ürün planı belgelerini kanonik `docs/` yollarına geri aç.
2. Manifestteki tüm dosya sayısını ve içerik bütünlüğünü doğrula. Root `AGENTS.md` bu dosyanın bağlayıcı ana ajan kuralı olarak kalır; arşivdeki AGENTS yalnızca referans kopyasıdır.
3. README bağlantılarının gerçek dosyalara gittiğini doğrula.
4. Planı baştan sona denetle; çelişki, uygulanamaz kabul eşiği, bilimsel kaynak veya eksik güvenlik/performans kriteri varsa kanıta dayalı PR içinde düzelt.
5. CI, main koruması, PR güvenliği, release akışı ve izole Windows GUI otomasyonunu kur.
6. OpenCL C 1.2 / CPU fallback, CPU renderer, görsel varlık hattı ve otomatik golden-scene kanıtı için küçük feasibility prototip/planları hazırla.
7. Tek bir dokümantasyon/altyapı PR'ı aç. İlk görevde büyük ürün özellikleri veya simülasyon sistemleri kodlama.
8. Bu PR tüm kontrollerden geçip birleştikten sonra P1'e geç; kullanıcıdan teknik onay veya tercih isteme.

Aktarım arşivinin beklenen SHA-256 değeri RESTORE belgesinde sabittir. Özet uyuşmazsa bozuk belgeyi kullanma; nedeni bul ve arşiv içeriğini doğrulamadan ilk görevi tamamlanmış sayma.

## Teknik kararlar — değişmez

- Uygulama, motor, UI, araçlar, asset compiler/validator, test ve benchmark kodu ISO C17.
- GPU hesaplama tek istisnası: OpenCL 1.2 API uyumluluğu ve OpenCL C 1.2 kernel dili. OpenCL C, C99 tabanlı C lehçesidir; ISO C17 değildir.
- C++, CUDA, GLSL, HLSL, SPIR-V shader source, Vulkan/OpenGL/Direct3D renderer pipeline, üçüncü taraf oyun/render/fizik/AI/audio runtime yasaktır.
- Kendi C17 tiled CPU renderer zorunlu referans/fallback; OpenCL compute GPU backend isteğe bağlı hızlandırma. GPU yoksa CPU fallback.
- Windows Win32/GDI yalnız pencere/girdi ve hazır piksel tamponu sunumu içindir; GDI render motoru değildir.
- İlk release'e giren mesh, doku, materyal, rig, animasyon ve ses içerikleri de dışarıdan hazır paket olarak alınmaz; kendi C17 içerik/varlık hattımızla özgün olarak üretilir veya proje içinde tanımlanır. Harici hazır model/texture/animation/audio asset paketi kullanılmaz. Bilimsel parametre ve ölçüm verileri kaynak gösterilerek model kartlarında referans alınabilir; bunlar runtime varlık paketi değildir. Harici executable, script, plugin, library, engine veya içerik paketi bağımlılığı eklenmez.
- İlk ekosistem 2×2 km ılıman karma orman/orman-çayır; habitatlar orman, çayır/açıklık, tatlı su kenarı ve kayalık/yamaç.
- İlk türler kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu. Dört türün tamamı fiziksel beden ve türe özgü hareket kontrol modeli kullanır. Kuş uçuşu sonraki sürüme bırakılır.
- Donanım profilleri, GPU kararı, asset ilkesi ve uzak alan modeli proje planındaki sabit kararlar belgesindedir.

## Teknik soru sorma yasağı

Kullanıcıya teknik tercih sorusu sorma. Sabit belgelerde yer alan kararı uygula. Ayrıntı eksikse C17, minimum bağımlılık, CPU fallback, deterministik simülasyon ve bilimsel olarak daha ihtiyatlı model varsayılanını seç. Karar değişikliği gerekiyorsa kaynak ve ölçümle ADR/PR üret; kullanıcıya seçenek listesi gönderme. Kullanıcı müdahalesi yalnız ücretli hizmet, gizli kimlik bilgisi, GitHub dışı idari yetki veya fiziksel erişim gibi gerçek dış engellerde istenebilir; önce otomatik ve ücretsiz yolları dene.

## Git, CI ve release

- `main` için branch protection veya eşdeğer ruleset zorunludur; doğrudan push yok; kısa ömürlü dal + PR.
- En fazla bir ana PR açık.
- Zorunlu kontrollerden biri fail/skipped/unknown ise merge yok.
- GitHub-hosted runner'larda PR build/unit/headless test. Self-hosted Windows GUI runner izole, secretsiz, allowlist'li; fork/harici PR kodu çalıştırılmaz.
- OpenHands kendi PR'ını tüm zorunlu test/kanıt kapıları geçince merge edebilir.
- Her kullanıcıya görünür milestone, Windows x64 paketi, temiz ortam smoke test, GUI/golden-scene ve performans kapıları geçince prerelease olur. Sadece belge/altyapı işleri ürün release'i sayılmaz.
- `1.0.0` ancak ürün sözleşmesindeki tüm koşullar geçince yayımlanır.
- Preview/stable release yayınlama yetkisi proje sahibi tarafından OpenHands'e devredilmiştir; tüm yayın kapıları geçince her release ayrıca izin sorulmadan yayımlanır. Belgelerdeki eski/çelişkili 'proje sahibi onayı gerekir' ifadeleri bu yetki devriyle geçersizdir; ilk dokümantasyon PR'ında düzeltilir.
- Branch protection / ruleset ilk görevde kontrol edilmeli; eksikse mevcut GitHub erişimi ve CLI/UI araçlarıyla kurulmalı. Entegrasyon yetkisi yetersizse bunu saklama, yapılmış gibi işaretleme veya kullanıcıyı teknik ayarlara yönlendirme; otomatik erişim sınırını sade dille raporla ve ürün koduna geçmeden önce en azından PR/CI kapılarını uygula.

## Tamamlandı tanımı

Kod + otomatik test + gerçek çalışma kanıtı + benchmark (ilgiliyse) + doküman + bilinen sınırlama raporu olmadan görev tamamlanmış sayılmaz. GUI testi gerçek Windows penceresini çalıştırmalı. Ekran görüntüsü/video gerçek build ve seed ile ilişkilendirilmeli. Görsel regresyon estetik kaliteyi tek başına ispatlamaz; golden scene kalite protokolü de gerekir. Bilimsel model iddiası model kartı ve D0-D4 kanıt seviyesi olmadan yapılmaz.
