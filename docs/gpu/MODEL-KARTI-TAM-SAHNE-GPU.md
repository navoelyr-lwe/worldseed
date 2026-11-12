# Model kartı — OpenCL C 1.2 tam sahne render yolu

1. **Kimlik / sürüm / modül:** `gpu.scene` v1; `kernels/ws_scene.cl` (OpenCL C 1.2),
   host `src/render/ws_gpu_scene.c`, `src/gpu/ws_opencl.{h,c}`. Referans:
   `src/render/ws_render.c` (`ws_render_scene`, `shade_pixel`, `to_screen`,
   `clip_near`). Karşılaştırma aracı: `tools/ws_gpu_scene_check.c`. Giriş noktası:
   `ws_render_scene_gpu(...)`. Cihaz keşfi: `src/platform/ws_opencl_probe.*`.
2. **Temsil edilen süreç ve amaç:** ADR-001 uyarınca GPU yalnız kararı verilmiş
   kareyi hızlandırır; otoritatif simülasyon durumu CPU C17 tarafında kalır. Bu
   kernel, CPU referans renderer'ının **tüm gölgeleme ardışık düzenini** OpenCL C
   1.2'ye taşır ve aynı sahneyi aynı kareye çevirir. Amaç, GPU bulunduğunda ürün
   sahnesini hızlandırmak ve GPU yoksa CPU referansına bit-özdeş düşmektir.
3. **Ölçek:** Girdi/çıktı ekran-uzayı; koordinatlar piksel, `z` NDC [0,1] (küçük
   yakın). Vertex kaydı 13 `float`: `sx,sy,z,invw, wx,wy,wz, nx,ny,nz, r,g,b`.
   Tile-bin girdisi üçgen başına 39 `float` (13·3). Global çalışma boyutu
   `tiles_x·tiles_y·tile_size·tile_size`; work-item başına bir piksel. SI birimi
   yoktur; bu bir görüntü işlemidir.
4. **Durum değişkenleri:** Girdi tamponları: `verts` (vertex başına 13 float,
   tile-bin ile strided), `obj_mat` (nesne başına 12 float), `consts` (gölgeleme
   sabitleri), `light_vp` (16 float), `shadow_depth` (opsiyonel gölge haritası),
   `triangle_count`, `object_count`, `const_count`, `width`, `height`, `tile_size`,
   `shadow_size`. Çıktı: `pixels` (RGBA8) ve `depth` (float). Kernel durumu yoktur.
5. **Algoritma (deterministik):** Her work-item bir piksel işler:
   - Tile-bin, pikselin döşemesindeki üçgen listesini verir; yalnız kapsayan
     üçgenler döngülenir (döşemeli çekirdekle aynı ikili ayıklama).
   - CPU ile aynı edge fonksiyonu ve iç testi (`area > 0` → tüm `w >= 0`, değilse
     `w <= 0`); perspektif-düzeltmeli `iw = Σ l·invw`, `p = l·invw / iw`.
   - `z` enterpolasyonu; "küçük derinlik kazanır, eşit derinlik yazmaz" kuralı.
   - Öznitelikler (`world`, `normal`, albedo) enterpolasyonu; `shade_pixel` ile
     aynı Lambert + hemisferik ortam + speküler + yönlü gölge (PCF) + sis + pozlama
     + ton eşleme; gökyüzü degradesi; son olarak `encode_display_u8` (sRGB)
     kuantalaması.
   - Piksel başına tek work-item olduğundan atomik/yarış yoktur; çıktı, çalıştırma
     sırasından bağımsız ve deterministiktir.
6. **Varsayımlar / ihmal edilen süreçler:** Yakın-düzlem kırpma ve fan üçgenleme
   host tarafındadır (`ws_gpu_scene.c`), CPU'daki `clip_near` ile aynı kuralı
   kullanır. Kernel yalnız görünür görüntüyü üretir; simülasyon durumu, fizik,
   biyomekanik ve davranış CPU'da kalır. Gölge haritası CPU ile aynı koddan
   (`ws_render_shadow_build_for_test`) kurulur.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** Gölgeleme sabitleri sahne
   ayarlarından (pozlama, ton eşleme, sis yoğunluğu, gölge gücü/yarıçapı, ışık
   rengi) host tarafında hesaplanır. Sayısal tip olarak `float` kullanılır; referans
   CPU `double`'dır. Bu bilinçli bir hız/duyarlılık tercihidir; belirsizlik
   düşük-orta (kayan nokta yuvarlaması ve eş-düzlemli derinlik beraberliği).
8. **Sayısal yöntem, kararlılık:** CPU ile aynı edge/enterpolasyon/gölgeleme
   ifadeleri; `float` birikim. Renk uzayı sözleşmesi: gölgeleme ara değerleri
   **linear** uzayda tutulur ve CPU'daki `encode_display_u8` (sRGB) ile aynı
   kuantalama adımı uygulanır. Eş-düzlemli yüzeylerde (bitişik arazi/döşeme
   yüzeyleri) derinlik sırası float ile double arasında son-bit düzeyinde
   değişebilir; bu belgelenmiş semantik sınırdır ve aracın derinlik-beraberliği
   ölçütüyle ayrıştırılır.
9. **Doğrulama planı:** `tests/test_gpu.c::scene_gpu_semantics`: küçük bir sahne
   CPU ve GPU yollarında çizilir; GPU varsa kanal farkı toleransla karşılaştırılır,
   yoksa CPU fallback'in bit-özdeş olduğu doğrulanır. `tools/ws_gpu_scene_check.c`:
   17 katalog sahnesinin tamamı çizilir; her farklı piksel derinlik beraberliği
   (yapısal olmayan) veya yapısal hata olarak sınıflandırılır. `tools/ws_perf.c`:
   tam sahne GPU kare süresi p50/p95/p99 ve CPU'ya göre semantik fark oranı. CI:
   `ci.yml` `opencl-cpu` işi pocl CPU ICD ile gerçek kernel derleme/çalıştırma
   kanıtı üretir; `gui-windows.yml` izole Windows runner'da gerçek GPU'da koşar.
10. **Kabul eşikleri ve sınırlar:** `ws_gpu_scene_check` yapısal fark oranı
    `<= 0.0005` ve derinlik-beraberliği renk farkı `<= 90/255` ister. Ölçülen
    (pocl CPU ICD): `gpu_used=17`, tüm sahnelerde `structural_fraction=0.00000`;
    `world_stream`'de `tie_fraction=0.06585`, `max_tie_channel_delta=81` (tüm
    farklar açıklanmış beraberlik). `orbit` sahnesinde uygulama GPU karesi CPU
    karesiyle bit-özdeş (`differing=0`). Bilinen sınır: `float` vs `double`
    nedeniyle eş-düzlemli sahnelerde tam bit-özdeşlik garanti edilmez; pocl CPU
    ICD gerçek GPU'dan yavaştır, hızlanma iddiası yalnız gerçek cihazda geçerlidir.
    Kanıt düzeyi **D2** (determinizm, tolerans ve hata yolu testleri geçti; gerçek
    GPU performans kıyası referans donanımda yapılır, bkz. `gui-windows.yml`).
11. **Değişiklik kaydı:** Edge testi, derinlik kuralı, gölgeleme ardışık düzeni,
    `float`/`double` tercihi veya tolerans değişirse bu kart, `kernels/ws_scene.cl`,
    `src/render/ws_gpu_scene.c`, `src/gpu/ws_opencl.*`, ilgili testler,
    `docs/P10-GPU-VE-PERFORMANS.md` ve `docs/P12-RENDER-PERFORMANS-VE-KALITE.md`
    birlikte güncellenir.
