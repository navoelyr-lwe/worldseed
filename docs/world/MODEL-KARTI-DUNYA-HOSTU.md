# Model kartı — Dünya host'u (kalıcı, çok ölçekli dünya)

1. **Kimlik / sürüm / modül:** `world.host` v1 (`WS_HOST_VERSION 1`);
   `src/world/ws_world_host.c`, `src/world/ws_world_host.h`; bölge geometrisi
   `src/world/ws_region.*`; bölge popülasyonu `src/life/ws_population.*`.
2. **Temsil edilen süreç ve amaç:** `04-DUNYA-VE-CEVRE.md`'nin "kamera taşınırken
   simülasyon adımı durmaz" ilkesi ve `10-SES-KALICILIK-TEKRAR.md`'nin
   kalıcılık/replay gereksinimleri. Host, odak çevresindeki bağlı bölgeleri yükler,
   her birini seviyesine göre ilerletir, kamera dışındaki dünyanın ilerlemesini
   korur, olayları günlüğe yazar ve tüm durumu checkpoint'lenebilir/replay
   edilebilir kılar. Görüntüleyiciden bağımsızdır.
3. **Ölçek:** Uzamsal: bölge kenarı `region.cells · region.cell_size` (m); yüklü
   küme Chebyshev yarıçapı `radius_load` (varsayılan 3) karedir. Zamansal: host
   otorite adımı 1 s; seviye kadansları NEAR=1 s, MID=4 s, FAR=16 s. Bunlar
   planlama değerleridir.
4. **Durum değişkenleri (SI):** `tick` (sayım), `time_s` (s), `focus` (bölge
   koordinatı); yüklü her bölge için `coord`, `level`, `hash`, `pop` (bkz.
   `MODEL-KARTI-POPULASYON.md`), `last_step_s`, `extinct_mask`; tahliye önbelleği
   (boşaltılmış bölgelerin korunmuş durumu); journal halka tamponu
   (`ws_event` dizisi, `next_sequence`, `events_dropped`); sayaçlar
   `total_loads`, `total_unloads`, `extinctions`.
5. **Denklemler / algoritma:**
   - **Yüklü küme (streaming):** odak `(fx, fy)` için
     `{(x, y) : max(|x−fx|, |y−fy|) ≤ radius_load}`. Odak hareketinde dışarı
     çıkan bölgeler boşaltılır (tahliye önbelleğine), içeri girenler yüklenir.
     Yükleme sırası sabit kare taramasıyla deterministiktir.
   - **Seviye ataması:** Chebyshev uzaklığı `d` için
     `d ≤ radius_near → NEAR`, `≤ radius_mid → MID`, `≤ radius_load → FAR`,
     aksi hâlde UNLOADED.
   - **İlerletme:** yüklü bölge, son adımından bu yana geçen süre kadansını
     aştığında, geçen süre sabit 1 s alt-adımlara bölünerek `ws_pop_step` ile
     ilerletilir. Böylece sonuç kadans ızgarasından bağımsızdır.
   - **Yok oluş:** bir tür `count[k] ≤ 0` olduğunda `extinct_mask` biti kurulur,
     `extinctions` artar ve journal'a yazılır.
   - **Checkpoint:** tüm alanlar açık endian, boyut sınırlı biçimde yazılır;
     bozuk/kısaltılmış kayıt reddedilir.
   - **Replay:** host durumunun SHA-256 özeti; checkpoint + odak yolu aynı özeti
     üretir.
6. **Varsayımlar / ihmal edilen süreçler:** Bölgeler homojen toplam modelle
   ilerletilir; ajan-bazlı ve toplam modelin ölçekler arası eşlenmesi (mid/far →
   near geçişte ajanlara geri açma) yoktur. Tahliye önbelleği sınırlıdır (iki
   yüklü küme); taşan bölge tohumundan yeniden türetilir (belgelenmiş çevrimdışı
   model). Süreçler arası canlı host ve uyku/kapalı süre telafisi bu dilimde yok.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:**
   `ws_world_host_config_default` yarıçapları ve kadansları planlama değeri
   olarak seçer; ölçülmüş performans/kalite kalibrasyonu yoktur. Seviye kadansları
   tasarım seçimidir.
8. **Sayısal yöntem, kararlılık:** Sabit boyutlu, ayrılmamış depolama
   (`WS_HOST_MAX_LOADED` 64, `WS_HOST_EVICT_CACHE` 128, `WS_HOST_MAX_EVENTS` 512).
   Yüklü küme `(2·radius_load+1)²` ≤ 64 olacak şekilde yapılandırılır; aşımda
   `WS_ERR_CAPACITY`. Journal sıra numaraları kesin artar. Determinizm: tüm
   rastgelelik bölge hash'inden türetilir; host mantığı deterministiktir.
9. **Doğrulama planı:** `tests/test_world_host.c` (13 test): bölge geometrisi ve
   kimliği; toplam popülasyon sınırlılığı; av-avcı eşleşmesi; yüklü küme ve
   seviye ataması; kamera dışı ilerleme; boşalt/yükle durum korunumu; streaming
   sınırları; journal bütünlüğü/sıralaması; checkpoint round-trip (bozuk ve
   kısaltılmış kayıt reddi dâhil); özetle replay determinizmi; on tohum replay;
   uzun koşu kararlılığı; geçersiz girdi reddi. `tests/test_world_env.c` içindeki
   `world_stream` sahne testi de aynı tohumda aynı döşemeleri ve özeti doğrular.
10. **Kabul eşikleri ve sınırlar:** Yüklü küme her odakta tam kare; boşalt/yükle
    çevriminde evrimleşmiş durum korunur; aynı config + aynı odak yolu aynı SHA-256
    özeti verir; bozuk kayıt yüklenmez; uzun koşuda hiçbir değer taşmaz. Bilinen
    sınırlar: sınırlı tahliye önbelleği, kalibre edilmemiş kadanslar, ölçekler
    arası eşleme yok. Kanıt düzeyi **D2** (determinizm, bozuk kayıt ve uzun koşu
    testleri geçti; nicel D3 kıyası yok).
11. **Değişiklik kaydı:** Streaming, seviye, journal veya checkpoint biçimi
    değişirse bu kart, `ws_world_host.*` ve testleri birlikte güncellenir;
    checkpoint biçimi değişirse `WS_HOST_VERSION` artırılır ve migration notu
    eklenir.
