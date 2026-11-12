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
4. Planı baştan sona denetle; özellikle `18-GUVENLIK-LISANS-PAKET.md` içindeki 'proje sahibi onayı gerekir' ifadesini yetki devriyle tutarlı hâle getir ve hazır dış asset kullanımına izin veren ifadeleri root `AGENTS.md`'deki kendi C17 varlık üretim ilkesiyle uyumlu kıl. Diğer çelişki, uygulanamaz kabul eşiği, bilimsel kaynak veya eksik güvenlik/performans kriterini de kanıta dayalı PR içinde düzelt.
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

## Render performansı — bit-özdeşlik notları

CPU renderer hızlandırması yalnız çıktı **bit-özdeş** kaldığında kabul edilir. Bu
kural kanıtla uygulanır:

- **Paralelleştirme güvenli:** Görüntü ve gölge haritası ayrık **satır bantlarına**
  bölünür; her piksel tam bir banda ait olduğundan sonuç bant sayısından ve iş
  parçacığı çizelgesinden bağımsız olarak seri sonuçla bit-özdeştir. Yeni paralel
  yolun seri yolla eşitliği doğrudan test edilir (bkz. `shadow_build` testi).
- **Örtülen pikseli atlamak güvenli:** Gökyüzü derinlik göndericisiyle
  maskelenerek geometriden sonra çizilir; örtülen piksel iki kez gölgelenmez,
  boş piksele yazılan değer değişmez.
- **Libm yerine yaklaşım tablosu KULLANMA:** `exp()`/`pow()` yerine enterpolasyon
  tablosu koymak 8-bit yuvarlama sınırında tek karede ~1 LSB sapma üretir; golden
  kareler küçük olduğu için fark görünmeyebilir, 1080p'de görünür. Sis için
  denendi ve **geri alındı** (4 sahnede 1 bayt/delta 1). Bit-özdeşlik kapısı
  bunu reddeder.
- **Bit-özdeşlik doğrulaması golden boyutundan büyük yapılmalı:** Küçük golden
  kareler nadir yuvarlama farklarını gizler; en az bir ürün çözünürlüğünde
  (1920×1080) piksel-piksel `cmp` ile doğrula.
- **Görünürlük (frustum) eleme güvenli:** Kamera frustumunun dışındaki bir
  nesnenin eksen-hizalı kutusu tamamen bir düzlem gerisindeyse o nesnenin tüm
  üçgenleri kırpılır; nesneyi atlamak bit-özdeş bir hızlandırmadır (gölge geçişi
  ışık frustum'u farklı olduğundan tüm nesneleri korur). Rasterleyici **uzak
  düzlemde kırpma yapmadığından** yalnız beş düzlem (sol/sağ/alt/üst/yakın)
  kullanılır; uzak düzlemi de elemek uzaktaki geometriyi yanlışlıkla siler.
- **Önden-arkaya (front-to-back) sıralama bit-özdeş DEĞİL:** Derinlik tamponu
  `float32`'dir; iki yüzeyin derinliği **aynı float değerine yuvarlandığında**
  hangi üçgenin önce yazıldığı kazananı belirler (ikinci yüzey kesin daha yakın
  değilse reddedilir). Derinliğe göre sıralama bu "önce yazan" sırasını
  değiştirdiğinden kazanan piksel değişir. Orijinal indekse dayalı kararlı bir
  beraberlik kırıcı bunu düzeltmez: sıralama anahtarı (örneğin üçgen merkez
  derinliği) piksel-başına yuvarlanmış derinlikten farklı olduğundan, örtüşen ve
  aynı float'a yuvarlanan yüzeyler yine yer değiştirebilir. `terrain_day`'de
  ölçüldü: 4188 bayt farklı çıktı (uzak mesafede float32 nicelemesi kaba olduğu
  için çok sayıda yüzey aynı derinliğe çöküyor). Bu yüzden çizim sırasını
  değiştiren aşırı-çizim azaltma bit-özdeş değildir ve bloke.
- **Arka-yüz (backface) eleme bit-özdeş DEĞİL:** Renderer **iki yüzlüdür**
  (double-sided); kapalı bir gövdenin iç yüzeyleri ürün sahnelerinde gerçekten
  görünür. `area < 0` üçgenleri atmak 17 sahnenin 15'ini değiştirir (ölçüldü);
  bu yüzden arka-yüz eleme yapılmaz. `inside_pos` yalnız tarama sırası içindir,
  yüz atma değildir.
- **Sıradaki büyük fırsat — aşırı çizim (overdraw):** Güncel ürün ölçümü
  (1920×1080) `terrain_day` **1.31x** (ertelenmiş gökyüzü bunu önceki ~1.9x'ten
  düşürdü), `ecosystem` 0.80x, `world_stream` 0.56x. Derinlik testi zaten
  gölgelemeden **önce** (erken-z) yapılır; bu yüzden kazanç yalnız çizim sırasını
  değiştirmekle elde edilir ve bu da yukarıdaki niceleme sorunu nedeniyle henüz
  bit-özdeş değildir.
- **Gerçek GPU ölçümü mevcut (izole Windows runner):** Self-hosted Windows x64
  koşucusunda gerçek bir GPU var: NVIDIA GeForce RTX 3060 Laptop (OpenCL 3.0
  CUDA, 30 birim, 6 GB). `.github/workflows/gui-windows.yml` artık `ws_gpu_check`
  ve `ws_perf`'i bu koşucuda çalıştırır (GPU yeteneği + native 4K ölçüm). Ölçülen:
  CPU/GPU semantik eşleşme tolerans içinde; döşemeli (tiled) OpenCL C 1.2 çekirdek
  referans GPU çekirdeğinden ~13x hızlı ve **bit-özdeş**; aygıt kaybı CPU geri
  dönüşüne temiz kapanır. Referans GPU çekirdeği bu iş yükünde CPU'dan yavaştır
  (`verdict=use_cpu`), yani "GPU yalnız gerçekten kazanç sağlıyorsa" ölçümle
  uygulanır. **Canlı ürün döngüsü artık GPU yoluna bağlı:** `--live-bench` modu
  gerçek ürün döngüsünü (dünya adımı + ekoloji + aktörler + kamera + render + HUD)
  native 4K'da `--live-gpu` ile 10 dakika ölçer ve kare-başı yol sayaçlarıyla
  (`gpu_frames`, `cpu_fallback_frames`, `gpu_errors`) GPU'nun her kareyi çizdiğini
  kanıtlar. Ölçülen (izole runner, PR #67): `path=gpu p95_ms=21.96` (51,2 FPS),
  `cpu_fallback_frames=0 gpu_errors=0` — native 4K/30 FPS hedefi (p95 ≤ 33,3 ms)
  bu cihazda **karşılanır** ve zorunlu CI geçididir. Döşemeli çekirdek yalnız
  **düz-renk** rasterdir; ürün gölgelemesi ayrı `kernels/ws_scene.cl` yolundadır ve
  CI golden kapısı CPU golden'ıyla **bit-özdeş** olmayı (`cmp`, 17 sahne) şart
  koştuğu için GPU yolu golden üretiminde kullanılmaz. **Sınır:** ölçüm `shadows=0`
  ve 6 GB cihazda alındı; 12 GB sınıfı referans donanım ve gölge-açık canlı ölçüm
  ayrıca üretilmelidir.
- **Release paketleme betiği PR'larda çalışmaz; kapıları bayatlayabilir:** PR #50
  hareket harness'ini 4 türe çıkarıp `ci.yml`'yi güncelledi ama
  `tools/package-windows.ps1` içindeki "tam 1 verdict=PASS" kapısını güncellemedi;
  bu yalnız release anında ortaya çıktı ve paketi bloke etti. Düzeltme: beklenen
  sahne sayıları artık her harness'ın kendi `WS_*_OK scenes=N` çıktısından okunur.
  Ayrıca Windows GUI iş akışına gerçek paketleme betiğini koşturan "Release package
  smoke" adımı eklendi; kırık release yolu artık PR'da yakalanır. Yeni bir
  sahne/tür kapısı eklerken hem `ci.yml` hem `tools/package-windows.ps1` güncellenir.

## P13 notları — frontal düzlem denge, dört ayak teması ve dönüş (ilk dilim)

- Mevcut referans çözücü (`src/life/ws_creature.c`, `ws_quadruped.c`) **sagittal/
  planar** bir zincirdir; sağ/sol bağımsız 3B state, yanal denge, roll ve yaw
  torku içermez. Sagittal golden sahneler bu yüzden **bit-özdeş kalır**; onlara
  lateral davranış eklemek altın kareyi kırar.
- Frontal düzlem dilimi ayrı bir azaltılmış mertebeden modelle eklendi:
  `src/life/ws_lateral.{h,c}` (LIPM + yakalama-noktası ayak yerleştirme + dört
  bağımsız ayak teması/yük + yük-ağırlıklı destek merkezi + sönümlü yalpa +
  tür-ölçekli yaw sınırı). Model sagittal çözücüye **yazmaz**; bağımsızlık
  korunur. Model kartı: `docs/life/MODEL-KARTI-FRONTAL-DENGE.md`.
- Kanıt ve kapılar: `tests/test_lateral.c` (birim; dört ayak temas + yük korunumu)
  + `tools/ws_lateral_check` (dört tür, düz yürüyüş + komutlu dönüş + temas/yük
  metrikleri). Kapı hem `ci.yml`'e hem `tools/package-windows.ps1`'e eklenir
  (yukarıdaki bayatlama kuralı).
- **Sınır:** bu azaltılmış mertebeden bir modeldir, tam 3B rijit-gövde çözücüsü
  değildir. Dikey yön entegre edilmez; yük yarı-statik ve korunumludur. P13'ün
  kalan kapıları (bağımsız 3B uzuv eklem state'i, dinamik temas kuvveti, 3B
  denge/enerji D3 kıyası, tür-özel 3B gait) açıktır; tam 3B çözücü ayrı iştir.

## P13 notları — tam 3B rijit-gövde çekirdeği (ikinci dilim)

- Sagittal golden sahnelerden **ayrı** bir üç boyutlu çözücü eklendi:
  `src/life/ws_body3d.{h,c}` (kuaterniyon yönelim, gövde-çerçevesi atalet
  tensörü, ball-socket nokta eklemleri, gerçek menteşe kilidi, tek taraflı açısal
  limit, motor torkları, 3B temas + Coulomb sürtünme, deterministik
  ardışık-impuls). Üstünde referans dört ayaklı: `src/life/ws_quad3d.{h,c}`
  (gövde + 4 bağımsız bacak, tırıs yürüyüş, vestibüler denge refleksi). Golden
  CPU çıktısı bu yüzden **bit-özdeş kalır**.
- Kanıt/kapılar: `tests/test_body3d.c` (birim: kuaterniyon, momentum korunumu,
  dört ayak üstünde duran kutu, determinizm, duruş, yürüyüş, yanal bağımsızlık)
  + `tools/ws_body3d_check` (referans kızıl geyik kapılanır; diğer türler INFO).
  `ci.yml`'e "Three-dimensional body evidence (P13)" adımı eklendi. Model kartı:
  `docs/life/MODEL-KARTI-UCD-BEDEN.md`.
- **Kararlılık dersleri:** (a) Çözücü 30 iterasyon gerektirir (12'de küçük
  türler devrilir); 30 ve üstü kararlı. (b) Küçük türler daha küçük adım
  gerektirir — `ws_quad3_suggested_dt` omuz yüksekliğinin 3/4 kuvvetiyle ölçekler.
  (c) Açık-çevrim tırıs sabit adıma kaotik duyarlıdır; **dünya baş açısı (heading)
  tutma terimi** olmadan asimetrik yürüyüş yavaşça kavis yapar (yanal sürüklenme
  14 m'ye çıkabilir). `ws_quad3_balance`'a heading terimi eklenince sürüklenme
  2,0–2,2 ms adım aralığında < 0,3 m'ye iner. Hız geri beslemeli (kapalı-çevrim)
  stride denetleyicisi denendi ve **işe yaramadı** (sistem yine kaotik); bu yüzden
  basit açık-çevrim + heading tutma tercih edildi.
- **Sınır:** yalnız referans kızıl geyik kapılanır. Tilki/tavşan/domuz henüz aynı
  standartta değil (küçük türler yavaş ilerliyor); koşu/farklı zemin
  senaryoları, dinamik temas kuvveti ve D3 kıyası açıktır.
- **Üçüncü dilim — referans türde dönüş ve yokuş:** `ws_quad3_balance`'ın yaw
  hız servosu **dünya baş açısı (heading) hedef izleme** terimine çevrildi.
  Ders: "yaw hızı hedefi" ile "baş açısı hedefi" aynı şey değildir; baş açısı
  tutma terimi hedefi **sıfıra** çektiği için komutlu dönüşü engelliyordu
  (ölçüldü: komut 6 rad, gerçekleşen 0,1 rad). Hedef izleme terimi (hata
  sarmalamasıyla) hedef sıfırken düz yürüyüşü korur, doğrusal artan hedefte
  dönüşü izler. Kanıt: `tools/ws_body3d_check`'e **komutlu dönüş** (izleme
  hatası < 0,25 rad, dik kalma > 0,9) ve **%10 eğimli rampada tırmanış** (> 0,5 m
  tırmanış, ileri ilerleme, dik kalma) kapıları; `tests/test_body3d.c`'ye iki
  birim testi (10/10). `tools/package-windows.ps1`'e 3B gövde kanıt kapısı
  eklendi (sahne sayısı harness'ın kendi `WS_BODY3D_OK scenes=N` çıktısından
  okunur, bayatlamaz). Zemin bir yükseklik fonksiyonudur; eğim bu yolla modellenir.
- **Canlı-döngü GPU benchmark'ı zaten mevcut ve CI'da zorunlu:** `gui-windows.yml`
  `worldseed --live-bench 600 --live-gpu --size 3840x2160` koşar; gerçek dünya
  adımı + ekoloji + aktörler + kamera + render + HUD, `gpu_frames` == kare sayısı,
  `cpu_fallback_frames=0 gpu_errors=0`, ham `frames.csv`, `p95_ok=1`. Son
  ölçüm (RTX 3060 Laptop, `main`=`b066d49`): p95 20,90 ms / ~52,6 FPS (run 43239422714). Bunu yeni bir
  iş sanma; mevcut kapıyı koru.
- **Branch protection açığı sürüyor:** token ile `branches/main/protection` ve
  `rulesets` uç noktaları **403 "Resource not accessible by integration"** verir
  (repo `permissions.admin=true` bildirse de). Dal koruması kurulmuş gibi
  işaretlenmez; bu erişim sınırı kullanıcıya sade dille raporlanır.
