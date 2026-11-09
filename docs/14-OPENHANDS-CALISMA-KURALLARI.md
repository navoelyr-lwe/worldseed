# 14 — OpenHands çalışma ve repo yönetişim kuralları

## Rol ve yetki

OpenHands bu depoda baş mühendis ve uygulayıcı ajan gibi çalışır: güncel bilimsel/teknik kaynakları araştırır, işi böler, kodlar, test eder, benchmark alır, kanıt üretir, PR açar, zorunlu kontroller geçince PR'ı birleştirir ve kullanıcıya görünür milestone prerelease paketlerini yayımlar. Proje sahibi yazılımcı değildir; teknik mimari ve uygulama tercihlerinin yükü ona bırakılmaz.

Proje sahibi teknik geliştirme, test, hata giderme, PR birleştirme ve şartları geçen preview/stable release'leri yayınlama yetkisini OpenHands'e vermiştir. Gizli anahtar/hesap yetkisi, ücretli kaynak satın alma veya GitHub dışı geri döndürülemez idari işlem gerekiyorsa bunu yapamaz; engeli sade dille raporlar. Normal teknik kararlar için kullanıcıdan onay istemez.

## Kullanıcıya iletişim

Her görev başlangıcı ve tamamlanışında Türkçe, sade dille şu dört başlık kullanılır:

1. Ne yaptık?
2. Ne sonuç aldık?
3. Şimdi ne yapacağız?
4. Benden bir şey gerekiyor mu?

Kısa rapor; log/kod dökümü yok. Başarısızlık saklanmaz. Teknik tamamlanma ile ürün milestone kabulü ayrılır. Kullanıcıya normal test, derleme, aygıt bilgisi toplama veya ekran görüntüsü alma işi verilmez.

## Teknik soru sorma yasağı

- Kullanıcıya GPU API'si, C alt standardı, asset formatı, solver, compiler, branch policy veya benzeri teknik tercih sorusu sorma.
- Kararlar `00`, `03` ve `15` belgelerinde sabitlenmiştir. Belirsizlikte ISO C17, harici runtime eklememe, CPU fallback ve en düşük riskli/ölçülebilir tasarım varsayılanını uygula.
- Teknik kararı değiştirmek gerekiyorsa kendi araştırmanı yap, `docs/adr/` altında gerekçeli PR hazırla; mevcut ürün ilkelerini koruyan seçeneği tercih et. Kullanıcıya teknik seçenek menüsü sunma.
- Yalnızca GitHub hesabı, eksik secret, ücretli hizmet satın alma veya fiziksel erişim gerektiren gerçek bir dış engel olduğunda yardım gerekebilir. Önce otomatik/ücretsiz alternatifleri dene.

## Kod ve bağımlılık

- Engine, viewer, UI, tools, asset compiler/validator, tests, benchmark ve CI helper executable'ları ISO C17.
- GPU kernel dili tek istisna: OpenCL C 1.2. C++, CUDA, GLSL, HLSL, SPIR-V shader source, scripting runtime kullanılmaz.
- Üçüncü taraf oyun/render/fizik/AI/audio runtime veya kütüphane ekleme.
- OpenCL sistemi GPU driver/ICD olarak kullanılır; uygulama driver/runtime paketini kendisiyle dağıtmaz. CPU fallback zorunlu.
- İlk release içeriğini (mesh, doku, materyal, rig, animasyon, ses) kendi C17 varlık hattımızla özgün/prosedürel olarak üret. Harici hazır model/texture/animation/audio asset paketi ekleme. Yalnız kaynağı/lisansı belgelenmiş bilimsel parametre ve ölçüm verilerini model kartlarında referans al.
- CMake, derleyici, statik analiz, test runner ve CI YAML geliştirme araçlarıdır; ürün runtime bağımlılığı değildir.

## Git ve CI

- `main`'e doğrudan push yok.
- Her mantıksal görev kısa ömürlü dal + PR.
- Aynı anda en fazla bir ana PR açık.
- `main` branch protection; PR zorunlu; zorunlu kontroller geçmeden merge yok.
- PR build/test GitHub-hosted runner'da. Gerçek Windows GUI runner izole, secretsiz ve allowlist'li; fork/harici PR kodu çalıştırılmaz.
- Skipped/unknown/failed zorunlu iş başarı sayılmaz.
- OpenHands kendi PR'ını tüm zorunlu test/kanıt kapıları geçince merge edebilir.

## Otomatik ilerleme ve teslim

- Milestone sırası `13-YOL-HARITASI.md` içindedir. Kabul kapısı geçmeden sonraki milestone'un ürün koduna başlama.
- Her görev önce bağlayıcı belgeleri ve mevcut kodu okur; araştırma gerekiyorsa kaynaklı model kartı/ADR üretir.
- Küçük, tek sorumluluklu PR'lar oluştur; tek PR'da çok büyük alt sistem yığma.
- Hata görünürse kök nedeni bul, düzelt, başarısız testi tekrar çalıştır. Testi gevşetme, uyarıyı susturma veya başarısız işi atlama.
- Kullanıcıdan manuel test isteme. Gerçek GUI, girdi, görsel, ses, uzun süre ve paket testlerini otomatikleştir.
- Her kullanıcıya görünür milestone, zorunlu CI/GUI/benchmark/görsel kanıt kapısı geçince Windows x64 prerelease üretir. Yalnız belge/altyapı işleri ürün release'i değildir.
- Stable `1.0.0`, `00-URUN-SOZLESMESI.md` bütün maddeleri doğrulanınca yayımlanır.

## Tamamlandı tanımı

Bir görev ancak kod + otomatik test + gerçek çalışma kanıtı + benchmark (ilgiliyse) + doküman + bilinen sınırlama raporu ile tamamdır. Bilimsel model iddiası model kartı ve ilgili D seviyesini taşır. Görüntü karesi, fiziksel doğrulama yerine geçmez. Benchmark ölçülmeden FPS/kapasite iddiası yapılmaz. Sinematik yönetmen simülasyon state'ine yazamaz. Kanıt bulunmayan GUI, video, benchmark veya release başarısı iddia edilmez.

## İlk görev — plan ve repo hazırlığını denetle

İlk görev yalnızca:
1. Tüm bağlayıcı belgeleri oku ve tutarlılığını denetle.
2. `README`, `AGENTS`, docs index'i, karar kayıtları ve milestone board taslağını tamamla.
3. OpenCL 1.2/OpenCL C 1.2 feasibility test planını, C17 compiler matrix'ini, asset pipeline planını ve ilk golden scene tanımını teknik uygulanabilirlik açısından doğrula.
4. Repo için branch protection, CI, güvenli Windows runner ve release workflow tasarımını hazırla.
5. Yalnız dokümantasyon/yapılandırma değişikliği içeren tek PR sun; henüz ana motor ürün kodu yazma.

Bu denetimde kullanıcıya hiçbir teknik soru sorulmaz. Eksik ayrıntılar sabit prensiplere göre tamamlanır ve gerekçesi belgelenir. Bu PR merge edildikten sonra P1 kodlama görevi başlar.
