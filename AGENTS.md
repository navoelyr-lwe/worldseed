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
  bu cihazda **karşılanabilir**, ama **marjinaldir**: aynı commit'te (PR #80) iki
  koşu p95 23,32 ms (geçti) ve 36,55 ms (düştü) verdi — 6 GB paylaşımlı laptop
  GPU'da ~1,57× koşu-arası varyans. Bu yüzden eşik zorunlu geçit olarak kalır ama
  tek bir başarısız koşu "kesin regresyon" sayılmaz; gerçek kabul 12 GB sınıfı
  referans donanımda alınmalıdır. Döşemeli çekirdek yalnız
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
  gerektirir; ölçülen kararlılık sınırı **bacak boyuyla doğrusal** (≈0,0056 ×
  `leg_upper`), bu yüzden `ws_quad3_suggested_dt` `leg_upper` ile ölçekler
  (önceki omuz^3/4 kuralı küçük türleri duraklatıyordu — bkz. beşinci dilim).
  (c) Açık-çevrim tırıs sabit adıma kaotik duyarlıdır; **dünya baş açısı (heading)
  tutma terimi** olmadan asimetrik yürüyüş yavaşça kavis yapar (yanal sürüklenme
  14 m'ye çıkabilir). `ws_quad3_balance`'a heading terimi eklenince sürüklenme
  2,0–2,2 ms adım aralığında < 0,3 m'ye iner. Hız geri beslemeli (kapalı-çevrim)
  stride denetleyicisinin bu ilk denemesi **işe yaramadı** (sistem yine kaotik);
  bu yüzden bu dilimde basit açık-çevrim + heading tutma tercih edildi. (Altıncı
  dilimde farklı bir tasarımla — Raibert ayak yerleştirme — kapalı-çevrim
  başarıyla eklendi; bkz. aşağıda.)
- **Sınır:** bu dilimde yalnız referans kızıl geyik kapılanıyordu; yedinci
  dilimde dört türün tamamı aynı standartta kapılandı. Dinamik temas kuvveti ve
  D3 kıyası hâlâ açıktır.
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
  okunur, bayatlamaz). Zemin bir yükseklik fonksiyonudur; eğim ve engebe bu
  yolla modellenir.
- **Dördüncü dilim — koşu ve farklı zemin:** Yürüyüş/koşu bir gait yapısına
  ayrıldı (`ws_quad3_gait` + `ws_quad3_gait_walk` / `ws_quad3_gait_run` /
  `ws_quad3_gait_apply`). Ders: koşu yalnız daha yüksek kadans değildir; adım
  genliği de artmalı. Ölçüm: frekans ×1,4 ve hip genliği ×2 referans geyikte
  ~1,77 m/s → ~2,13 m/s verir ve stabil kalır. **Duty (duruş oranını) kısaltmak
  denendi ve bu adımda açık-çevrim sürücüyü bozdu** (düşme/yanal sürüklenme);
  bu yüzden duty korunur. Engebeli zemin (10 cm genlikli sinüs toplamı) kapılandı.
- **Küçük türler — beşinci dilim (adım kuralı düzeltildi):** Adım kuralı
  `ws_quad3_suggested_dt` omuz yüksekliğinin **3/4 kuvvetiyle** ölçekliyordu; bu
  fiziksel değildi ve küçük türlere gereğinden küçük adım verip açık-çevrim
  tırısı aşırı sönümleyerek duraklatıyordu. Ölçülen kararlılık sınırı çözücüde
  **bacak boyuyla doğrusal** (≈0,0056 × `leg_upper`); adım artık `leg_upper` ile
  orantılı ve referans geyikte kalibre edilmiş 2 ms'de sınırlı. Kanıt aracı
  `ws_body3d_check` yürüyüş frekansını tüm türler için sabit 1,4 Hz kullanıyordu;
  artık her tür kendi `ws_quad3_gait_walk` frekansını kullanır (bu, tavşanın
  aşırı yüksek kadansta kaotikleşmesini de önler). Ölçüm (aynı kazançlar):
  tilki 0,15 → **0,63 m/s**, tavşan ~0,20 → **0,17 m/s**, domuz 1,36 → **1,07
  m/s**; dört tür de ayakta (`min_contacts=4`), geyik değişmedi (1,77 m/s).
  `tests/test_body3d.c::species walk own dt` her türün kendi adımıyla durup ileri
  yürüdüğünü kilitler. **Sınır:** açık-çevrim tırıs hâlâ tür profil hedeflerinin
  altında (tilki 1,8 / tavşan 2,0 / domuz 1,3 m/s); koşu/dönüş/yokuş senaryoları
  dört türe yayılmadı. Bunlar kapalı-çevrim (temas/hız geri beslemeli)
  denetleyici gerektirir — ayrı bir P13 dilimi. Ders: gait hızı adım genliği ×
  kadans ile sınırlıdır; açık-çevrim sürücüde kadansı tek başına yükseltmek
  ilerlemeyi artırmaz, kaotikleştirir.
- **Canlı-döngü GPU benchmark'ı zaten mevcut ve CI'da zorunlu:** `gui-windows.yml`
  `worldseed --live-bench 600 --live-gpu --size 3840x2160` koşar; gerçek dünya
  adımı + ekoloji + aktörler + kamera + render + HUD, `gpu_frames` == kare sayısı,
  `cpu_fallback_frames=0 gpu_errors=0`, ham `frames.csv`, `p95_ok=1`. Son
  doğrulanan ölçüm (`main`=`4348b5f`, run 43280683169): `path=gpu frames=31275
  avg_ms=19.19 p50=19.17 p95=21.02 p99=22.02 fps=52.1 cpu_fallback_frames=0
  gpu_errors=0`. Bunu yeni bir iş sanma; mevcut kapıyı koru.
- **Branch protection açığı sürüyor:** token ile `branches/main/protection`
  (PUT/GET) ve `rulesets` **oluşturma** uç noktaları **403 "Resource not
  accessible by integration"** verir (repo `permissions.admin=true` bildirse ve
  `rulesets` listesi okunabilse de; liste boş). Bu, token'da `administration`
  yetkisi olmadığını gösterir. Dal koruması kurulmuş gibi işaretlenmez; bu erişim
  sınırı kullanıcıya sade dille raporlanır.

## P13 notları — kapalı-çevrim yürüyüş denetleyicisi (altıncı dilim)

- **Açık-çevrim sürücü hızı düzenleyemez.** `ws_quad3_gait_apply` her ekleme
  fazdan türeyen sabit açı yazar; belli bir kadasta gövde pasif dinamiğin
  bıraktığı yerde oturur. Küçük türler komut hızının çok altında kalır (tilki
  0,63, tavşan 0,17 m/s).
- **Kapalı-çevrim denetleyici Raibert ayak yerleştirme kullanır:**
  `ws_quad3_gait_closed_loop`, `neutral = 0,5·v_ff·T_stance + kv·(v_act − v_des)`.
  İki tuzak kanıtla bulundu ve kodda kaçınıldı:
  1. **Dikey IK referansı nominal olmalı.** Ölçülen (çökmüş) kalça yüksekliğini
     hedefe geri beslemek, gövde çökerken bacağı tam o anda kısaltır — pozitif
     geri besleme, duruş çöker (deer 1,04 → 0,53 m). Referans `ground +
     shoulder_h` olmalı (açık-çevrim sürücü de böyle yapar).
  2. **İleri besleme hızı bacak erişimine sınırlanmalı.** `0,5·v_des·T_stance`
     bacağın yatay erişimini (`reach = sqrt((l1+l2)² − shoulder_h²)`) aşarsa nötr
     nokta bacağın dışına taşar, IK doyar ve gövde geri sürüklenir (tavşan
     −0,31 m/s). `v_ff = min(v_des, 2·0,8·reach/T_stance)` bunu önler; hız-hata
     terimi yine gerçek `v_des` kullanır.
- **Sürüş mekanizması:** duruş ayağı dünya uzayında sabit kalır (latch edilmiş
  `td_x`); `v_des > v_act` iken nötr nokta kalçanın gerisine düşer ve gövdeyi
  ileri iter. Ayağı duruş boyunca geriye "yürüyen bant" gibi kaydırmak **yanlış**
  (her zaman ileri iter, kaçar) — denenip geri alındı.
- **Ölçüm (20 s, düz zemin, komut türün kendi `ws_species_walk_speed` değeri):**
  tilki 0,63→**0,81**, tavşan 0,17→**0,36**, geyik 1,77→2,29, domuz 1,07→1,04
  m/s; dört tür de dik (`upright_min > 0,9`). `tests/test_body3d.c::closed-loop
  speed` ve `tools/ws_body3d_check.c` `closed_loop` satırları bunu kilitler.
- **Sınır:** hiçbir tür komut hızının tamamına ulaşmaz; küçük türlerin bacak
  erişimi ve kadansı tilki 1,8 / tavşan 2,0 m/s'yi bu gövde geometrisiyle yapısal
  olarak engeller. Bu değerler ayrıca sabit-Froude (`sqrt(shoulder_h)`)
  ölçeklemesiyle tutarsızdır (tilki ≈0,90, tavşan ≈0,62 m/s olurdu). Profil
  hızlarının gözden geçirilmesi ya da tür adım/kadans geometrisinin yeniden
  ayarlanması ayrı bir dilimdir. Kapalı-çevrim şu an yalnız düz zemin yürüyüşü
  için kapılanır; dönüş/yokuş/engebeli zeminde kapalı-çevrim kapılanması ayrı bir
  dilimdir.
- **Golden bit-özdeşliği:** bu çözücü sagittal golden sahnelerden ayrıdır; 17
  golden sahne bit-özdeş kalır (doğrulandı). `ws_species_walk_speed`'i Froude
  değerlerine çekmek ürün 2B hareket kapısını kırar (2B model de komutun ~%15'ini
  tutturur) — geri alındı; değerler bu yüzden değiştirilmedi, yalnız yorum
  gerçeği yansıtacak biçimde güncellendi.

## P13 notları — dört türde tür-ölçekli senaryolar (yedinci dilim)

- **Koşu/dönüş/yokuş/engebeli zemin artık dört türde kapılanıyor.** Bu senaryolar
  önceden yalnız referans geyikte ve geyik boyutunda mutlak eşiklerle kapılanıyordu
  (`v_fwd > 1,9`, `climb > 0,5`, `drift < 0,6`). Artık her tür kendi ölçeğinde
  değerlendirilir: duruş/yürüyüş sınırları omuz yüksekliğiyle, koşu eşiği türün
  **gerçekleşen** yürüyüş hızının 1,10 katıyla, yokuş tırmanışı gövde yüksekliğinin
  1,2 katıyla, engebeli zemin eşiği yürüyüş hızının 0,6 katıyla ölçeklenir. Kanıt
  aracı artık 20 sahne üretir (stand/walk + closed_loop + turn + slope + run +
  rough; her aile dört türden geçer). CI ve `tools/package-windows.ps1` kapıları
  aile bazında 4/4 ister.
- **Koşu adımı tür ölçeğine bağlandı — asıl hata buradaydı.** `ws_quad3_gait_run`
  hip genliğini tüm türlerde **sabit 0,20 rad**'a çıkarıyordu. Hip açısı bir
  açıdır; ayağın kat ettiği yol = açı × bacak boyu. Sabit açı kısa bacaklı
  türlerde doyuma giriyordu: tavşanın 0,10 m bacağı 0,60 m bacağın yarısından az
  yol kat ediyor, bu yüzden "koşusu" yürüyüşünden hızlı olmuyordu (ölçülen oran
  ~1,05; tilki de ~1,25'te tıkanıyordu). Düzeltme: hip açısını
  `sqrt(0,60 m / leg_upper)` ile ölçekle (Froude tutarlı adım). Ölçülen
  yürüyüş→koşu oranı: geyik 1,20, tilki 1,38, tavşan **1,64**, domuz 1,28.
  `tests/test_body3d.c::species run exceeds walk` her türün koşusunun kendi
  yürüyüşünü 1,10×'ten fazla geçtiğini kilitler.
- **Ders:** tür-ölçekli bir gait parametresi mutlak değil, boyutla ölçeklenen bir
  sayı olmalı. Sabit bir "koşu adım açısı" referans türde doğru görünür ama kısa
  bacaklı türlerde sessizce doyuma girer; kapı yalnız referans türde olduğu sürece
  bu görünmez.
- **Dönüş hızı kaotik bir uçurum gösterdi (kanıtla bulundu).** Dönüş kapısı
  eskiden 0,6 rad/s sabit yaw ile koşuyordu. Windows koşucusunda (farklı libm)
  tavşan bu hızda tam ters döndü (`upright=-1.0`); Linux'ta şansla geçiyordu.
  Yaw hızını taradım: **0,16 rad/s üstünde dört tür de devriliyor**, altında
  kararlı. Bu yüzden kabul kapısı tüm türlerde geniş marjla kararlı olan **0,10
  rad/s**'ye çekildi (dört türde de `upright ≥ 0,987`). Ders: kaotik bir rejimde
  eşiği şanslı bir noktaya sabitlemek kırılgan bir kapı üretir; kapı, ölçülmüş
  kararlılık bölgesinin içinde geniş marjla seçilmeli. Daha hızlı ve sağlam dönüş
  ayrı bir denetleyici dilimidir.
- **Golden bit-özdeşliği:** `ws_quad3_gait_run` sagittal golden sahnelerden
  ayrıdır; 18 golden sahne bu değişiklikten sonra bit-özdeş kaldı (doğrulandı).

## P13 notları — dönüş kanıtı dürüst kapıya bağlandı (sekizinci dilim)

- **Birim testi gerçek bir hatayı gizliyordu.** `tests/test_body3d.c::quad3 turns`
  0,6 rad/s ile koşup yalnız **son kareyi** kontrol ediyordu. Ölçtüm: bu hızda
  gövde koşu ortasında takla atıyor (`up_min = −0,998`, diklik `up.y ≈ −1`),
  sonra toparlanıp son karede `up.y > 0,9` dönüyordu — test yine "geçiyordu".
  Düzeltme: test artık koşu boyunca **minimum dikliği** izler ve `up_min > 0,9`
  şartını arar. Ders: bir testin yalnız son durumu kontrol etmesi, koşu ortasında
  olup biten bir devrilmeyi gizler; kararlılık kapısı **tüm koşu boyunca** en kötü
  değeri izlemeli.
- **Dönüş sınırı mimari DEĞİL; denetleyici/ayar sınırı (ölçüldü).** Önceki
  "kök neden abduction eklemi yokluğu" iddiası dokuzuncu dilimde **çürütüldü**.
  45 s'lik koşularda: 0,10 rad/s'de her tür **15–21 s sonra** yavaşça öne
  yunuslayıp devrilir (tavşan 14,7 / domuz 14,9 / tilki 16,8 / geyik 20,8 s), yani
  10 s'lik eski kapı sürdürülemeyen bir dönüşü "geçiriyordu". Yalnız **≤0,04
  rad/s** dört türde 30–45 s dik kalır (0,05 rad/s geyikte 42, tavşanda 28 s'de
  devrilir). Denenip **başarısız** olan yükseltmeler: (a) yalpa/yunuslama integral
  terimi (etkisiz), (b) dönüş-hızına bağlı denge kazancı (yürüyüşü bozar),
  (c) hip abduction eklemi + roll geri beslemesi (devrilme sürer), (d) gövde/
  dünya-çerçevesi yaw torku, (e) dönüşe yatış referansı (`phi_ref = a_lat/g`),
  (f) dt/4 (dönüşü düzeltir ama yürüyüş hızını durdurur: geyik 2,2→0,25 m/s),
  (g) 16–32× denge kazancı (dönüşü düzeltir ama yürüyüşü bozar). Sonuç: sınır
  sayısal/kontrolcü ayarına bağlı; sürdürülebilir dönüşü yükseltmek ayrı bir
  dilimdir ve burada **iddia edilmez**. Ek ölçüm: 3B referans gait stance
  sırasında zamanın ~%46'sında teması sıfıra düşürüyor (gövde zıplaması); diz
  kaldırma (knee_lift) bunun ana kaynağı — ayrı bir dilim.
- **Dönüş kanıtı artık sürdürülebilir hıza bağlı.** Birim testi ve kanıt aracı
  `0,04 rad/s'de 30 s` koşar ve koşu boyunca `upright_min > 0,9` arar (ölçülen
  dört türde ≥ 0,933, `track_err ≤ 0,010`); `body3d turn` satırı `upright_min`
  değerini yazdırır.
- **Kapı artık sürdürülebilir hızda.** Yukarıdaki eski "0,10 rad/s'de kalır"
  notu geçersizdir; birim testi ve kanıt aracı `0,04 rad/s'de 30 s` koşar
  (`upright_min > 0,9`; ölçülen dört türde ≥ 0,933). 17 golden sahne bit-özdeş
  kalır.
- **Kapalı-çevrim hız denetimi sınırlı; kaçış kapısı eklendi.** Raibert nötr
  ayağı bacak erişiminin 0,8 katıyla sınırlıdır, bu yüzden doğal gait hızı bu
  adımı aşan gövdeyi frenleyemez: ölçülen geyik komutun ~1,5 katına kaçar
  (2,29 / 1,50), domuz ~0,8 katında kalır; profil hızı kendi erişimini aşan
  tilki/tavşan altında kalır (0,45 / 0,18 katı). Dokuzuncu dilim hem birim
  testine hem kanıt aracına **üst sınır** ekledi (`v_fwd < 2 × v_des`) ki bir
  kaçış (sürekli hızlanan gövde) gerçek hata olarak yakalansın. Kaçışın kendisi
  açık bir kalibrasyon işidir.

