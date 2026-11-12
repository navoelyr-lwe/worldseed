# P10 — GPU ve performans ölçekleme

Bu belge P10 aşamasının durumunu, uygulanan kodu, otomatik kanıtları ve bilinen
sınırlamaları kaydeder. Kabul ölçütleri `docs/13-YOL-HARITASI.md` (P10) ve
`docs/11-PERFORMANS-DONANIM.md` içindedir; GPU dili/API kararı `docs/15-RISKLER-KARARLAR.md`
(ADR-001) ile sabittir.

## Kapsam

- Donanım keşfi (CPU çekirdek, RAM, OS, OpenCL cihaz özeti).
- OpenCL 1.2 API + OpenCL C 1.2 kernel ile gerçek kernel derleme ve çalıştırma.
- Aynı sahnede CPU/GPU semantik karşılaştırması ve hata toleransları.
- GPU device-loss (aygıt kaybı) kurtarma ve CPU fallback.
- Çok çekirdekli iş zamanlayıcı (work scheduler).
- 4K performans ölçümü ve p50/p95/p99 frame time.

## Ne yapıldı

### Donanım keşfi — `src/platform/ws_hardware.{h,c}`

`ws_hardware_probe` CPU iş parçacığı sayısını, fiziksel RAM'i, işletim sistemini ve
OpenCL cihaz özetini doldurur. Eksik alan hata değildir; sıfır/boş bırakılır.
`ws_hardware_describe` log için tek satır profil üretir. Hiçbir sayı ilan edilmiş
minimum sistem gereksinimi değildir (bkz. `docs/11`); yalnız uyarlanabilir kalite ve
benchmark etiketlemesi içindir.

### Thread katmanı — `src/platform/ws_threads.{h,c}`

Motorun tek platform-özel thread kodu. Windows'ta Win32 (CreateThread /
CRITICAL_SECTION / CONDITION_VARIABLE), diğer platformlarda POSIX threads kullanır.
Çekirdek bu katman dışında hiçbir OS thread başlığı içermez. Thread'ler simülasyon
durumu taşımaz: otoritatif dünya tek thread'de ilerler.

### İş zamanlayıcı — `src/core/ws_sched.{h,c}`

Sabit bir worker havuzu üzerinde "parallel for" çalıştırır. Determinizm kuralı
(`docs/11` "Çok çekirdek ve GPU"): her indeks bağımsız, kendi yuvasına yazan bir
iştir; gövde indeksin saf fonksiyonu olmalıdır. Havuz yalnız duvar-saati süresini
değiştirir, üretilen veriyi değiştirmez. Çağıran thread her işin sonunda bir payı
kendisi koşar ve tüm worker'lar bitene kadar bekler. Worker sayısı donanım
thread sayısına göre sınırlanır; `0` worker deterministik seri fallback'tir.

### OpenCL 1.2 host ve kernel — `src/gpu/ws_opencl.{h,c}`, `kernels/ws_raster.cl`

- Sistem OpenCL ICD'si (`OpenCL.dll` / `libOpenCL.so.1`) çalışma zamanında yüklenir;
  ürün hiçbir OpenCL runtime'ı dağıtmaz (ADR-001).
- Cihaz seçimi: önce GPU, yoksa CPU OpenCL cihazı; cihaz yokluğu hata değildir.
- `kernels/ws_raster.cl` OpenCL C 1.2 çekirdek diliyle yazılmış düz gölgeli üçgen
  rasterleyicisidir. Yalnız OpenCL C 1.2 çekirdek özelliklerini kullanır (2.x/3.x
  builtin, sub-group, image/sampler, atomik yok). Kernel kaynağı derleme zamanında
  `cmake/embed_kernel.cmake` ile C string olarak gömülür ve `-cl-std=CL1.2` ile derlenir.
- Semantik olarak `src/render/ws_raster.c` (`ws_raster_triangle`) referansıyla
  eşleşecek şekilde tanımlıdır: piksel başına bir work-item (atomik/yarış yok,
  deterministik), aynı edge-fonksiyonu iç testi, aynı barycentric renk/derinlik
  enterpolasyonu, aynı "küçük derinlik kazanır, eşit derinlik yazmaz" kuralı.
- Her API çağrısı kontrol edilir. Cihaz kaybı bayrağı `ws_opencl_device_lost` ile
  bildirilir; çağıran CPU'ya düşer.

### Tam sahne GPU yolu — `kernels/ws_scene.cl`, `src/render/ws_gpu_scene.c`

- `kernels/ws_scene.cl` (OpenCL C 1.2), CPU referans renderer'ının **tüm gölgeleme
  ardışık düzenini** yansıtır: perspektif-düzeltmeli enterpolasyon, Lambert doğrudan
  ışık, hemisferik ortam, speküler, yönlü gölge haritası (PCF), mesafe sisi, pozlama
  ve ton eşleme, gökyüzü degrade. Vertex kaydı 13 float (`sx,sy,z,invw, wx,wy,wz,
  nx,ny,nz, r,g,b`); tile-bin girdisi üçgen başına 39 float'tır.
- `src/render/ws_gpu_scene.c` host tarafı: sahnenin nesne/vertex/üçgen verisini
  arabelleklere yazar, yakın-düzlem kırpma ve fan üçgenlemeyi yapar, gölge haritasını
  CPU ile aynı koddan (`ws_render_shadow_build_for_test`) kurar ve çekirdeği
  gönderir. `ws_render_scene_gpu(...)` giriş noktasıdır; cihaz yoksa CPU referansına
  düşer ve `stats->path = WS_RENDER_PATH_CPU` bildirir.
- Renk uzayı sözleşmesi: çekirdek, gölgeleme ara değerlerini **linear** uzayda tutar
  ve CPU'daki `encode_display_u8` (sRGB) ile aynı kuantalama adımını uygular; bu
  yüzden float ile double arasındaki fark yalnız son 8-bit yuvarlama basamağına
  düşer.

GPU, otoritatif simülasyonu etkilemez (ADR-001): yalnız kararı verilmiş kareyi
hızlandırır.

## Kanıtlar (otomatik)

- **Semantik karşılaştırma** — `tools/ws_gpu_check`: CPU referansı ve OpenCL C 1.2
  kernel aynı deterministik üçgen yığını üzerinde koşar; piksel farkı oranı ve
  kanal farkı toleransla karşılaştırılır. Gerçek koşu (pocl CPU ICD, 256×192,
  96 üçgen): `diff_pixels=1 / 49152` (`diff_fraction=0.000020`),
  `max_channel_delta=1`, `max_depth_delta=1.192e-07` — float (kernel) ile double
  (CPU) arasındaki yuvarlama farkı; `semantic_match=ok`.
- **Device-loss kurtarma** — aynı araç simüle edilmiş cihaz kaybından sonra
  backend'in kullanılamaz olduğunu, GPU çağrısının temiz hata verdiğini ve CPU
  görüntüsünün bit-özdeş kaldığını doğrular: `device_loss_recovered=1`.
- **GPU yokluğu** — cihaz yoksa araç CPU referansını yine doğrular ve
  `gpu=unavailable` yazar; CI GPU'suz runner'da yanlış hata üretmez.
- **Tam sahne GPU/CPU karşılaştırması** — `tools/ws_gpu_scene_check`: 17 katalog
  sahnesinin **tamamı** OpenCL tam-sahne yoluyla çizilir ve CPU referansıyla
  karşılaştırılır. Araç, her farklı pikseli bir **derinlik beraberliği** (iki yol da
  aynı derinlikteki iki yüzeyden birini seçer; float/double farkı) ile **yapısal
  hata** (derinlik de uyuşmuyor) olarak ayırır. Ölçülen sonuç (pocl CPU ICD):
  `gpu_used=17`, tüm sahnelerde `structural_fraction=0.00000`; en yüksek renk farkı
  `world_stream`'de 81/255'tir ve o sahnenin **tüm** farklı pikselleri açıklanmış
  derinlik beraberliğidir (6,6%). Eş-düzlemli yüzeylerin (bitişik arazi/akış
  döşemeleri) derinlik sırası float ile double arasında son-bit düzeyinde
  değişebilir; bu bir hata değil, belgelenmiş semantik sınırdır.
- **Ürün uygulaması GPU karesi** — `worldseed --capture ... --gpu` opt-in bayrağı
  aynı sahneyi OpenCL tam-sahne yoluyla çizer; `orbit` sahnesinde GPU karesi CPU
  karesiyle **bit-özdeş** (`ws_imgdiff differing=0`). Varsayılan yakalama yolu CPU
  referansıdır, bu yüzden golden kapısının uygulama↔golden `cmp` karşılaştırması
  etkilenmez.
- **Zamanlayıcı determinizmi** — `tests/test_gpu.c`: seri ve paralel sonuç
  bit-özdeş (`identical=1`); ikinci gönderim havuzu yeniden kullanır; sıfır-worker
  havuzu seri fallback'tir; boş aralık iş göndermez.
- **Performans** — `tools/ws_perf`: donanım profili, CPU vs GPU raster süresi ve
  hızlanma (GPU yalnız gerçekten hızlıysa `verdict=use_gpu`), zamanlayıcı ölçekleme
  (örnek: 3 worker, `speedup=3.29x`, `identical=1`) ve referans sahne frame
  süreleri p50/p95/p99 ile. `--quick` modu CI için küçültülmüş; tam mod 4K
  (3840×2160) ölçer.

### Örnek tam koşu (bu geliştirme makinesi, pocl CPU ICD)

```
perf raster_cpu 1920x1080 triangles=2048 ms=96.64
perf raster_gpu 1920x1080 device="cpu-skylake-avx512-..." ms=6175.24 speedup_vs_cpu=0.02x diff_fraction=0.000005 verdict=use_cpu
perf scene scene=terrain_day 1920x1080 frames=4 avg_ms=2190.47 p50_ms=2191.61 p95_ms=2194.17 p99_ms=2194.33 avg_fps=0.5 triangles=21878
```

Bu makinede OpenCL cihazı pocl'un CPU ICD'sidir; kendi CPU rasterleyicimizden
yavaştır. Ürün politikası gereği GPU yalnız gerçek kazanç olduğunda kullanılır, bu
yüzden `verdict=use_cpu` doğru davranıştır. Gerçek GPU'da `use_gpu` beklenir; iddia
gerçek cihaz ölçümü olmadan yapılmaz.

## CI

- `ci.yml` içindeki `opencl-cpu` işi pocl CPU ICD'yi kurar ve OpenCL C 1.2 kernel'i
  gerçekten derleyip çalıştırır: `ws_gpu_check` (semantik + device-loss) ve
  `ws_perf --quick` (ölçekleme + frame süreleri) kanıt olarak yüklenir.
- Linux sanitizer matrisi ve Windows MSVC işleri OpenCL ICD kurmaz; bu işlerde
  `gpu` testi cihaz yokluğunda CPU fallback yolunu doğrular (sanitizer ile temiz).
- GPU testi için ASan sızıntı denetimi kapatılır: sızıntılar yalnız ICD
  sürücüsündedir (bizim kodumuzda değil), ASan bellek hatalarını yine yakalar.

## Bilinen sınırlamalar

- Gerçek GPU donanımı bu ortamda yoktur; GPU hızlanma iddiası yalnız OpenCL uyumlu
  CPU ICD ile semantik ve yol kanıtına dayanır. Gerçek GPU ölçümü referans
  donanımda yapılmalıdır.
- Kernel yalnız düz gölgeli üçgen rasterleme yapar (semantik karşılaştırma için
  referansla eşleşen alt küme). Tam sahne GPU rasterleme/pixel-buffer sunumu
  sonraki dilimdedir. **Güncelleme:** tam sahne GPU yolu artık uygulandı ve her
  katalog sahnesinde CPU referansıyla karşılaştırıldı (bkz. yukarıda "Tam sahne
  GPU/CPU karşılaştırması"); eş-düzlemli yüzeylerde son-bit derinlik beraberliği
  nedeniyle tam bit-özdeşlik yerine belgelenmiş bir semantik sınır geçerlidir.
- Çok çekirdekli zamanlayıcı altyapısı hazırdır ve ölçülmüştür; otoritatif
  simülasyon adımının paralelleştirilmesi (bölge/canlı işleri) determinizm kanıtıyla
  birlikte sonraki dilimlere bırakılmıştır.
- 4K/30 FPS ürün hedefi henüz karşılanmamıştır; bu bir CPU referans renderer'ıdır ve
  hedef profiller donanım benchmark'ıyla doğrulanır (`docs/11`).
