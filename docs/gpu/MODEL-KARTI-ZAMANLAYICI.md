# Model kartı — Çok çekirdekli iş zamanlayıcı

1. **Kimlik / sürüm / modül:** `core.sched` v1; `src/core/ws_sched.c`,
   `src/core/ws_sched.h`. Thread katmanı: `src/platform/ws_threads.*`. Ölçüm aracı:
   `tools/ws_perf.c` (`run_scaling`). Test: `tests/test_gpu.c`.
2. **Temsil edilen süreç ve amaç:** `11-PERFORMANS-DONANIM.md` "Çok çekirdek ve GPU"
   uyarınca bağımsız işlerin (bölge/canlı/parçacık) kontrollü paralelleştirilmesi.
   Amaç: duvar-saati süresini kısaltmak, üretilen veriyi değiştirmemek. Dünya yazımı
   ve olay sırası deterministik sınırlarla korunur; otoritatif simülasyon tek
   thread'de ilerler.
3. **Ölçek:** İş uzayı `[begin, end)` tamsayı indeks aralığı; chunk'lar `parts = worker+1`
   katılımcı arasında dönüşümlü dağıtılır. Zaman ölçeği iş başına (gövdenin maliyeti).
   Thread sayısı donanım thread sayısına sınırlanır.
4. **Durum değişkenleri:** Havuz: worker dizisi, `worker_count`; kilit/koşul değişkenleri;
   `generation` (gönderim sayacı), `last_gen[]` (worker başına son çalıştırdığı nesil);
   aktif iş parametreleri (`begin/end/chunk/parts/body/ctx`); `remaining` (bitmemiş
   katılımcı sayısı); `stop`; `jobs_dispatched`.
5. **Algoritma (deterministik):**
   - `parallel_for`: chunk = `ceil(total / (parts·4))` (≥1); iş parametreleri kilit
     altında yazılır; `generation++`; tüm worker'lar uyandırılır; çağıran thread
     `worker_count` indeksli katılımcı olarak bir payı kendisi koşar; sonra
     `remaining == 0` olana kadar bekler.
   - Katılımcı `p`, `c ≡ p (mod parts)` olan chunk'ları koşar. Her gövde çağrısı bir
     indeks içindir; gövde indeksin saf fonksiyonu olmalı ve yalnız kendi yuvasına
     yazmalıdır.
   - Worker döngüsü: kilitle, nesil değişene kadar bekle; nesli işaretle, kilidi bırak,
     payı koş, `remaining--`; sıfırsa `job_done` yayınla.
6. **Varsayımlar / ihmal edilen süreçler:** İç içe paralellik yoktur; tek çağıran
   thread'den yeniden-girişlidir. Gövde bağımsızlığı çağıranın sözleşmesidir; havuz
   bunu denetlemez. Yük dengeleme kaba (dönüşümlü chunk); çok düzensiz işlerde statik
   dengeleme yetersiz kalabilir.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** Chunk hedefi `parts·4` tasarım
   seçimidir (yük dengeleme vs senkronizasyon maliyeti dengesi). Worker sayısı
   donanımdan otomatik. Kalibrasyon gerekmez; belirsizlik düşük.
8. **Sayısal yöntem, kararlılık:** Sonuç, çalıştırma sırasından bağımsızdır çünkü her
   indeks bağımsız ve kendi yuvasına yazar. Senkronizasyon tek kilit + iki koşul
   değişkeniyle yapılır; jenerasyon sayacı yarışları önler (worker yalnız kendi
   neslini bir kez işaretler). Boş aralık (`end <= begin`) iş göndermez.
9. **Doğrulama planı:** `tests/test_gpu.c` `scheduler_determinism`: 100000 indeks
   üzerinde seri ve paralel sonuç `memcmp` ile bit-özdeş; ikinci gönderim havuzu
   yeniden kullanır ve yine bit-özdeş; `worker_count <= hardware-1`; sıfır-worker
   havuz seri fallback ile bit-özdeş. `scheduler_empty_range`: boş/ters aralık iş
   göndermez. `tools/ws_perf.c` `run_scaling`: seri/paralel süre ve `speedup`,
   `identical` bayrağı (örnek: 3 worker, `speedup=3.29x`, `identical=1`).
10. **Kabul eşikleri ve sınırlar:** Paralel sonuç seri sonuçla bit-özdeş olmalıdır
    (`identical=1`); ihlal bir hatadır. Hızlanma ölçülen bir değerdir, garanti değil.
    Bilinen sınır: otoritatif simülasyon adımı henüz paralelleştirilmemiştir; havuz
    altyapısı ve determinizm kanıtı hazırdır. Kanıt düzeyi **D2** (bit-özdeşlik ve
    ölçekleme testleri geçti).
11. **Değişiklik kaydı:** Chunk politikası, senkronizasyon şeması veya worker seçimi
    değişirse bu kart, `src/core/ws_sched.*`, ilgili testler ve
    `docs/P10-GPU-VE-PERFORMANS.md` birlikte güncellenir.
