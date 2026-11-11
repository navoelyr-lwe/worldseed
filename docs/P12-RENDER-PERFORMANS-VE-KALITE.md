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
  en yakın derinliği yazar. Gölge geçişi paralel görüntü geçişinden **önce**,
  tek seferde koşar; bütün bantlar aynı veriyi örnekler.
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

## Ölçüm

- 1920×1080 `terrain_day`: ~150–194 ms (~5.5–6.7 FPS).
- 3840×2160 `terrain_day`: ~651–703 ms (~1.5 FPS).
- `raster_cpu` 1080p / 2048 üçgen: ~35.6 ms.
- Zamanlayıcı: seri ~174 ms → paralel ~58 ms (`speedup≈3.0x`, `identical=1`).
- `shadows` sahnesi 1920×1080: ~130 ms (gölge geçişi dâhil).
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
- Gölge haritası sahne başına bir kez kurulur (statik sahne varsayımı); hareketli
  nesneler için gelecekte kare başına güncelleme gerekir.
- Gölge yumuşatması 3×3 PCF ile sınırlıdır; daha geniş yarı-gölge (penumbra)
  sonraki dilimlerdedir.
- Döşemeli GPU yolu şu an bir **doğrulanmış ve kanıtlanmış raster ilkeli**dir;
  canlı gölgeli sahne hâlâ CPU yolundan çizilir. Döşemeli çekirdeği canlı
  gölgeli render'a bağlamak (renk + döşeme başına binlerce üçgeni GPU'ya
  aktarma) sonraki dilimdir. Bu dilimde çekirdek ürünün çizim koduna bağlı
  değildir; burada kanıtlanan, darboğazın çözüldüğü ve çıktının bit-özdeş
  kaldığıdır.
- CPU yolu zorunlu referanstır; GPU (OpenCL C 1.2) varyantı sonraki iş.
