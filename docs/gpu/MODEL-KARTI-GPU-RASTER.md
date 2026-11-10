# Model kartı — OpenCL C 1.2 düz gölgeli rasterleme kernel'i

1. **Kimlik / sürüm / modül:** `gpu.raster` v1; `kernels/ws_raster.cl`
   (OpenCL C 1.2), host `src/gpu/ws_opencl.c`, `src/gpu/ws_opencl.h`. Referans:
   `src/render/ws_raster.c` (`ws_raster_triangle`). Karşılaştırma aracı:
   `tools/ws_gpu_check.c`. Cihaz keşfi: `src/platform/ws_opencl_probe.*`.
2. **Temsil edilen süreç ve amaç:** ADR-001 uyarınca GPU yalnız kararı verilmiş
   kareyi hızlandırır; otoritatif simülasyon durumu CPU C17 tarafında kalır. Kernel,
   ekran-uzayı üçgenleri (zaten dönüştürülmüş, kırpılmış) düz (flat) barycentric
   renkle rasterler. Amaç, referans CPU rasterleyicisiyle anlamca eşleşen, taşınabilir
   bir OpenCL 1.2 compute yolu sunmak ve gerçek kazanç olduğunda kullanmaktır.
3. **Ölçek:** Girdi/çıktı ekran-uzayı; koordinatlar piksel, `z` NDC [0,1] (küçük
   yakın). Global çalışma boyutu `width·height`; piksel başına bir work-item. SI
   birimi yoktur; bu bir görüntü işlemidir.
4. **Durum değişkenleri:** Girdi tamponları `vx,vy,vz` (üçgen başına 3 float),
   `color` (üçgen başına 9 uchar), `triangle_count`, `width`, `height`. Çıktı
   `pixels` (RGBA8) ve `depth` (float). Kernel durumu yoktur (saf fonksiyon).
5. **Algoritma (deterministik):** Her work-item bir piksel `(x,y)` işler:
   - Tüm üçgenler döngülenir; edge fonksiyonları `w0,w1,w2` ve `area` CPU ile aynı
     ifadeyle hesaplanır.
   - İç test: `area > 0` ise tüm `w >= 0`, değilse tüm `w <= 0` (CPU ile aynı).
   - Barycentric `l = w / area`; `z = Σ l·vz`.
   - "Küçük derinlik kazanır, eşit derinlik yazmaz": `z >= best_z` ise atla.
   - Renk `Σ l·color`, `[0,255]`'e kırpılır; `alpha = 255`.
   - Piksel başına tek work-item olduğundan atomik/yarış yoktur; çıktı, çalıştırma
     sırasından bağımsız ve deterministiktir.
6. **Varsayımlar / ihmal edilen süreçler:** Kernel yalnız düz gölgeli üçgen yazar;
   perspektif-düzeltmeli öznitelik enterpolasyonu, Lambert/Blinn-Phong gölgeleme,
   gökyüzü, sis ve ton eşleme içermez (bunlar tam sahne renderer'ındadır). Ayrı
   derinlik testi CPU ile aynı sıralı kuralla (tek adım) yapılır. Yakın-düzlem
   kırpma host/CPU tarafındadır.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** Yapılandırılabilir parametre
   yoktur. Sayısal tip olarak `float` kullanılır; referans CPU `double`'dır. Bu
   bilinçli bir hız/duyarlılık tercihidir; tolerans bunu kapsar (bkz. 10). Belirsizlik
   düşük-orta (kayan nokta yuvarlaması).
8. **Sayısal yöntem, kararlılık:** Aynı edge/enterpolasyon ifadeleri; `float`
   birikim. Derinlik eşitliğinde yazmama kuralı iki yolun da aynı kazananı seçmesini
   sağlar. Global boyut `width·height` ile birebir eşlenir (work-item başına bir
   piksel; sınır koşulu `gid >= count`).
9. **Doğrulama planı:** `tests/test_gpu.c`: cihaz varsa CPU ve GPU aynı yığın
   üzerinde koşar; `<= %1` piksel toleransla ve `<= 4` kanal farkıyla karşılaştırılır;
   cihaz yoksa CPU fallback doğrulanır. `tools/ws_gpu_check.c`: daha büyük yığın,
   `diff_fraction`/`max_channel_delta`/`max_depth_delta` raporu, device-loss
   kurtarma. `tools/ws_perf.c`: CPU vs GPU süre ve hızlanma (`verdict`). CI:
   `opencl-cpu` işi pocl CPU ICD ile gerçek kernel derleme/çalıştırma kanıtı üretir.
10. **Kabul eşikleri ve sınırlar:** Semantik karşılaştırmada piksel farkı `<= %1` ve
    kanal farkı `<= 4/255`. Ölçülen örnek: `diff_pixels=1/49152`, `max_channel_delta=1`,
    `max_depth_delta=1.19e-07`. Device-loss'ta backend kullanılamaz olur, GPU çağrısı
    temiz hata verir, CPU görüntüsü bit-özdeş kalır. Bilinen sınır: gerçek GPU
    donanımı bu ortamda yok; hızlanma iddiası yalnız gerçek cihazda geçerlidir.
    Kanıt düzeyi **D2** (determinizm, tolerans ve hata yolu testleri geçti; gerçek
    GPU performans kıyası D3 yok).
11. **Değişiklik kaydı:** Edge testi, derinlik kuralı, `float`/`double` tercihi veya
    tolerans değişirse bu kart, `kernels/ws_raster.cl`, `src/gpu/ws_opencl.*`, ilgili
    testler ve `docs/P10-GPU-VE-PERFORMANS.md` birlikte güncellenir.
