# Model kartı — Toplam bölge popülasyonu

1. **Kimlik / sürüm / modül:** `life.population` v1 (`WS_POP_VERSION 1`);
   `src/life/ws_population.c`, `src/life/ws_population.h`; tür parametreleri
   `src/life/ws_species.c` (P7) ile paylaşılır.
2. **Temsil edilen süreç ve amaç:** P7 ajan tabanlı ekolojinin (`ws_ecology`)
   **toplam (aggregate)** vekili. `04-DUNYA-VE-CEVRE.md` ve `10-SES-KALICILIK-TEKRAR.md`
   uyarınca, çok ölçekli dünyada uzak ve orta seviye (FAR/MID) bölgelerin her biri
   için binlerce ajanı tek tek çözmek yerine o bölgenin biyokütlesini ve tür
   sayılarını kapalı denklemlerle ilerletir. Amaç, kamera dışındaki dünyanın da
   ilerlemesi ve yaklaşık olarak doğru olmasıdır; tam ajan çözünürlüğü odak
   bölgesinde (NEAR) ayrı tutulur.
3. **Ölçek:** Uzamsal: bölge kenarı `area_m` (m); kapasiteler bu alana göre
   ölçeklenir. Zamansal: değişken adım `dt` (s); host NEAR=1 s, MID=4 s, FAR=16 s
   kadansıyla çağırır ve adımı sabit 1 s alt-adımlara böler. Bu ölçekler planlama
   değerleridir.
4. **Durum değişkenleri (SI):** `plant` (toplam bitki biyokütlesi), tür başına
   `count[k]` (sürekli popülasyon), kümülatif `births`, `deaths`, `extinctions`,
   `time_s` (s).
5. **Denklemler / algoritma (tümü kapalı, deterministik):**
   - **Bitki büyümesi (lojistik + tohum):**
     `P ← P + g·P·(1 − P/cap)·dt + seed·(1 − P/cap)`, `cap = plant_capacity·shock`.
     Tohum terimi sıfıra otlatılan bölgenin emici duruma düşmesini engeller.
   - **Otçul tüketimi:** tür `k` için tüketim
     `consume[k]·count[k]·P/(P + plant_half)` (doygun tepki); toplam tüketim
     `P`'yi negatif yapamaz.
   - **Av-avcı aktarımı:** avcı için av desteği
     `pred_gain·prey·pred_half/(prey + pred_half)` benzeri doygun kazanç; av
     üzerindeki baskı avcı sayısıyla sınırlıdır (av yok edilmeden birlikte
     kalıcılık).
   - **Üreme/ölüm:** tür başına net öz-kuvvet
     `growth[k]·count[k]·(1 − count[k]/cap_k)`, `cap_k = support[k]·(kaynak)`.
     Taşıma kapasitesine yumuşak yaklaşma; `count_cap[k]` sert üst sınır.
   - **Yok oluş:** `min_viable` altına inen tür o bölgede tükenir ve host
     tarafından journal'a yazılır.
6. **Varsayımlar / ihmal edilen süreçler:** Bölge homojendir; mekânsal mozaik,
   göç dalgaları, hastalık, mevsimsellik ve sosyal yapı yoktur. Birey kimliği,
   cinsiyet, yaş dağılımı ve hareket modellenmemiştir (bu bilinçli bir toplam
   yaklaşımdır). Besin ağı ilk release'te tek kenardır (avcı → av). Zaman ölçeği
   sıkıştırılmıştır.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** `ws_pop_config_default`
   tüm katsayıları tohumdan türetir; hiçbiri ölçülmüş saha verisine kalibre
   edilmemiştir. Belirsizlik yüksektir; D3 için ölçülmüş popülasyon zaman
   serileriyle nicel kıyas gerekir.
8. **Sayısal yöntem, kararlılık:** Euler, `dt` host tarafından ≤1 s alt-adımda
   verilir. `plant ∈ [0, cap]`, `count[k] ∈ [0, count_cap[k]]`; tüm değerler
   sonludur. Rastgelelik yoktur → aynı tohum ve aynı zaman ızgarası bit-özdeş
   sonuç verir.
9. **Doğrulama planı:** `tests/test_world_host.c` içinde: toplam popülasyonun
   sınırlılığı (200 000 adım sonunda `plant ∈ [0, cap]`, `count[k] ∈ [0, cap_k]`,
   hepsi sonlu); av-avcı eşleşmesi (avsız avcı azalır, avlı avcı kalıcı olur);
   uzun koşu kararlılığı.
10. **Kabul eşikleri ve sınırlar:** Uzun koşuda hiçbir değer taşmaz veya NaN
    olmaz; avsız avcı popülasyonu başlangıç değerinin altına iner. Bilinen
    sınırlar: kalibre edilmemiş oranlar, tek kenarlı besin ağı, birey çözünürlüğü
    yok. Kanıt düzeyi **D2** (korunum, sınır ve çok tohumlu determinizm testleri
    geçti; nicel D3 kıyası yok).
11. **Değişiklik kaydı:** Denklem, parametre veya besin ağı kenarı değişirse bu
    kart, `ws_population.*`, ilgili testler ve gerekiyorsa `ws_world_host.*`
    birlikte güncellenir.
