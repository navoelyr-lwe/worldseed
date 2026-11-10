# P8 — Kalıcı ve çok ölçekli dünya

Bu belge `13-YOL-HARITASI.md` P8 aşamasının durumunu ve kanıtlarını kaydeder.
P8, `10-SES-KALICILIK-TEKRAR.md`'nin istediği kalıcılığı ve `04-DUNYA-VE-CEVRE.md`
ile `09-SINEMATIK-YONETMEN.md`'nin gerektirdiği "kamera taşınırken simülasyon
adımı durmaz" ilkesini ekler: bağlı bölgeler, region streaming, near/mid/far
simülasyon seviyeleri, olay günlüğü (event journal), checkpoint ve deterministik
replay. Hiçbir davranış kameraya veya kullanıcı arayüzüne bakmaz; dünya host'u
görüntüleyiciden ayrıdır. Üçüncü taraf dünya, streaming, kalıcılık veya oyun
motoru kullanılmaz; tüm kod ISO C17'dir ve yalnız C17 standart kütüphanesine
(`math.h`, `string.h`) dayanır.

## Dilim 1 — Bölge geometrisi ve kimliği (tamamlandı)

`src/world/ws_region.*`: dünyayı sabit kenarlı bölgelere böler. Dünya konumu
`(wx, wy)` → bölge koordinatı `(region.x, region.y)` ve bölge-yerel konum
`(local_x, local_y)`; tersine dönüşüm de vardır (floating-origin güvenli).

- **Kimlik:** `ws_region_hash` bir bölgenin kimliğini yalnız tohum ve bölge
  koordinatının saf fonksiyonu olarak üretir. Aynı bölge her zaman aynı kimliği
  taşır; bu, uzak (henüz yüklenmemiş) bölgelerin içeriğinin yeniden türetilebilir
  olmasını sağlar.
- **Komşuluk ve mesafe:** `ws_region_adjacent` (yüz paylaşan komşu; köşegen
  komşu değil) ve `ws_region_distance` (Chebyshev) seviye atamasında kullanılır.
- Tüm aritmetik deterministiktir; `int64` bölge koordinatları negatif bölgeleri
  de doğru temsil eder.

## Dilim 2 — Toplam (seviye-duyarlı) popülasyon modeli (tamamlandı)

`src/life/ws_population.*`: P7'nin ajan tabanlı `ws_ecology` modelinin **toplam
(aggregate)** eşdeğeri. Uzak bölgeler için binlerce ajanı tek tek çözmek yerine,
o bölgenin toplam biyokütlesini ve tür sayılarını kapalı bir denklemle ilerletir.

- **Bitki üretimi:** lojistik büyüme (`plant_capacity` sınırı) artı düşük bir
  tohum-yağmuru terimi (sıfıra otlatılan bölge emici duruma düşmez).
- **Otçul tüketimi:** rezerv doygunluğuna bağlı, tür-başına otlama katsayısıyla
  sınırlı (kaynak sınırsız madencilikle tüketilemez).
- **Av-avcı aktarımı:** av sayısına bağlı, Holling tip II benzeri doygun tepki;
  avcı yok etmeden birlikte kalıcı olurlar.
- **Üreme/ölüm:** lojistik öz-kuvvetlendirme ve taşıma kapasitesine yumuşak
  yaklaşma; tür-başına taşıma kapasitesi (`count_cap`) üst sınırı aşılmaz.
- **Determinizm:** bütün katsayılar tohumdan türetilir; rastgelelik yoktur. Aynı
  tohum ve aynı zaman ızgarası bit-özdeş sonuç verir.

`docs/life/MODEL-KARTI-POPULASYON.md` kartı parametreleri, varsayımları ve kanıt
düzeyini (D2) kaydeder. Model, P7 ajan tabanlı ekolojinin yerine geçmez; uzak/mid
ölçekte onun toplam vekilidir.

## Dilim 3 — Dünya host'u: streaming, seviyeler, journal (tamamlandı)

`src/world/ws_world_host.*`: dünyanın otoritesidir. Sabit boyutlu, ayrılmamış
(yığın) depolama kullanır; streaming maliyeti sınırlı ve deterministiktir.

- **Yüklü küme:** odak (focus) bölgesi çevresinde Chebyshev yarıçapı
  `radius_load` olan kare. Odak hareket ettikçe bu kare kayar; dışarı çıkan
  bölgeler boşaltılır, içeri girenler yüklenir. Her ikisi de journal'a yazılır.
- **Simülasyon seviyeleri:** yüklü her bölge odağa Chebyshev uzaklığına göre
  NEAR / MID / FAR atanır ve kendi kadansıyla ilerletilir (1 s / 4 s / 16 s).
  Böylece uzak bölgeler daha ucuza ilerlerken dünya yine de durmaz. Kadans
  ızgarasından bağımsızlık için adımlar sabit 1 s alt-adımlara bölünür.
- **Kamera dışı ilerleme:** odaktan uzaktaki bölgeler de (FAR kadansıyla)
  ilerler; görüntüleyici bir bölgeye bakmasa bile o bölgenin durumu değişir.
- **Boşalt/yükle durum korunumu:** yeni boşaltılan bölgenin toplam durumu
  sınırlı bir tahliye önbelleğinde (`WS_HOST_EVICT_CACHE`, iki tam yüklü kümeyi
  tutacak boyutta) saklanır; bölge geri yüklendiğinde evrimleşmiş durumu aynen
  geri gelir (yeniden tohumlanmaz). Önbellekten taşan bölge, belgelenmiş
  çevrimdışı model uyarınca tohumundan yeniden türetilir.
- **Olay günlüğü (journal):** her yükleme, boşaltma, seviye değişimi, odak
  hareketi ve tür yok oluşu, artan sıra numarasıyla sabit boyutlu sıralı bir
  halka tampona yazılır (`ws_event`, `WS_EVENT_CUSTOM`). Sinematik yönetmen
  buradan **gerçek olayları** gözler; olay uydurmaz.

## Dilim 4 — Checkpoint, replay ve sahne köprüsü (tamamlandı)

- **Checkpoint (`ws_world_host_save` / `_load`):** tüm host durumu alan alan,
  açık endian ve boyut sınırlı biçimde (`ws_serialize`) yazılır. Ham C struct
  diske yazılmaz. Bozuk veya kısaltılmış kayıt `WS_ERR_CORRUPT` ile reddedilir;
  yetersiz tampon `WS_ERR_CAPACITY` döner (sessiz kırpma yok).
- **Replay (`ws_world_host_replay`, `ws_world_host_digest`):** host durumunun
  SHA-256 özeti iki eşit durumu eşitler. Aynı config ve aynı odak yolu, aynı
  özeti verir; checkpoint'ten geri yüklenen host, kalan koşuyu birebir üretir.
- **Sahne köprüsü:** `ws_env_scene` yeni bir `world_stream` sahnesi (scene_kind
  7) kazanır: host headless koşar (odak sabit bir yol izler, her bacak 90 s);
  yüklü küme, seviyeye göre renklendirilmiş döşemeler (NEAR yeşil, MID amber,
  FAR mavi-gri; odak bölgesi parlak) olarak çizilir. Kamera tüm yüklü kümeyi
  yukarıdan görür; görüntü host'un gerçek sonucudur, script edilmiş dizilim
  değildir.
- Katalog artık on dört sahne: `orbit`, `lowlight`, `spheres`, `materials`,
  `textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`,
  `terrain_physics`, `creature_walk`, `organism_forage`, `ecosystem`,
  `world_stream`. Diğer on üç sahne byte-özdeş kalır (regresyon yok).
  `world_stream` deterministik host mantığı içerdiğinden (sıcak yolda aşkın
  `libm` çağrısı yok) derleyiciler arası piksel-kararlıdır ve statik sahneler
  gibi sıkı toleransla kapılanır.

## Model kartları ve kanıt düzeyi

`docs/world/` altında P8 kartı vardır: `MODEL-KARTI-BOLGE-POPULASYONU.md`
(toplam bölge popülasyonu). Kanıt düzeyi D2'dir (korunum, sınır, çok tohum ve
determinizm testleri geçti); nicel saha verisiyle D3 kıyası yoktur.

## Otomatik doğrulama

- `ws_test_world_host` (13 test): bölge geometrisi ve kimliği; toplam
  popülasyonun sınırlılığı; av-avcı eşleşmesi; yüklü küme ve seviye ataması;
  kamera dışı ilerleme; boşalt/yükle durum korunumu; streaming sınırları;
  journal bütünlüğü ve sıralaması; checkpoint save/load round-trip (bozuk ve
  kısaltılmış kayıt reddi dâhil); özetle replay determinizmi; on tohum replay;
  uzun koşu kararlılığı; geçersiz girdi reddi.
- `ws_test_world_env` içine eklenen test, `world_stream` sahnesinin aynı tohumla
  aynı döşemeleri, aynı özeti ve aynı koşu özetini ürettiğini doğrular.
- Golden harness on dört sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` on dört sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.

## Bilinen sınırlamalar

- Bölgeler homojen toplam modelle ilerletilir; P7 ajan tabanlı ekolojisi yalnız
  yüklü odak bölgesinde değil, tüm bölgelerde toplam vekille temsil edilir.
  Ajan-bazlı ve toplam modelin ölçekler arası eşlenmesi (mid/far → near geçişte
  ajanlara geri açma) sonraki dilimlere bırakılmıştır.
- Tahliye önbelleği sınırlıdır (iki yüklü küme). Çok uzun menzilli, çok adımlı
  gezintide taşan bölgeler tohumundan yeniden türetilir; bu belgelenmiş
  çevrimdışı modeldir ve bit-özdeş determinizmi korur, ancak o bölgede geçen
  ara durum korunmaz.
- Host görüntüleyiciden ayrı tasarlanmıştır; süreçler arası canlı host (görüntü
  kapatıldığında host açık kalır) sonraki ürün aşamasındadır.
- Bilgisayar uykuya/kapalı duruma geçtiğinde gerçek simülasyon ilerlediği
  söylenmez; çevrimdışı süre telafisi ayrı ve açık bir model kararıdır.
- Seviye kadansları (1/4/16 s) tasarım seçimidir; performans-kalite dengesi için
  ölçülmüş kalibrasyon sonraki performans dilimindedir.
