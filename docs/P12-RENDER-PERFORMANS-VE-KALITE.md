# P12 — Render performansı ve görsel kalite

Bu belge P12 aşamasının durumunu, uygulanan kodu, otomatik kanıtları ve bilinen
sınırlamaları kaydeder. Kapsam `docs/08-CIZICI-VARLIK-HATTI.md` (çizici ve görsel
kalite) ve `docs/11-PERFORMANS-DONANIM.md` (performans ve donanım) ile
`docs/13-YOL-HARITASI.md` içindedir.

## Kapsam

- CPU referans renderer'ının kare süresini düşürmek (30 FPS hedefine doğru).
- Değişmezlik korunarak: hızlanma görüntüyü **bit-özdeş** bırakmalı.
- Görsel kaliteyi yükseltmek: yönlü (directional) gölge eşlemesi.
- Yeni özellikler mevcut golden kareleri değiştirmemeli (opt-in, eklemeli).

## Ne yapıldı

### 1. CPU render hızlandırma (PR #19)

Profil çıkarma iki baskın maliyeti gösterdi: her karede 153M+51M çapraz-çeviri
birimi (TU) matematik yardımcı çağrısı ve `pow`/`exp` sıcak noktaları (her biri
yaklaşık %15). Uygulananlar:

- Küçük matematik yardımcıları (`ws_lerp`, `ws_vec2/3/4_*`, `ws_clamp_double`)
  başlıkta `static inline` yapıldı; çapraz-TU çağrı yükü kalktı.
- 4096 girişlik sRGB kodlama LUT'u (piksel başına kanal başına 3 `pow()`
  çağrısını değiştirir). LUT, paralel bölgeden önce tek seferde ve iş parçacığı
  güvenli biçimde kurulur.
- Çok çekirdekli bant/tarama-satırı paralel render. Bantlar görüntüyü bölüştürür
  ve her piksel yalnız kendi derinlik/renk yuvasına yazar; bu yüzden paralel
  sonuç seri sonuçla **bit-özdeş**tir. Üçgen sayacı yalnız 0. bantta tutulur
  (görüntü geneli istatistik), böylece sayaçlar da banttan bağımsızdır.
- `shade_pixel` içinde normalizasyon uzunlukları bir kez hesaplanır.

**Sonuç:** 1920×1080 `terrain_day` sahnesi 543.78 ms → ~88 ms (yaklaşık 6.2x;
başlangıca göre 12.6x). Zamanlayıcı hızlanması 3.22x (seri 175 ms → paralel
54 ms), `identical=1`. Golden özetleri ve metrikleri değişmedi (yalnız zaman
sütunu farklı). 18/18 test hem release hem ASan/UBSan altında geçti; uygulama
yakalamaları (16 sahne) golden harness ile bit-özdeş.

### 2. Yönlü gölge eşlemesi (bu PR)

`ws_render.c` içine ışığın bakış açısından tek ortografik derinlik haritası
eklendi:

- **Işık izdüşümü:** Sahne nesnelerinin dünya uzayı AABB'lerinden ışık
  frustum'u türetilir (`scene_world_bounds` + `shadow_light_matrix`). Böylece
  kare başına elle ayar gerekmez; ışık merkezin `2*yarıçap` gerisinden merkeze
  bakar ve `[-r, r]` ortografik kutu kullanılır (`ws_mat4_ortho`).
- **Derinlik geçişi:** `shadow_render_pass` ışık uzayında üçgenleri rasterleyip
  en yakın derinliği yazar. Gölge geçişi paralel görüntü geçişinden **önce**
  koşar; bütün görüntü bantları aynı haritayı örnekler. Kurulum da satır
  bantlarına bölünerek aynı işçi havuzunda paralelleştirilir (bkz. §11).
- **Örnekleme:** `shadow_factor` 3×3 PCF yapar; sonuç yalnız saklanan derinliğin
  saf fonksiyonudur, bu yüzden deterministik ve bantlar arası özdeştir. Acne'ye
  karşı ışık uzayı derinlik bias'ı uygulanır.
- **Gölgeleme:** Gölge yalnız **doğrudan** ışığı azaltır (difüz + specular);
  yarım-küre ambient dokunulmaz. Gölgeler kapalıyken çarpan tam 1.0'dır, bu
  yüzden çarpım işlemsizdir ve mevcut sahne/golden yolu bit-özdeş kalır.

`ws_scene.h` ayarlarına `shadows_enabled`, `shadow_strength`, `shadow_bias`,
`shadow_map_size` alanları eklendi; varsayılan **kapalı** (eklemeli, opt-in).

### 3. Yeni golden sahne — `shadows`

Alçak güneş altında yükseltilmiş blok ve küre, zemine gölge düşürür. Sahne
gölge geçişini açar (`shadows_enabled=1`); diğer bütün sahneler kapalı bırakır,
bu yüzden onların golden kareleri etkilenmez. `tests/golden/shadows.ppm` ve
`manifest.csv` satırı eklendi; CI sahne listeleri (Linux + Windows) güncellendi.

## Otomatik kanıt

- **Yeni test:** `render shadow darkens occluded` — yükseltilmiş bir küpün
  altındaki zemini gölgelendirdiğini, hiçbir pikseli aydınlatmadığını
  (`darker > 40`, `brighter == 0`) ve ikinci gölgeli render'ın bit-özdeş
  olduğunu doğrular.
- **Değişmezlik:** gölgeler kapalı 16 sahnenin golden hash'leri ve metrikleri
  bit-özdeş kaldı.
- **Uygulama = harness:** 17 sahnenin uygulama yakalaması golden harness
  çıktısıyla bit-özdeş (`shadows` dâhil).
- **Test sonucu:** 18/18 test release ve ASan/UBSan altında geçer.
- **Reproducibility:** `shadows` sahnesi iki koşuda bit-özdeş.

### 4. Canlı üründe yönlü gölgeler (bu PR)

Gölge eşlemesi artık yalnız bir golden sahne değil, çalışan uygulamada gerçek bir
ürün özelliğidir:

- **Anahtar:** Canlı uygulamada **T** yönlü gölgeleri açar/kapatır. Yardım
  panelinde listelenir. Varsayılan **kapalı**'dır; böylece açılıştaki canlı
  görünüm golden referanslarla uyumlu kalır ve kullanıcı isterse açar.
- **Durum:** Oturum (`ws_session`) `WS_CMD_TOGGLE_SHADOWS` komutunu ve
  `ws_session_shadows_visible()` sorgusunu kazanır. Render sırasında gölge
  ayarı ortam sahnesinin ayarlarına yazılır; dünya durumu değişmez (gözlemci
  salt-okunur kalır).
- **Tur kanıtı:** Deterministik tur iki yeni kare üretir: `step-08-shadows_off`
  ve `step-09-shadows_on` (aynı düşük sabah güneşi, aynı serbest görünüm). Tur,
  ayrıca gölge açık/kapalı iki render'ın farklı olduğunu ve gölgeli karenin
  daha parlak olmadığını doğrular (`shadows_verified=1`).
- **Test:** `session_shadows_toggle` birimi gölgelerin varsayılan kapalı
  olduğunu, açıldığında kareyi koyulaştırdığını ve kapatıldığında kareyi
  bit-özdeş geri getirdiğini doğrular. `ws_imgdiff` tabanlı CI adımı gölge
  çiftinin gerçekten farklı olduğunu kanıtlar.
- **Değişmezlik:** Gölgeler kapalıyken canlı kare ve 16 golden sahne bit-özdeş
  kalır; gölge yalnız eklemeli ve opt-in'dir.

### 5. Döşemeli (tile-binned) OpenCL C 1.2 raster çekirdeği (bu PR)

Referans GPU çekirdeği (`kernels/ws_raster.cl`) her piksel iş öğesinin bütün
üçgen listesini dolaşmasını gerektirir; maliyet piksel × üçgen olarak büyür ve
tam sahneye ölçeklenmez. Ölçüm: 1280×720, 20 000 üçgen → **63.7 s** tek karede.
Bu, P12'nin asıl darboğazıdır.

- **Ana bilgisayar tarafı ikili ayıklama:** Yeni `src/gpu/ws_tilebin.{h,c}` (ISO
  C17) ekran uzayında bir döşeme ızgarası ve döşeme başına, ekran sınır kutusu o
  döşemeye değen üçgen indekslerini üretir. Liste üçgen sırasına göre artan
  tutulur. Bu bir **filtredir**: kutu değen üçgen tutulur, kesin kenar testini
  çekirdek yine yapar.
- **Yeni çekirdek:** `kernels/ws_raster_tiled.cl` (OpenCL C 1.2; yalnız çekirdek
  çekirdek özellikleri, atomik yok). Piksel başına bir iş öğesi kendi döşemesinin
  listesini dolaşır. Aynı kenar-fonksiyonu testi, aynı float barycentric renk/
  derinlik interpolasyonu ve aynı "küçük derinlik kazanır, eşit derinlik üzerine
  yazmaz" kuralı kullanılır.
- **Bit-özdeşlik:** İki çekirdek aynı girdi için **birebir aynı** görüntü ve
  derinlik üretir (testte doğrulandı). Bu, ADR-001'in CPU/GPU semantik
  karşılaştırma güvencesidir.
- **Gömme:** Çekirdek kaynağı hâlâ yapılandırma zamanında gömülür (ADR-001);
  ancak tek C dizesi C99'un 4095 karakter güvencesini aştığı için gömme artık
  satır başına bir dize parçası üretir ve ana bilgisayar bunları tek
  `clCreateProgramWithSource` çağrısında birleştirir.
- **Ana bilgisayar API:** `ws_opencl_raster_tiled(...)` eklendi; aygıt kaybı
  yine CPU geri dönüşüne yol açar (davranış `ws_opencl_raster` ile aynı).

### 6. CPU renderer ölçeklenmesi — paylaşılan köşe hazırlığı ve bant dengeleme (bu PR)

Tam sahne (`terrain_day`) CPU yolunda darboğaz iki katmandı ve ikisi de
**bit-özdeş** biçimde giderildi (görüntü, derinlik, istatistik ve golden
SHA-256 değerleri değişmedi):

- **Kare başına köşe hazırlığı (prepass):** Önceden her görüntü bandı, bütün
  üçgenleri baştan kırpıyor ve ekran uzayına eşliyordu; ölçüm 262 536 köşe
  dönüşümü/kare gösterdi (21 878 üçgen × 4 bant × 3 köşe). Artık her köşe için
  klip dönüşümü, dünya konumu, normal, albedo ve ekran koordinatı kare başına
  **bir kez** hesaplanır ve bütün bantlarca salt-okunur paylaşılır. Aritmetik
  birebir aynıdır (`make_clip_vertex` + `to_screen`), yalnız tekrarı kaldırılır.
- **Yakın-düzlem hızlı yolu:** Üç köşesi de yakın düzlemin önünde olan üçgenlerde
  kırpma özdeştir; ekran koordinatları doğrudan hazırlıktan alınır, `clip_near`
  ve `to_screen` atlanır. Düzlemi kesen üçgenler tam kırpma yoluna düşer.
- **Bant başına üçgen ayıklama:** Bandın satır aralığına değmeyen (ekran uzayı
  sınır kutusu bant dışında kalan) üçgenler o bantta atlanır. Bu güvenli bir
  üst-kümedir (kırpma yalnız üçgen içindeki alanı kaldırır), bu yüzden hiçbir
  piksel düşmez. İstatistik sahibi bant (bant 0) hiç ayıklamaz.
- **Bant dengeleme:** Bant sayısı iş parçacığı sayısının 8 katına çıkarıldı;
  satır maliyeti dengesiz olduğundan (yakın satırlar çok daha fazla piksel
  gölgeler) daha ince iş öğeleri zamanlayıcının yükü dengelemesini sağlar.
  Köşe hazırlığı paylaşıldığı için ek bantlar ucuzdur.

### 7. Ürün görsel kalite kontrol listesi ve referans kabul protokolü (bu PR)

`docs/12-TEST-KABUL-CI.md` "Görsel kanıt standardı" ve `docs/08` "Golden scene
seti", golden hash'in estetik kaliteyi tek başına kanıtlamadığını ve **sahne
bazlı görsel kalite kontrol listesi ile referans kabul protokolü** gerektiğini
söyler. Bu dilim o boşluğu kapatır:

- **Yeni modül:** `src/render/ws_quality.{h,c}` (ISO C17) her karenin gerçek
  piksel tamponundan ve onu üreten sahneden **deterministik** kalite ölçüleri
  hesaplar: geometri (`objects`, `geometry_coverage`, `coverage`), ışık/ton
  (`mean_luma`, `luma_stddev`, `dark_fraction`, `blown_fraction`), materyal/renk
  (`mean_saturation`, `colour_variety`) ve atmosfer (`sky_gradient`). Altı ürün
  referans sahnesi (`docs/00` §5) katalog kimliklerine eşlenir ve her biri için
  ölçülebilir eşikler tanımlanır.
- **Yeni araç:** `tools/ws_quality` altı sahneyi gerçek katalogdan kurar, render
  eder, ölçer, determinizmi doğrular (aynı seed → bit-özdeş kare) ve
  `quality.csv` + konsol özeti yazar; herhangi bir sahne geçemezse çıkış kodu
  sıfırdan farklı olur (`WS_QUALITY_FAIL`).
- **Yeni belge:** `docs/QUALITY-CHECKLIST.md` kontrol listesini, eşik gerekçesini
  ve referans kabul protokolünü tanımlar.
- **CI kapısı:** `ci.yml` "Product visual-quality checklist" adımı
  `WS_QUALITY_OK`, altı `verdict=PASS` ve `reproducible=1` arar.
- **Birim testleri:** `tests/test_render.c` dört yeni vaka ekler (düz kare
  ölçüsü, altı sahne kimliği, her eşiğin gerçekten kapı işlevi görmesi, ürün
  sahnelerinin gerçek katalogdan kontrol listesini geçmesi). Test sayısı 32 →
  36'ya çıkar.

### 8. Canlı ürün sahnesi kare-zamanı yüzdelik kapısı (bu PR)

P12 kabul kapılarından biri, performansın **tam ürün sahnesinde** ölçülmesidir
(yalnız test üçgeni çekirdeği değil) ve ölçüt tek bir FPS örneklemesi değil,
kare-zamanı yüzdelikleridir (docs/11: p95 hedefi). `worldseed --live-headless`
kanıt koşusu artık her karenin kare zamanını toplayıp `p50_ms`, `p95_ms` ve
`p99_ms` değerlerini raporlar; bunlar canlı oturumun gerçek render + HUD yolunu
kapsar (kamera, dünya, ekoloji ve HUD dâhil). Yüzdelik yardımcısı
(`ws_live_percentile_ms`) bağımsız birim testiyle doğrulanır (`tests/test_live.c`
`live_frame_percentiles`: sıralama, uç değerler, monotonluk, boş küme). `ci.yml`
ve `gui-windows.yml` artık headless çıktısında `p95_ms=` alanını zorunlu kapı
yapar; böylece performans iddiası otomatik ve makine-okunur kanıta bağlanır.

### 9. Ürün hareket kalite kontrol listesi (bu PR)

P12 kabulü altı kalite boyutu ister: geometri/malzeme/ışık/gölge/atmosfer/
**hareket** (`docs/13` P12; `docs/QUALITY-CHECKLIST.md`). Önceki dilim görsel
boyutları (`ws_quality`) kapıladı, ancak **hareket** boyutu yalnız dolaylı
kapsanıyordu (canlı turun kare-arası adımları ve biyomekanik testleri); hareket
tek bir karede ölçülemez. Bu dilim o boşluğu doğrudan, otomatik bir kapıyla
kapatır:

- **Yeni modül:** `src/render/ws_motion.{h,c}` (ISO C17) gerçek referans dört
  ayaklı canlıyı (kızıl geyik, P5 biyomekanik; `src/life/ws_creature.c`,
  `src/life/ws_quadruped.c`) sabit adımlı yürüyüş denetleyicisiyle düz zeminde
  20 s (≈10 000 adım) koşar ve koşunun **kendisinden** deterministik hareket
  ölçüleri çıkarır: ilerleme (`forward_m`, `forward_speed_mps`), pürüzsüzlük
  (`max_com_accel_mps2`), denge (`max_torso_tilt_rad`), ayak teması
  (`min_foot_clearance_m`), enerji (`max_energy_j`) ve süre (`frames`). Başlangıç
  fazı tohumdan türetilir, böylece her tohum aynı eşikleri yürüyüşün farklı
  noktasından sınar.
- **Yeni araç:** `tools/ws_motion` referans yürüyüşü koşar, determinizmi
  doğrular (aynı tohum → bit-özdeş metrik) ve `motion.csv` + konsol özeti yazar;
  eşik veya determinizm geçilmezse çıkış kodu sıfırdan farklı olur
  (`WS_MOTION_FAIL`).
- **Yeni belge bölümü:** `docs/QUALITY-CHECKLIST.md` hareket ölçülerini, eşik
  gerekçesini ve referans kabul protokolünü tanımlar.
- **CI kapısı:** `ci.yml` "Product motion-quality checklist" adımı
  `WS_MOTION_OK`, `verdict=PASS` ve `reproducible=1` arar.
- **Windows paketi:** `tools/package-windows.ps1` aracı çalıştırıp
  `evidence/motion/` çıktısını pakete koyar (`quality/` klasörü altında).
- **Birim testleri:** `tests/test_render.c` üç yeni vaka ekler (her eşiğin
  gerçekten kapı işlevi görmesi, aynı tohumun bit-özdeş metrik üretmesi/farklı
  tohumun farklı koşu üretmesi, referans yürüyüşün kontrol listesini geçip
  gerçekten ileri gitmesi). Test sayısı 36 → 39'a çıkar.

### 10. Daha geniş yarı-gölge (penumbra) — yumuşak gölge (bu PR)

Önceki dilimin bilinen sınırlaması "gölge yumuşatması 3×3 PCF ile sınırlıdır;
daha geniş yarı-gölge (penumbra) sonraki dilimlerdedir" idi. Bu dilim o sınırı
kaldırır:

- **Yeni ayar:** `ws_render_settings.shadow_softness` (gölge eşlemi texel'i
  cinsinden yarı-gölge yarıçapı). Varsayılan **0**'dır ve 3×3 PCF sabit yolu
  korur; bu yüzden gölge kullanmayan bütün sahneler ve golden kareleri
  **bit-özdeş** kalır (17 golden sahnenin tamamı doğrulandı).
- **Yumuşak filtre:** `shadow_softness > 0` iken `shadow_factor` sabit bir
  **tam sayı disk** örnekleme ızgarası (yarıçap = softness texel, en fazla 8)
  kullanır. Örnek konumları tam sayı olduğu için sonuç render bandından ve
  iş parçacığından **bağımsızdır**; filtre hâlâ saklanan derinliklerin saf bir
  fonksiyonudur (deterministik, bantlar arası bit-özdeş).
- **Ürün referans sahnesi:** `shadows` sahnesi artık `shadow_softness = 3.0`
  seçer; ürün gölge referansı sert PCF kenarı yerine yumuşak temas gölgesi
  gösterir. `tests/golden/shadows.ppm` ve `manifest.csv` satırı güncellendi.
- **Birim testi:** `tests/test_render.c` yeni vaka `render shadow soft penumbra`
  ekler: aynı sahnede sert (0) ve yumuşak (3) gölgenin görünür biçimde
  farklılaştığını (penumbra gerçek), yumuşak render'ın bant-bağımsız
  **deterministik** (iki koşu bit-özdeş) ve hiçbir pikseli tam tersine
  çevirmediğini doğrular. Test sayısı 39 → 40'a çıkar.
- **Gölge yalnız doğrudan ışığı azaltır** ilkesi korunur; yumuşak filtre
  ambient terime dokunmaz.

### 11. Gölge geçişi paralelleştirme ve ertelenmiş gökyüzü (bu PR)

Profil, gölgeli sahnelerde gölge eşleminin kare süresinin büyük kısmını
tuttuğunu gösterdi: `shadows` sahnesi 1280×720'de gölge geçişi ≈25 ms
(karenin ≈%75'i), çünkü geçiş **tek iş parçacığında** koşuyordu. İki bağımsız,
bit-özdeş optimizasyon uygulandı:

- **Gölge haritası paralel kurulumu:** Derinlik haritası, görüntü geçişindeki
  gibi ayrık **satır bantlarına** bölünür ve her bant sahneyi yalnız kendi
  satırlarına rasterler (`shadow_build`). Her piksel tam olarak bir banda ait
  olduğundan ve üçgen yürüyüşü her bantta aynı olduğundan sonuç, bant sayısından
  ve iş parçacığı çizelgesinden bağımsız olarak seri kurulumla **bit-özdeş**tir.
  Kare başına tek bir işçi havuzu artık gölge ve görüntü geçişleri arasında
  paylaşılır (önceden iki kez kurulup yıkılıyordu).
- **Ertelenmiş gökyüzü (depth-masked sky):** Gökyüzü artık geometriden **önce**
  bütün kareye çizilmez; derinlik tamponu varsa geometriden **sonra**, yalnız
  derinlik göndericisinde kalan (hiç geometri görmemiş) piksellerde çizilir
  (`draw_sky_band` derinlik maskesi). Örtülen pikseller artık iki kez
  gölgelenmez. Boş bir piksele yazılan değer değişmediği için görüntü
  **bit-özdeş**tir; bu saf bir optimizasyondur.

**Sonuç (aynı makine, 4 çekirdek, 1920×1080, öncesi/sonrası):**

- `terrain_day`: 154.9 ms → **136.5 ms** (~%12).
- `ecosystem`: 100.0 ms → **80.1 ms** (~%20).
- `sky`: 39.1 ms → **34.1 ms** (~%13).
- `world_stream`: 79.4 ms → **73.4 ms** (~%8).
- `shadows`: 71.2 ms → **70.6 ms** (gölge eşlemi kurulumu küçüktür; asıl
  gölge maliyeti zaten paralel örneklemedir).
- **Canlı ürün sahnesi** (`--live-headless 8`): 1920×1080 `avg_ms` 117.6 → **103.4**
  (`p95` 133.2 → **119.8**); 1280×720 `avg_ms` 55.2 → **48.5** (`p95` 65.8 → **56.9**).
- **Bit-özdeşlik:** 17 golden sahnenin tamamı öncesi/sonrası birebir aynı
  (`cmp`); canlı ürün `world_digest` değeri iki build'de de aynı
  (`8322137f…`), yani simülasyon durumu değişmedi. (`frame_sha256` duvar-saati
  bağımlı yakalama zamanı içerdiğinden koşular arasında değişir; bu önceki
  build'de de böyledir ve determinizm kapısı `world_digest`'tir.)
- **Yeni birim testleri:** `render shadow parallel build identical` (gölge
  haritası 1 bant ile 2…17 bant arasında bit-özdeş) ve
  `render sky deferred matches sky first` (ertelenmiş gökyüzünde örtülen
  piksel gökyüzü-kapalı render'la, boş piksel saf gökyüzü render'ıyla birebir
  aynı). Test sayısı 41 → 43.

### 12. Görünürlük (frustum) eleme — bit-özdeş (bu PR)

Kamera frustumunun tamamen dışında kalan nesneler artık çizilmez. Her nesnenin
dünya-uzayı eksen-hizalı kutusu (AABB) beş düzleme (sol/sağ/alt/üst/yakın) karşı
sınanır; kutu bir düzlemin tamamen gerisindeyse nesne atlanır. Bu **bit-özdeş**
bir hızlandırmadır: böyle bir kutunun içerdiği her üçgen zaten kırpılacağı için
atlamak çıktıyı değiştirmez. Rasterleyici **uzak düzlemde kırpma yapmadığından**
uzak düzlem eleme için kullanılmaz (aksi hâlde uzaktaki geometri yanlışlıkla
silinirdi). Gölge geçişi ışık frustum'u farklı olduğundan tüm nesneleri korur.

- **Kapsam:** `src/render/ws_render.c` (frustum düzlem çıkarımı, AABB sınama,
  nesne görünürlük dizisi), `src/render/ws_render.h` (`objects_culled` sayacı).
- **Fırsat (ölçüldü):** `terrain_day` üçgenlerinin ~%43'ü, `ecosystem`'in ~%97'si
  frustum dışında; `world_stream`/`shadows`/`sky` sahnelerinde eleme yok.
- **Kazanç:** Sahneler görünür-piksel gölgelemesine bağlı olduğundan eleme
  kare-zamanını yalnız ~%1–2 düşürür (`terrain_day` 136.9 → 136.7 ms,
  `ecosystem` 80.4 → 79.2 ms, 1080p). Eleme, ekran-dışı rasteri kaldırır; kalan
  maliyet görünür piksellerin gölgelenmesidir. Yine de sıfır riskli ve P12
  kapsamındadır (LOD/eleme).
- **Köşe ön-geçişi atlaması:** Elenen nesnenin ön-geçiş (vertex prepass) köşeleri
  hiçbir zaman okunmadığından, elenen nesneler için köşe hazırlığı da atlanır
  (imleç yine ilerletilir, böylece her nesnenin taban uzaklığı benzersiz kalır).
  Bu, eleme kazancının üzerine ölçülen ek kazanç: `terrain_day` 136.4 → 134.4 ms,
  `ecosystem` 80.0 → 77.3 ms (1080p, üç koşu ortalaması); bit-özdeş.
- **Bit-özdeşlik:** 17 golden sahnenin tamamı 1920×1080'de birebir aynı (`cmp`);
  `terrain_day` ve `ecosystem` ayrıca **3840×2160**'ta da bit-özdeş (küçük kare
  nadir farkları gizleyebildiği için ürün çözünürlüğünde doğrulandı).
- **Yeni birim testi:** `render frustum cull is bit identical` — frustum dışı bir
  küpün elendiğini (`objects_culled == 1`) ve sahnenin o küp olmadan çizilen
  sürümle bit-özdeş kaldığını doğrular.
- **Kalan engel (aşırı çizim):** Güncel ölçüm (1920×1080) `terrain_day` aşırı
  çizimi **1.31x** (ertelenmiş gökyüzü bunu önceki ~1.9x'ten düşürdü);
  `ecosystem` 0.80x, `world_stream` 0.56x, `sky` 0.22x, `shadows` 0.37x. Derinlik
  testi zaten gölgelemeden önce (erken-z) yapılır; bu yüzden kazanç yalnız çizim
  sırasını değiştirmekle gelir ve bu, z **beraberliğinde** hangi üçgenin pikseli
  kazandığını değiştirdiğinden bit-özdeş değildir (`terrain_day`'de ölçüldü:
  4188 bayt farklı). Bu optimizasyon beraberlik kırıcı deterministik bir anahtar
  bulunana kadar ertelenir.

## Ölçüm

- 1920×1080 `terrain_day` (bu PR öncesi/sonrası, aynı makine, 4 çekirdek):
  ~169–179 ms → **~155–160 ms** (~5.6–5.9 → ~6.2–6.4 FPS).
- 3840×2160 `terrain_day`: ~651–703 ms → **~603–608 ms** (~1.5 → ~1.6 FPS).
- Golden sahne örnekleri: `terrain_day` 24.6 → 15.8 ms; `shadows` 12.1 → 7.5 ms.
- **Bit-özdeşlik:** Golden 17 sahnenin tamamında `triangles_submitted`,
  `triangles_drawn`, `pixels_shaded`, `coverage`, `mean_luma`, `max_luma` ve
  `sha256` değerleri önce/sonra birebir aynı; yalnız `render_ms` değişir.
- **Kalan büyük maliyet:** Piksel başına `exp` (sis) ve `pow` (speküler)
  çağrıları; sis kapatıldığında 1080p ~156 → ~141 ms, sis+speküler kapatıldığında
  ~101 ms. Bu dilimde gökyüzü ertelenerek örtülen piksellerin iki kez
  gölgelenmesi kaldırıldı ve gölge kurulumu paralelleştirildi; kalan `exp`/`pow`
  sıcak noktaları sonraki dilimde (yaklaşım tabloları / GPU) hedeflenir.
- `raster_cpu` 1080p / 2048 üçgen: ~35.6 ms.
- **Ürün görsel kalite kontrol listesi** (`ws_quality`, 640×360, seed 20260101,
  bu makine): altı referans sahnenin tamamı geçer; örnek metrikler:
  `terrain_day` geom_cov=1.00 stddev=18.0 sat=0.25 variety=37 sky_grad=0.84;
  `terrain_dusk` stddev=35.0; `organism_forage` sky_grad=15.4;
  `ecosystem` objects=718. Aynı seed iki koşuda bit-özdeş (`reproducible=1`).
- **Canlı ürün sahnesi kare zamanı** (`--live-headless 8`, bu makine, 4 çekirdek,
  CPU referans yolu; render + HUD, kamera/dünya/ekoloji dâhil; bu dilim
  sonrası):
  - 1280×720: `avg_ms≈48.5`, `p50≈48.2`, `p95≈56.9`, `p99≈65.0` (~21 FPS).
  - 1920×1080: `avg_ms≈103.4`, `p50≈102.9`, `p95≈119.8`, `p99≈138.0` (~10 FPS).
  - Önceki dilimde aynı komutla 1280×720 `avg_ms≈57.8` / 1080p `avg_ms≈123.2`
    idi; ertelenmiş gökyüzü ve paralel gölge kurulumu bunları düşürdü.
  - Bu, test üçgeni çekirdeği değil gerçek ürün sahnesidir; 1080p p95 hedefi
    (33,3 ms) CPU yolunda henüz tutulmuyor.
- Zamanlayıcı: seri ~174 ms → paralel ~58 ms (`speedup≈3.0x`, `identical=1`).
- `shadows` sahnesi 1920×1080: ~130 ms (gölge geçişi dâhil).
- `shadows` sahnesi yumuşak gölge maliyeti (bu makine, 4 çekirdek, 1920×1080):
  sert 3×3 PCF (`shadow_softness=0`) ~37 ms; yumuşak yarıçap 3 ~72 ms;
  yarıçap 5 ~147 ms. Örnek sayısı yarıçapla kare olarak arttığı için maliyet de
  öyle artar; bu yüzden `shadows` ürün referans sahnesi ölçülü bir yarıçap
  (3.0) seçer.
- Döşemeli OpenCL raster (pocl CPU aygıtı, bu ortamda GPU yok):
  - 640×480 / 3000 üçgen: referans 1393 ms → döşemeli 12.6 ms (**110x**),
    `bit_identical=1`.
  - 1280×720 / 20 000 üçgen: referans 65 372 ms → döşemeli 103 ms (**635x**),
    `bit_identical=1`.
  - `ws_perf` 1920×1080 / 2048 üçgen: `raster_gpu_tiled` 22.9 ms,
    referansa göre 269x, `identical=1`, karar `use_tiled`.

## Bilinen sınırlamalar

- Hedef 30 FPS henüz 1080p'de tutulmuyor (~6 FPS). Gölge geçişi ek bir
  geometri geçişi maliyeti getirir; sahne başına opt-in'dir.
- Gölge haritası **her karede** sahnenin güncel nesne listesinden yeniden
  kurulur, bu yüzden hareketli nesneler (canlı üründeki aktörler) gölgelerini
  doğru izler; bu davranış `render shadow follows moving occluder` testiyle
  kilitlenir. Kurulum artık satır bantlarına bölünüp işçi havuzunda koşar
  (`shadow_build`) ve görüntü geçişiyle **aynı** havuzu paylaşır; sonuç seri
  kurulumla bit-özdeştir (`render shadow parallel build identical` testi).
  Maliyeti kare başına ek bir geometri geçişidir; hareketli sahnelerde gölge
  haritasını yalnız değişen nesneler için güncellemek (artımlı yeniden kurma)
  sonraki dilimdir.
- Yarı-gölge (penumbra) genişletildi (`shadow_softness`, sabit tam sayı disk);
  filtre gerçek bir yumuşak temas gölgesi verir ve deterministiktir, ancak örnek
  sayısı yarıçapla kare olarak artar (yarıçap 8'de ≈197 örnek), bu yüzden gölge
  kullanan sahnelerde piksel maliyeti yükselir. Örnek konumları deterministik
  olduğu için mavi-gürültü/dönüşlü örnekleme ile daha az örnekle eşdeğer kalite
  sonraki dilimdir. (Gölge haritasını kare başına güncelleme ve bu kurulumu
  paralelleştirme bu dilimde yapıldı.)
- Döşemeli GPU yolu şu an bir **doğrulanmış ve kanıtlanmış raster ilkeli**dir;
  canlı gölgeli sahne hâlâ CPU yolundan çizilir. Döşemeli çekirdeği canlı
  gölgeli render'a bağlamak (renk + döşeme başına binlerce üçgeni GPU'ya
  aktarma) sonraki dilimdir. Bu dilimde çekirdek ürünün çizim koduna bağlı
  değildir; burada kanıtlanan, darboğazın çözüldüğü ve çıktının bit-özdeş
  kaldığıdır.
- CPU yolu zorunlu referanstır; GPU (OpenCL C 1.2) varyantı sonraki iş.
