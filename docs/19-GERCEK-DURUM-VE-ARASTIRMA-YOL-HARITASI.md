# 19 — Worldseed ürün gerçekliği denetimi ve araştırma temelli sonraki yol

**Denetim tarihi:** 11 Kasım 2026  
**İncelenen ana dal:** main  
**İncelenen commit:** f6d15ed8c54ef51183626aafb565ff817d0b45c0 (v0.21.0-preview)  
**Kapsam:** ürün sözleşmesi, milestone/yol haritası, C17 kaynak kodu, model kartları, otomatik doğrulama, performans raporları ve güncel bilimsel/teknik literatür.

Bu belge bir yeniden başlatma veya teknoloji değişikliği önerisi değildir. Mevcut Worldseed kod tabanını ve sabit mimari kararlarını korur. Amacı, çalışan prototiplerle V1.0 ürün kabulünü birbirinden ayırmak, eksikleri kanıt düzeyine göre sıralamak ve OpenHands'in sonraki geliştirme seçimlerini gerçek ürün hedeflerine bağlamaktır.

## 1. Yönetici özeti

Worldseed'in mühendislik temeli gerçek ilerleme kaydetmiştir: C17 çekirdeği, deterministik dünya üretimi, çevre/terrain katmanı, temel fizik, temsilî hayvan ajanları, çok ölçekli dünya host'u, kayıt/tekrar, Windows gözlem uygulaması, kendi CPU renderer'ı, OpenCL 1.2 denemesi ve CI/GUI kanıt zinciri mevcuttur. 11 Kasım 2026'da son preview'ın CI, izole Windows GUI ve release akışları başarılıdır.

Bununla birlikte repo durumu, kullanıcının nihai hedefi olan doğal görünümlü, biyolojik olarak makul, sesli, 2 km × 2 km yaşayan orman dünyasının tamamlandığını göstermemektedir. Bazı milestone'lar dar teknik prototip ölçütlerine göre tamamlanmış, ancak aynı başlıkların ürün sözleşmesindeki daha geniş gerçekçilik hedefleri açık kalmıştır.

**En önemli sonuç:** OpenHands'in mevcut iş seçme davranışı, belgelerdeki küçük ve somut kontrol listelerini tamamlamaya uygundur. Ancak P12 sonrası ayrıntılı yol haritası ve ürün seviyesinde kalite kapıları açıkça yazılmadığı sürece, arayüz/kamera/dışa aktarma gibi dar özellikler üzerinde ilerlemeye devam edip 3B biyomekanik, ekoloji, gerçekçi ses ve fotogerçekçi görüntü kalitesini ertelemesi mümkündür.

Bu nedenle önerilen sıra:

1. Mevcut çalışma alanını kaybetmeden OpenHands oturumunun durumunu raporlatmak.
2. Bu denetim ve yol haritası PR'ını OpenHands'e inceletmek.
3. P12 render/performance işini ürünün bir sonraki ana önceliği yapmak.
4. P13–P17 alanlarını bağımlılık sırasıyla ele almak.
5. 1.0.0'ı yalnız ürün sözleşmesinin bütün kapıları geçildiğinde yayımlamak.

## 2. Denetim yöntemi ve kanıt sınırı

İnceleme; main dalındaki kaynak kodu ve belgelerin okunması, PR #39–#41, son CI/GUI/release kayıtları ve aşağıdaki kaynaklardan yapılan literatür çapraz kontrolüyle yürütüldü.

Güncel otomasyon kanıtları:

- PR #39: serbest kamerada ağaç gövdesinden kaçınma.
- PR #40: zaman damgalı PPM kareleri + 48 kHz stereo WAV dışa aktarma.
- PR #41: v0.21.0-preview yayın notları.
- CI run 42974024398: Linux GCC/Clang matrisleri, Windows MSVC Debug/Release, OpenCL CPU kernel, plan bütünlüğü ve doküman bağlantıları başarılı.
- Windows GUI run 42974024303: gerçek Win32 penceresi, canlı uygulama, dünya seçimi, replay, export ve gölge kanıtları başarılı.
- Release run 42974038527: v0.21.0-preview yayın akışı başarılı.

Bu denetim GitHub'daki kaynak ve kanıt dosyalarını incelemiştir; burada gerçek bir GPU'lu referans bilgisayarda uygulamayı çalıştırıp bağımsız FPS ölçümü yapılmamıştır. Performans sayıları P12 raporundan alınmıştır. 3B biyomekanik, akustik veya ekoloji alanında bağımsız laboratuvar ölçümü yapılmış gibi bir iddia yoktur. Literatür, model tasarımı ve doğrulama hedefleri için kullanılmıştır; parametreler otomatik olarak kalibre edilmiş değildir.

## 3. Çalışan ve güvenilir temeller

| Alan | Doğrulanan mevcut temel | Değerlendirme |
|---|---|---|
| Mimari | ISO C17 motor/ürün kodu; OpenCL C 1.2 tek GPU kernel istisnası; kendi CPU renderer'ı; üçüncü taraf oyun motoru yok | Sabit mimari kararı korunmalı |
| Determinizm | Sabit tohumlu üretim, dünya özeti, checkpoint/replay ve dışa aktarma tekrar testleri | Güçlü mühendislik temeli |
| Çevre | Prosedürel terrain, su sınıflandırması, dört habitat sınıfı, bitki yerleşimi, günün saati ve meteoroloji alanları | İlk makro çevre prototipi mevcut |
| Dünya sürekliliği | 512 m bölgeler, near/mid/far kadansları, region streaming, journal ve checkpoint | Kamera dışı ilerleme için temel mevcut; uzak alanlar toplulaştırılmış |
| Uygulama | Dünya seçimi, serbest/takip/harita kipleri, HUD, ayarlar, kayıt/yükleme, replay ve export | Kullanıcıya dönük iskelet ve önemli işlevler mevcut |
| Otomasyon | PR CI, gerçek Windows GUI, screenshot/evidence artifact, paketleme ve release akışı | Son preview'da başarılı |
| Bilimsel dürüstlük | Model kartları, D0–D4 seviyesi ve bilinen sınırlama kayıtları | Uygun ilke; bazı ürün kabul etiketleriyle daha tutarlı bağlanmalı |

Bu temeller korunmalı; yeniden yazma veya Unreal/Unity/Godot'a geçiş önerilmemektedir.

## 4. Ürün hedefi ile gerçek uygulama arasındaki fark

Aşağıdaki durumlar kaynak kodu ve mevcut model kartlarının gösterdiği kapsamdır. Bunlar, ekibin gelecekte yapamayacağı anlamına gelmez; yalnızca bugün tamamlanmış gibi sunulmaması gerektiğini gösterir.

### 4.1 Dünya boyutu ve ayrıntı katmanları

- Oturum başlığı referans dünyayı 4 × 4 adet 512 m bölge, yani 2.048 m × 2.048 m olarak tanımlar.
- Bölge ızgarası 64 × 64 hücre ve hücre başına 8 m'dir.
- Canlı oturumdaki yüksek ayrıntılı ekoloji, WS_SESSION_PATCH_M = 256 m olan odak yaması üzerinde çalışır.
- Odak dışındaki dünya host'u 512 m bölgeler için near/mid/far düzeylerinde sürekli ama toplulaştırılmış nüfus modeli yürütür. Model kartı bu uzak nüfus modelinin D2 ve kalibre edilmemiş olduğunu belirtir.
- Oturum, odak yaması değiştiğinde çevre render sahnesini yeniden kurar.

**Sonuç:** 2 km × 2 km referans ölçeği ve kamera dışı ilerleme altyapısı vardır; ancak bu, 2 km × 2 km alanın tamamında tek tek yüksek ayrıntılı hayvanların ve aynı ayrıntıdaki ekolojik etkileşimlerin simüle edildiği anlamına gelmez. LOD/aggregate yaklaşımı geçerlidir; ürünün hangi ölçekte hangi ayrıntıyı koruduğu açık bir kabul tablosuyla ölçülmelidir.

Kaynak dosyalar: src/ui/ws_session.h, src/ui/ws_session.c, src/world/ws_region.c, src/world/ws_world_host.c, src/life/ws_population.h.

### 4.2 Habitat, bitki ve çevresel süreçler

- Dört habitat sınıfı kodda bulunur; yerleşim terrain yüksekliği/eğim/su ve deterministik gürültüden türetilir.
- Bitki yerleşimi seed ve habitat sınıfına göre ağaç/çalı/ot olarak olasılıksal üretilir.
- Hidroloji D8 en dik iniş komşusuna akış yönü atar, akış biriktirir ve nehir hücrelerini sınıflandırır; yağış/buharlaşma basit bir hücre su bütçesidir.
- Hava/çevre alanları sıcaklık, rüzgâr, nem, bulut ve yağış için deterministik, analitik/gürültü tabanlı yaklaşımlar kullanır.
- Render edilen ağaçlar şimdilik koni mesh örnekleridir. Bitki katmanındaki yerleşim, gerçek ağaç taç geometrisi, gövde dallanması veya uzun dönem orman ardıllığı ile eşdeğer değildir.
- Yüksek ayrıntılı ekolojinin bitki biyokütlesi ızgarası ile render edilen her bitkinin büyüme, yaşlanma, ölüm ve yeniden oluşum süreçleri aynı ayrıntıda bağlanmış değildir.

**Sonuç:** Bu, seed'e göre tekrarlanabilir bir çevre prototipidir. Orman dinamiği, mevsimsel bitki fenolojisi, rekabet, canopy ışığı, tohum bankası ve ağaç yaşam döngüsü için henüz nihai ekolojik model değildir.

### 4.3 Dört türün biyomekaniği

- Kızıl geyik, tilki, Avrupa tavşanı ve yaban domuzu için farklı boyut/kütle/uzuv/gait parametre profilleri vardır.
- Dört tür aynı ortak quadruped inşa ve kontrol sisteminden geçer.
- Kaynak kodu ve tür kartları modelin sagittal düzlemde (yan görünüş) çalıştığını; sol/sağ uzuvların ayrı 3B dinamikler olmadığını; lateral denge ve yaw torkunun henüz bulunmadığını açıkça söyler.
- Görsel sunumda beden parçaları ortak küp mesh bağlantılarıdır; dört türü ayıran parametre ve renk vardır, ancak fotogerçekçi tür anatomisi/deri/kürk/baş/göz/ayak/tırnak/antler detayları yoktur.
- Canlı oturumun render edilen aktif aktörleri, odak ekolojisinde hayatta kalan her türden deterministik olarak en fazla bir temsilciyi seçer. Bu, nüfustaki bütün bireylerin ayrı 3B hayvan olarak çizildiği anlamına gelmez.

**Sonuç:** Tür parametreleri ve kapalı çevrim yürüyüş prototipi mevcut; dört türün bağımsız, tam 3B biyomekanik modelleri henüz tamamlanmış değildir. İlk D3 referans tür olarak kızıl geyik için 3B kinematik/enerji doğrulaması yapılmalı; ardından aynı model standardı diğer üç türe uygulanmalıdır.

### 4.4 Algı ve davranış

- Güncel ajan modeli görsel görüş alanı/menzili, en yakın kaynak/tehdit ve utility tabanlı ihtiyaç davranışlarını kapsar.
- Modelde gerçek koku alanı, koku izi/plume takibi, türlere göre işitsel algı, çevresel ses maskelemesi ve geniş ölçekli görüş çizgisi/örtülme algısı doğrulanmış değildir.
- Hafıza/öğrenme sınırlıdır; sosyal gruplar, bireysel geçmiş, bölge/sosyal hiyerarşi ve tür içi etkileşimler nihai kapsamda değildir.

**Sonuç:** Temel algı ve ihtiyaç döngüsü vardır; hayvanların ormanda gerçekçi biçimde bilgi toplaması için çok duyulu algı ve bağlama bağlı davranış katmanları gereklidir.

### 4.5 Ekoloji ve türler arası ilişkiler

P7 ekoloji model kartı kendi kapsamını açıkça sınırlar:

- Standalone model 300 m kenarlı homojen bir yamadır; bitki kaynağı 24 × 24 ızgaradır.
- Canlı oturum bu modeli 256 m odak yaması için yapılandırır.
- Kaynak tek bir soyut bitki biyokütlesidir.
- İlk besin ağı tek kenardır: tilki → tavşan.
- Model kartı mevsimsellik, habitat mozaiği, sosyal yapı, göç dalgaları, hastalık ve tür içi rekabetin dışarıda olduğunu yazar.
- Parametreler saha verisine kalibre edilmemiştir; kart açıkça D2 der.
- 12 seed × 3.600 simülasyon saniyesi koşusu sayısal kararlılık/kalıcılık testidir; tek başına gerçek popülasyon büyüklüklerinin doğru tahmin edildiğini kanıtlamaz.

Tür bazında önemli boşluklar:

- Kızıl geyik: habitat seçimi, gündüz/gece örtü-yem dengesi, ot/browse mevsimselliği, sürü yapısı ve yavru davranışı.
- Kızıl tilki: tavşan dışında küçük memeliler ve omurgasızlar gibi diyet çeşitliliği, mevsim ve kaynak durumuna bağlı av değiştirme.
- Avrupa tavşanı: çok girişli warren/yuva sistemi, sosyal gruplar ve hiyerarşi.
- Yaban domuzu: kök/yeraltı besinleri, meşe palamudu/mast, omurgasızlar, mevsimsel omnivori ve rooting/toprak karıştırma etkisi.

**Sonuç:** Dört tür ve nüfus/enerji/av-avcı iskeleti bulunur. Ancak mekânsal habitat mozaiği, türe özgü besin ağları ve bitki-toprak-hayvan geribildirimiyle çalışan 2 km ekosistemi henüz mevcut değildir.

### 4.6 Sinematik yönetmen

P9 model kartı ve kodun anlattığı işleyiş:

- Yönetmen yalnız gerçek olay akışını okur; dünyaya yazmaz.
- Gerçek yaşam olayları şu anda ATE, BORN, DIED ve KILLED ile sınırlıdır.
- Olay türü dört sabit çekim sınıfına eşlenir: av için takip, ölüm için yakın plan, doğum için orta plan, otlama için geniş plan.
- Mevcut çekim bir süre tutulur ve önem puanı eşiği aşılırsa yeni olaya geçilir.
- Planlar arası geçiş sert kesmedir.
- Yönetmen tek konuyu izler; birden fazla hayvanın ilişkisini aynı kadrajda kurma ve olay olmayan doğal yaşamı düzenli bir çevre planı olarak sunma sınırlıdır.
- Occlusion kontrolü yükseklik alanına dayanır; ağaç/bitki/nesne geometri örtüşmesini tam çözmez.

**Sonuç:** Gerçek olay tabanlı yönetmen çekirdeği iyi bir temeldir; ancak doğal yaşam belgeseli kamerası değildir. Olay olmayan uzun sakin zamanlar, çevre/geniş planlar, grup ilişkileri ve süreklilikli kamera hareketi ayrı kabul ölçütleri olmalıdır.

### 4.7 Ses

Mevcut ws_audio.c uygulaması:

- Av: 180 Hz; ölüm: 120 Hz; doğum: 660 Hz; otlama: 320 Hz sabit frekanslı sinüs osilatörleri.
- Kısa attack/decay zarfı, 32 ses kaynağı, basit stereo pan.
- Pan yalnız olayın x koordinatından gelir.
- WAV dışa aktarma 48 kHz stereo PCM üretir.
- Canlı platform ses aygıtı çıkışı henüz uygulanmamıştır.

Henüz bulunmayanlar:

- Sürekli çevresel ses tabakaları: rüzgâr, yaprak, yağmur, dere/nehir ve habitat ambiyansı.
- Tür ve davranışa özgü sesler; adım, solunum, beslenme, uyarı, sosyal çağrı.
- Mesafe, yön, engel, zemin, bitki yoğunluğu, nem/sıcaklık ve yansıma etkileri.
- İşitme eşiği, arka plan gürültüsü/maskeleme ve sesin hayvan kararlarına etkisi.
- Uygulama içinde gerçek zamanlı ses aygıtı çıkışı ve aygıt kaybında güvenli devam.

**Sonuç:** Şu anki sistem deterministik olay-tonu/WAV prototipidir; doğal orman ses manzarası veya mekânsal akustik sistemi değildir. Ürün sözleşmesinin 8. bölümünde ses kabulü açıkça yazılmadığından, yalnız mevcut testler geçilerek ses hedefinin atlanması riski vardır. Bu PR, mevcut docs/10 ve kullanıcı deneyimi hedefiyle uyumlu bir ses kabul maddesi eklemeyi önerir.

### 4.8 Render ve performans

- Ürün, kendi C17 CPU renderer'ını kullanır; bu mimari karar korunmalıdır.
- P12 ölçüm raporunda 1920×1080 terrain_day yaklaşık 150–194 ms/kare, yani 5,5–6,7 FPS; 3840×2160 yaklaşık 651–703 ms/kare, yani 1,4–1,5 FPS olarak raporlanır.
- Sözleşmedeki 4K/30 FPS için kare bütçesi 33,3 ms'dir. Mevcut 4K ölçümünün bu bütçeye yaklaşmadığı açıktır.
- Etkileşimli pencere ölçeği CPU maliyetini sınırlamak için otomatik küçültülür. Bu, büyük bir pencerede görülen görüntünün aynı çözünürlükte native render edildiği anlamına gelmez.
- OpenCL kernel'i yalnız flat-shaded triangle raster testidir. Her piksel work-item'i üçgen listesinin tamamını dolaşır; bu nedenle sahne karmaşıklığı büyüdükçe piksel × üçgen iş yükü oluşur.
- P10 belgesi tam sahne GPU raster/pixel-buffer entegrasyonunun sonraki dilime bırakıldığını ve gerçek GPU donanımında performans ölçülmediğini açıkça belirtir. Son CI'daki OpenCL işi pocl CPU ICD'de kernel derleme/çalıştırma kanıtıdır; gerçek GPU hızlanma kanıtı değildir.
- **Güncelleme (PR #64/#66 ve PR #67):** Tam sahne OpenCL C 1.2 gölgeleme yolu (`kernels/ws_scene.cl`) artık uygulanmıştır ve canlı ürün döngüsüne opt-in bağlanmıştır. Gerçek GPU donanımı mevcuttur (izole Windows runner, NVIDIA GeForce RTX 3060 Laptop GPU, OpenCL 3.0 CUDA, 6 GB). `--live-bench 600 --live-gpu --size 3840x2160` ile **canlı ürün döngüsü** (dünya adımı + ekoloji + aktörler + kamera + render + HUD) native 4K'da 10 dakika ölçüldü: `path=gpu p95_ms=21.96` (51,2 FPS), `cpu_fallback_frames=0 gpu_errors=0` — 33,3 ms kabul bütçesi bu cihazda karşılanır ve artık zorunlu CI geçididir. Bu, "GPU test kernel'i başarılı olması tam ürün renderer'ının GPU'ya taşındığı anlamına gelmez" itirazını artık karşılar. Açık kalan: ölçüm `shadows=0` ile ve 6 GB cihazda alındı; 12 GB sınıfı referans donanım ve gölge-açık canlı ölçüm ayrıca üretilmelidir.
- PBR benzeri malzeme, yüksek ayrıntılı özgün mesh, doğal kürk/deri, çok seviyeli bitki ve mesh LOD, yoğunluk/occlusion culling, güncel hareketli nesne gölgeleri ve atmosferik kalite henüz ürün hedefi seviyesinde değildir.

**Sonuç:** P12 gerçek bir blokajdır. 4K/30 FPS yalnız bir benchmark sayısı değildir; tam ürün sahnesi, animasyon, gölge, bitki, UI ve canlı dünya yüküyle ölçülmelidir. GPU test kernel'inin başarılı olması, tam ürün renderer'ının GPU'ya taşındığı anlamına gelmez.

### 4.9 Test kalitesi ve yönetişim

- Son main CI ve Windows GUI akışları başarılıdır; bu olumlu ve somut kanıttır.
- Deterministik golden görseller, aynı kodun aynı kareyi üretmesini kanıtlar; tek başına estetik/doğal görünüm kalitesini kanıtlamaz. docs/12 de bunu açıkça belirtir ve ayrıca görsel kalite kontrol listesi/referans kabul protokolü ister.
- Ekoloji testleri sayısal sınır ve kararlılığı kanıtlar; ölçülmüş popülasyon zaman serisiyle nicel D3 doğrulaması değildir.
- Branch protection/ruleset konusu hâlâ ayrı bir yönetişim riskidir. Mevcut repo belgeleri bu API erişiminin 403 verdiğini söylüyor; bu denetimde de bağlı GitHub entegrasyonuyla branch-protection okuması 403 döndü. Dolayısıyla zorunlu kontrollerin GitHub sunucu tarafında gerçekten kilitlendiği bu denetimde doğrulanamadı.
- docs/MILESTONE-BOARD.md ve docs/CI-VE-DAL-KORUMASI.md GitHub-hosted runner'ların 10 Kasım 2026'da hâlâ engelli olduğunu söylüyordu; ancak 11 Kasım 2026 tarihli run 42974024398'de Linux GCC/Clang, Windows MSVC, OpenCL, plan ve docs işleri başarılı. Bu denetimde her iki belgedeki eski engel notu tarihsel olaya dönüştürüldü; güncel durum olarak sunulmuyor.

## 5. Araştırma bulguları ve model tasarımına etkileri

Aşağıdaki çalışmalar doğrudan parametreleri koda kopyalama talimatı değildir. Model kartları, eksik mekanizmalar ve doğrulama deneyleri için dayanak sağlar.

### 5.1 Biyomekanik

Modern tetrapod locomotion araştırması; 3B hareket yakalama, XROMM, kuvvet/temas verisi ve dinamik kas-iskelet modellerinin birbirini tamamladığını gösterir. Sadece 2B eklem açıları veya önceden tanımlı adım salınımı, üç boyutlu hayvan mekaniğini doğrulamaz.

Kızıl geyik için 1977 treadmill çalışmasında üç erkek hayvandan alınan ölçümlerde yatay yürüme maliyeti yaklaşık 2,6 J kg⁻¹ m⁻¹, dikey tırmanış maliyeti yaklaşık 21,5 J kg⁻¹ dikey metre olarak raporlanmıştır. Örneklem küçük ve deney koşulları sınırlıdır; değerler doğrudan evrensel sabit yapılmamalı, D3 kıyas hedefi olarak ve belirsizlik aralığıyla ele alınmalıdır.

Kaynaklar:
- McDonald ve ark. (2023), Modern three-dimensional digital methods for studying locomotor biomechanics in tetrapods: https://doi.org/10.1242/jeb.245132
- Brockway & Gessaman (1977), The energy cost of locomotion on the level and on gradients for the red deer: https://doi.org/10.1113/expphysiol.1977.sp002405

### 5.2 Kızıl geyik davranışı

GPS tabanlı iki popülasyon araştırması kızıl geyiklerde alacakaranlık aktivite zirveleri bulur; ancak gündüz/gece dağılımının popülasyon, insan rahatsızlığı, mevsim ve yerel hava koşullarına göre farklılaşabildiğini gösterir. Bu nedenle sabit saatlerde otomatik “geyik davranışı” yerine ışık, habitat, rahatsızlık ve bireysel farklılığa bağlı olasılıksal aktivite uygundur.

Norveç'te 62 dişi geyiğin telemetri verileriyle yapılan habitat seçimi çalışmasında, hayvanlar aktif beslenirken yem zengini açık alanları; dinlenme dönemlerinde ise örtü sağlayan ormanı daha fazla kullanmıştır. Habitat seçimi yalnız en yakın besin noktasına gitme davranışı olmamalıdır.

Kaynaklar:
- GPS Based Daily Activity Patterns in European Red Deer and North American Elk: https://pmc.ncbi.nlm.nih.gov/articles/PMC4160215/
- Godvik ve ark. (2009), Temporal scales, trade-offs, and functional responses in red deer habitat selection: https://doi.org/10.1890/08-0576.1

### 5.3 Türlerin diyet ve sosyal ekolojisi

- Kızıl tilki diyet meta-analizi 5 kıtada 217 çalışma ve 276 lokasyonu birleştirir; diyetin belirgin biçimde değiştiğini ve küçük memelilerle omurgasızların sık görüldüğünü gösterir. Yalnız tilki→tavşan kenarı genel tür davranışı için yeterli değildir.
- Avrupa tavşanı vahşi ortamda çok girişli warren çevresinde sosyal gruplar ve hiyerarşiler oluşturabilir; yalnız bağımsız bireyler olarak dolaştırılması sosyal ekolojiyi atlar.
- 2026 tarihli 470 yaban domuzu mide içeriği çalışması, yaprak/üst yüzey kaynaklarının ilkbahar-yaz, yeraltı organlarının sonbahar-kış dönemlerinde öneminin değiştiğini gösterir.
- 2026 rooting derlemesi, yaban domuzu toprak karıştırmasının bitki örtüsü, toprak organizmaları, besin döngüsü ve orman yenilenmesine etkisinin yoğunluk, habitat, toprak nemi ve mevsime bağlı olduğunu raporlar.

Kaynaklar:
- Red fox diet review (2022): https://doi.org/10.1111/mam.12292
- The Social Nature of European Rabbits (2016): https://pmc.ncbi.nlm.nih.gov/articles/PMC5113872/
- Wild boar seasonal diet (2026): https://doi.org/10.3390/ani16172688
- Wild boar rooting review (2026): https://doi.org/10.1016/j.foreco.2026.124061

### 5.4 Algı ve duyular

Memeli koku sistemi, uzak koku ipuçlarını ve koku plume/iz takibini kullanabilir; özellikle gece/alacakaranlık aktif memeliler için koku bilgisi önemli olabilir. 2024 hayvan karar verme derlemesi, doğal kararların dahili durum, bellek, sosyal ipuçları ve çevresel duyuların birlikte işlenmesiyle anlaşılması gerektiğini vurgular. Bu, yalnız görsel FOV ve en yakın kaynak seçiminin nihai davranış modeli olamayacağını gösterir.

Kaynaklar:
- Marin, Schaefer & Ackels (2021), Spatial information from the odour environment in mammalian olfaction: https://doi.org/10.1007/s00441-020-03395-3
- Goldshtein & Yovel (2024), Onboard Sensors Reveal New Insights into Animal Decision-Making: https://www.annualreviews.org/content/journals/10.1146/annurev-ecolsys-102722-125640

### 5.5 Orman ekolojisi ve kalibrasyon

Orman gap-model literatürü, ormanı farklı yaş/süksesyon aşamalarındaki yamalar halinde modellemenin yararlı olduğunu; ancak yamaların yatay homojen kabul edilmesinin ve yamalar arası etkileşimlerin ihmalinin sınırlama olduğunu tartışır. Worldseed'in mevcut statik yerleşim ve tek biomass grid yapısı, orman büyümesi/rekabet modeli olarak sunulmamalıdır.

2026 ekolojik model kalibrasyon ve belirsizlik derlemesi, parametre belirsizliği, model yapısı belirsizliği, duyarlılık analizi ve kalibrasyonun birlikte ele alınmasını önerir. 2024 avcı-av modeli çalışması, carrying capacity ve fonksiyonel tepki parametrelerinin gelişigüzel seçilmemesi, saha verisi ve biyolojik ölçütlerle kıyaslanması gerektiğini gösterir.

Kaynaklar:
- Bugmann, A review of forest gap models: https://doi.org/10.1023/A:1012525626267
- Malchow & Hartig (2026), Calibration, Sensitivity and Uncertainty Analysis of Complex Ecological Models: https://doi.org/10.1111/ele.70375
- Ecological Modelling (2024), Methods for comparing theoretical models parameterized with field data using biological criteria and Sobol analysis: https://doi.org/10.1016/j.ecolmodel.2024.110728

### 5.6 Ses ve dış ortam akustiği

Karasal ekolojide arka plan sesleri yalnız kullanıcıya işitsel atmosfer değildir; hayvanların sinyal algısını, davranışını ve topluluk ilişkilerini etkileyebilir. Ormanda ses yayılımında geometrik uzaklık, atmosferik soğurma, zemin, yansıma, engel ve bitki örtüsü önemlidir. Bu nedenle yalnız x eksenine göre stereo pan, mekânsal akustik değildir.

ISO 9613-2:2024, dış mekân ses yayılımı için geometrik yayılma, atmosferik soğurma, zemin etkisi, yansıma ve engel ekranlamasını mühendislik hesaplarına dahil eder; yaprak/bitki yayılımı için ek bilgi de verir. Bu standart doğrudan hayvan duyma/orman biyolojisi modeli değildir; ancak fiziksel ses iletimi için iyi bir başlangıç çerçevesidir. Ayrıca ormanlarda orta/yüksek frekanslı seslerin 40 m üzerindeki yayılımında ağaç yapısının etkisini ölçen deneysel çalışmalar vardır.

Kaynaklar:
- Francis, Phillips & Barber (2023), Background Acoustics in Terrestrial Ecology: https://doi.org/10.1146/annurev-ecolsys-102220-030316
- ISO 9613-2:2024: https://www.iso.org/standard/74047.html
- Tarrero ve ark. (2008), Sound propagation in forests: https://doi.org/10.1016/j.apacoust.2007.01.007

### 5.7 Render mimarisi

Khronos OpenCL kaydı, OpenCL C 1.2 yürütme/iş grubu modeli için resmi spesifikasyon kaynağıdır. Projenin OpenCL 1.2 sınırı korunabilir. Ancak mevcut kernel'in her pikselde bütün üçgenleri dolaşması, gerçek sahne ölçeğinde maliyetlidir. Tam sahne yoluna geçmeden önce üçgenleri ekran tile'larına bölme, tile başına sınırlı görünür üçgen listesi ve deterministik CPU/GPU semantik doğrulama gibi aşamalı yöntemler araştırılmalıdır. Bu bir teknoloji değişikliği değil, mevcut OpenCL C 1.2 sınırında algoritma geliştirme konusudur.

Kaynak:
- Khronos OpenCL Registry ve 1.2 spesifikasyonları: https://registry.khronos.org/OpenCL/

## 6. Risk ve öncelik sıralaması

| Öncelik | Risk | Neden önemli | Önerilen çözüm |
|---|---|---|---|
| P0 / Blokaj | 4K/30 FPS ve tam sahne renderer entegrasyonu yok | 1.0 görsel/perf release kapısı karşılanmıyor | **Kısmen çözüldü (PR #64/#66/#67):** tam sahne OpenCL C 1.2 yolu canlı döngüye bağlı ve gerçek GPU'da native 4K `p95 ≈ 22 ms` (51 FPS) ölçüldü; kalan: 12 GB sınıfı referans donanım ve gölge-açık ölçüm |
| P0 / Blokaj | Biyomekanik yalnız sagittal ortak model | Dört türde 3B beden/kontrol hedefi karşılanmıyor | 3B bağımsız uzuv/temas/denge, önce kızıl geyik D3. **Kısmi ilerleme:** frontal düzlem (lateral) denge/dönüş modeli dört tür için eklendi (`src/life/ws_lateral.*`); tam 3B çözücü açık |
| P0 / Blokaj | Ekoloji homojen ve kalibre edilmemiş | 2 km habitat mozaiği ve besin ağı hedefi karşılanmıyor | Tür/habitat/bitki katmanlarını bağla; D3 referans kıyası |
| P1 / Yüksek | Ses olay-tonu düzeyinde | Orman/animal soundscape ürün deneyimi yok | Çevre tabakaları, tür sesleri, yayılım ve canlı cihaz çıkışı |
| P1 / Yüksek | Yönetmen dar olay/tek konu/hard-cut davranışında | Doğa belgeseli anlatımı ve sakin gözlem eksik | Ortam planları, çok aktör, çekim geçişleri, event continuity |
| P1 / Yüksek | Testler deterministik, fakat kaliteyi ölçmüyor | Test yeşili görsel/biyolojik gerçekçilik kanıtı değil | Ayrı görsel ve bilimsel kabul protokolleri |
| P1 / Yüksek | Milestone ve canonical roadmap ayrışması | OpenHands dar checklist'leri ürün yönü sanabilir | Bu PR'daki yol haritası eklemesi |
| P2 / Orta | Branch protection durumu doğrulanamadı | Sunucu tarafı merge zorunlulukları teyit edilemiyor | Yönetici/API erişimi olduğunda ruleset'i doğrula |
| P2 / Orta | CI runner engel notu güncel değil | Yanlış durum raporu ve karar riski | 11 Kasım başarılı matris koşularıyla notu güncelle |

## 7. Önerilen geliştirme sırası: P12–P18

Aşağıdaki sıra, sabit C17/OpenCL C 1.2 mimarisini ve mevcut kodu koruyan bir öneridir. Her aşama ayrı PR/task'lara bölünmeli; aynı anda tek ana PR açık tutulmalıdır. Bir aşamanın dokümantasyon işareti, 1.0 ürün kabulünü geçtiği anlamına gelmez.

### P12 — Render performansı ve gerçek sahne kalitesi (ilk öncelik)

Amaç: 4K/30 FPS hedefini tam ürün sahnesinde karşılamak; yalnız test triangle kernel'ini hızlandırmış olmakla yetinmemek.

Kabul kapıları:
- CPU referans renderer'ı tüm sahne bileşenleri için doğruluk/fallback olarak kalır.
- OpenCL C 1.2 yolu tam sahne geometry/material/depth/lighting/shadow workload'una entegre edilir; gerçek GPU donanımında ölçülmeden GPU hızlanması iddia edilmez.
- Ekran tile binning, LOD/instancing, culling ve bellek aktarımı gibi algoritmalar ölçülür; sonuçlar CPU/GPU semantik sınırlarıyla karşılaştırılır.
- 3840×2160 native çıktı, referans donanımda en az 10 dakikalık ölçümde p95 frame-time ≤33,3 ms hedefini geçer. Ölçüm cihazı, sürücü, scene seed, kalite profili ve ham frame-time verisi kaydedilir.
- 6 ürün referans sahnesi (orman/nehir geniş planı, çayır hayvanları, su kenarı etkileşimi, yakın beden, yağmur/sis, gece) görsel kalite kontrol listesiyle değerlendirilir.
- Görsel regresyon hash/SSIM/PSNR ile ölçülebilir; ancak bunların yanında geometri, malzeme, ışık, gölge, bitki, atmosfer ve hareket için ayrı kalite protokolü bulunur.
- Kendi C17 varlık hattı yüksek ayrıntılı özgün bitki/hayvan içeriğine doğru genişler; dışarıdan hazır oyun/render/asset paketi eklenmez.

### P13 — 3B tür biyomekaniği

Amaç: mevcut düzlemsel prototipten gerçek 3B, türlerin anatomisine göre ayrışan fiziksel beden/kontrol sistemine geçmek.

Kabul kapıları:
- Ön ve arka bacaklar ile sağ/sol taraf birbirinden bağımsız 3B state ve temas kullanır.
- Gövde kütle/ataleti, eklem sınırları, temas kuvveti, ağırlık merkezi, denge, yaw/roll/pitch ve dönüş dinamiği açıkça modellenir.
- Yürüme, hızlı yürüyüş/trot, koşu, dönüş, eğim ve farklı zemin senaryoları için ölçülebilir gait/temas testleri bulunur.
- Önce kızıl geyik için referans hareket ve enerji verileriyle D3 kıyas yapılır; veri olmayan parametreler varsayım ve belirsizlik aralığı olarak kalır.
- Sonraki türlerin vücut oranı, kütle/atalet, ekstremite, ayak/pençe/toynak ve gait profilleri birbirinden ayrılır.
- Görsel beden her zaman fiziksel state'ten türetilir; animasyon pozu fiziksel gerçeğin yerine geçmez.

**İlerleme (2026-11):** İlk dilim tamamlandı — frontal düzlem (lateral) denge,
dört ayak teması ve dönüş modeli dört tür için eklendi ve CI'da kapılanıyor
(`docs/life/MODEL-KARTI-FRONTAL-DENGE.md`, `src/life/ws_lateral.*`,
`tools/ws_lateral_check.c`, `tests/test_lateral.c`). Bu model bağımsız dört ayak
teması (duruş/salınım, yük korunumu), yük-ağırlıklı destek merkezi, LIPM +
yakalama-noktası ayak yerleştirme, sönümlü yalpa ve tür-ölçekli dönüş sınırını
**azaltılmış mertebeden** içerir ve sagittal çözücüye yazmaz. Yukarıdaki
kapıların tam 3B kısmı (bağımsız 3B uzuv eklem state'i, dinamik temas kuvveti,
3B denge/enerji D3 kıyası, tür-özel 3B gait doğrulaması) **açıktır**.

### P14 — Çok duyulu algı ve doğal davranış

Amaç: hayvanların yalnız en yakın görsel hedefe yönelmediği, tür ve çevreye özgü algı/karar modeli.

Kabul kapıları:
- Görüş, görüş çizgisi/örtülme, işitme ve koku algısı ayrı model bileşenleridir.
- Ses/koku ipuçlarının mesafe, rüzgâr, ortam gürültüsü ve engellerden etkilenmesi test edilir.
- İhtiyaç, yaş, enerji, sağlık, geçmiş deneyim, sosyal grup ve habitat durumu karar girdileri olarak belgelenir.
- Davranış testleri farklı seed, hava, günün saati, tehdit ve kaynak dağılımıyla yürütülür.
- Öğrenme iddiaları ölçülebilir bir görev performansı değişikliği göstermedikçe sınırlı/heuristic olarak etiketlenir.

### P15 — 2 km habitat mozaiği ve ekolojik döngüler

Amaç: mevcut tek tip resource patch modelini dört bağlantılı habitatta yaşayan heterojen ekosisteme dönüştürmek.

Kabul kapıları:
- 2.048 m × 2.048 m referans dünyada orman, çayır/açıklık, tatlı su kenarı ve kayalık/yamaç habitatları harita üzerinde ayrışır ve birbirine bağlanır.
- Bitki tür/işlev grupları, mevsimsel fenoloji, büyüme/ölüm/yenilenme, kaynak ve habitat taşıma kapasitesi açıkça modellenir.
- Kızıl geyik örtü/yem zaman dengesi; tilki alternatif av/diyet; tavşan sosyal grup/warren; yaban domuzu mevsimsel omnivori/rooting en azından V1 için ölçülebilir asgari davranış olarak uygulanır.
- Avcı-av ilişkisi yalnız tek kenara bağlı kalmaz; fonksiyonel tepki ve carrying capacity parametreleri gerekçeli veri aralıklarıyla kalibre edilir.
- Çoklu seed ve çok yıllı/çok mevsimli uzun koşular; popülasyon, kaynak, doğum/ölüm, habitat kullanımı ve enerji bütçesi raporları üretir.
- Her model kartı D0–D4 kanıt seviyesini, parametre kaynağını, belirsizliği, geçerlilik alanını ve karşılaştırma sonucunu açıkça gösterir.
- D3 veri yoksa model tahmin olarak sunulmaz; davranış/prosedürel simülasyon olduğu belirtilir.

### P16 — Doğal ses manzarası ve canlı mekânsal ses

Amaç: olaylara bağlanmış birkaç sinüs tonundan orman ve hayvan ses dünyasına geçmek.

Kabul kapıları:
- Sürekli çevresel ses katmanları (rüzgâr/yaprak, yağmur, akarsu/su, habitat ambiyansı) vardır.
- Tür ve davranışa özgü hayvan sesleri, adım/nefes/beslenme/uyarı/sosyal ses olayları gerçek simülasyon state/event'lerinden türetilir.
- Mesafe, yön, kaynak directivity, frekans, atmosferik soğurma, zemin ve engel/bitki yayılımı test edilir.
- İşitme modeli varsa hayvan davranışına etkisi ayrı test edilir; maskeleme/arka plan sesi varsayımları belgelenir.
- Windows uygulamasında gerçek zamanlı ses aygıtı çıkışı bulunur; cihaz kaybı/dolup taşması simülasyon adımını bloke etmez.
- Aynı seed aynı kaynak/event dizisinde deterministik WAV üretmeye devam eder.
- Duyusal/akustik kalibrasyon bulunmayan tür/senaryolarda iddia D2 veya daha düşük seviyede tutulur.

### P17 — Doğal gözlem ve sinematik yönetmen

Amaç: yalnız olay anlarını değil, olay öncesi/sonrası ve sakin doğal yaşamı da doğru gözlemleyen yönetmen.

Kabul kapıları:
- Yönetmen gerçek event ve read-only snapshot tüketmeye devam eder; dünya state'ine yazamaz ve olay uyduramaz.
- Çok aktörlü kadraj (av-avcı, grup, anne-yavru, türler arası karşılaşma) ve habitat/çevre planları vardır.
- Olay olmayan normal yaşam için düşük öncelikli/ortam gözlemi vardır; sahte olay oluşturulmaz.
- Geniş/orta/yakın/takip ve çevre planları arasında kamera sürekliliği, hız/ivme ve örtülme ölçülür.
- Kamera gerçek sahne geometrisini/engelleri görür; hayvanı göstermek için ağaç/bitkiyi yapayca kaldırmaz.
- Replay, shot/event ID, dünya zamanı ve ses zamanını aynı zaman çizelgesinde ilişkilendirir.

### P18 — Entegrasyon ve V1.0 kabulü

Amaç: tüm alt sistemlerin birlikte gerçek üründe kanıtlanması.

Kabul kapıları:
- Sözleşmedeki 2.048 m × 2.048 m dünya ve dört bağlı habitat gerçek uygulamada.
- Dört türün doğrulanmış farklı 3B fiziksel model/kontrolü.
- Çoklu habitatta kaynak/bitki/avcı-av/üreme/ölüm süreçleri.
- Kamera dışında ilerleme, kalıcılık, checkpoint ve replay.
- Gerçek olayları gözlemleyen çok aktörlü sinematik yönetmen.
- Doğal çevresel + hayvan sesleri, mekânsal sunum ve canlı aygıt çıkışı.
- Altı görsel referans sahnesi kalite protokolünü geçer.
- Referans donanımda native 4K/30 FPS p95 kapısı ve daha düşük donanım CPU fallback profili kanıtlanır.
- Otomatik unit/integration/scientific regression, uzun süre/çok seed, GUI, package, security ve clean-install kapıları başarılıdır.
- Hiçbir aşama, kullanıcıdan normal manuel test istemez.

## 8. Önerilen durum etiketleri

Milestone panosunda aşağıdaki ayrım kullanılmalıdır:

- **Prototip tamamlandı:** dar teknik amaç ve kendi testleri geçti.
- **Alt sistem dilimi tamamlandı:** belgelenmiş alt kapsam tamamlandı.
- **Ürün sözleşmesi kabulü açık:** ilgili 1.0 ürün hedefi henüz karşılanmadı.
- **Ürün kabulü geçti:** 00-URUN-SOZLESMESI.md bölüm 8 ve ilgili kalite kapıları kanıtlandı.

Bu ayrım P5/P7/P9/P10 gibi prototipleri değersizleştirmez; yalnız “prototipin çalışması” ile “ürün gerçekçiliğinin tamamlanması”nı karıştırmayı önler.

## 9. OpenHands çalışma talimatı için öneri

- Önce mevcut çalışma alanını ve git durumunu raporla; commit edilmemiş değişiklikleri koru.
- Bu denetim PR'ını, docs/00 sözleşmesini, docs/13 yol haritasını ve MILESTONE-BOARD durumlarını birlikte oku.
- Aynı anda tek ana PR ilkesini koru; bu PR incelenmeden ikinci ana PR açma.
- Ürün önceliği P12 render/performance ve kalite kapılarıdır. Yeni UI/kamera konfor özelliği, P12'nin eksik ürün kapılarını ertelemek için gerekçe olmasın.
- P12'de gerçek GPU donanımına erişim yoksa GPU hızı iddia etme; gerçek GPU ölçümünü açık bir blokaj olarak kaydet ve CPU fallback yolunu geliştirmeye devam et.
- Yeni biyolojik parametreyi ölçülmüş veri gibi sunma. Model kartında kaynak, birim, belirsizlik, geçerlilik alanı ve test referansını yaz.
- 1.0.0 öncesinde ses, üç boyutlu biyomekanik, habitat-ekoloji ve görsel kalite kabul kapılarının hepsi kanıtlanmalıdır.

## 10. Kaynak ve araştırma bibliyografyası

Aşağıdaki bağlantılar 11 Kasım 2026 tarihinde kontrol edilmiştir. Bunlar proje dokümantasyonu için başlangıç kaynaklarıdır; her yeni parametre/iddia için özgün makale ve geçerlilik alanı ayrıca incelenmelidir.

1. McDonald ve ark. (2023), 3B tetrapod locomotion yöntemleri, Journal of Experimental Biology: https://doi.org/10.1242/jeb.245132
2. Brockway & Gessaman (1977), kızıl geyik yürüme/tırmanma enerji maliyeti: https://doi.org/10.1113/expphysiol.1977.sp002405
3. GPS tabanlı kızıl geyik/elk günlük aktivite örüntüleri: https://pmc.ncbi.nlm.nih.gov/articles/PMC4160215/
4. Godvik ve ark. (2009), kızıl geyikte habitat seçimi, yem/örtü dengesi: https://doi.org/10.1890/08-0576.1
5. Castañeda ve ark. (2022), beş kıtada kızıl tilki diyet varyasyonu: https://doi.org/10.1111/mam.12292
6. DiVincenti & Rehrig (2016), Avrupa tavşanlarının sosyal doğası: https://pmc.ncbi.nlm.nih.gov/articles/PMC5113872/
7. Nasiadka ve ark. (2026), 470 yaban domuzunda mevsimsel üst/alt yüzey bitki besini kullanımı: https://doi.org/10.3390/ani16172688
8. Cai ve ark. (2026), orman ekosisteminde yaban domuzu rooting etkileri derlemesi: https://doi.org/10.1016/j.foreco.2026.124061
9. Marin, Schaefer & Ackels (2021), memelilerde koku uzamsal bilgisi: https://doi.org/10.1007/s00441-020-03395-3
10. Goldshtein & Yovel (2024), hayvan karar vermede onboard sensory systems: https://www.annualreviews.org/content/journals/10.1146/annurev-ecolsys-102722-125640
11. Bugmann, A review of forest gap models: https://doi.org/10.1023/A:1012525626267
12. Malchow & Hartig (2026), ekolojik model kalibrasyonu/duyarlılık/belirsizlik: https://doi.org/10.1111/ele.70375
13. Ecological Modelling (2024), biyolojik kriter ve Sobol duyarlılık analizi ile predator-prey model kıyaslama: https://doi.org/10.1016/j.ecolmodel.2024.110728
14. Francis, Phillips & Barber (2023), karasal ekolojide arka plan akustiği: https://doi.org/10.1146/annurev-ecolsys-102220-030316
15. ISO 9613-2:2024, dış ortamda ses yayılımının zayıflaması: https://www.iso.org/standard/74047.html
16. Tarrero ve ark. (2008), ormanlarda ses yayılımının deneysel ölçümü: https://doi.org/10.1016/j.apacoust.2007.01.007
17. Khronos OpenCL kayıt/specifikasyonları: https://registry.khronos.org/OpenCL/
18. Khronos OpenCL 1.2 API specification: https://registry.khronos.org/OpenCL/specs/opencl-1.2.pdf
19. Bugmann & Seidl (2022), The evolution, complexity and diversity of models of long-term forest dynamics: https://doi.org/10.1111/1365-2745.13989

---

**Denetim sonucu:** Worldseed'in mühendislik iskeleti ve tekrarlanabilir geliştirme zinciri korunmalı. Ürünün nihai hedefi için P12 ve sonrasındaki gerçekçilik alanları henüz açık. Bu rapor, OpenHands'in küçük checklist'leri tamamlamaya devam ederken asıl 1.0 kalite hedeflerini gözden kaçırmaması için hazırlanmıştır.
