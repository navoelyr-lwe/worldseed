# Model kartı — Ekosistem ve popülasyon dinamiği

1. **Kimlik / sürüm / modül:** `life.ecology` v1 (`WS_ECOLOGY_VERSION 1`);
   `src/life/ws_ecology.c`, `src/life/ws_ecology.h`; tür parametreleri
   `src/life/ws_species.c`.
2. **Temsil edilen süreç ve amaç:** `07-BIYOLOJI-EKOLOJI.md`'nin istediği
   ekolojik bağlantılar: bitki üretimi, otçul tüketimi, avcı-av enerji aktarımı,
   üreme, ölüm ve popülasyon. Amaç, dört temsilî türün tek bir ölçekli yamada
   kendi kendine sürdürülebilir, sınırlı ve deterministik bir ekosistem
   oluşturmasıdır.
3. **Ölçek:** Uzamsal: `area_m` = 300 m kenarlı kare yama (2×2 km dünyanın bir
   parçası); bitki ızgarası `resource_cells` = 24 (hücre ≈ 12,5 m). Zamansal:
   adım 0,05 s (yerel ayar). Bu ölçekler ölçülmüş değil, planlama değerleridir.
4. **Durum değişkenleri (SI):** Bitki ızgarası: hücre başına biyokütle (soyut
   birim, kapasite `plant_capacity`). Ajan: tür, cinsiyet, konum (m), yönelim
   (rad), enerji rezervi [0,1], yaş (s), kat edilen yol (m), avlanma bekleme
   süresi (s), ısırık/öldürme/yavru sayaçları. Topluluk: doğum, ölüm (tür
   başına), avlanma ve otlatma olayı sayaçları; seyreltilmiş popülasyon zaman
   serisi.
5. **Denklemler / algoritma:**
   - **Bitki büyümesi (lojistik):** her hücrede
     `b ← b + g·b·(1 − b/cap)·dt + seed·(1 − b/cap)`, `cap = plant_capacity ·
     shock_biomass`. İkinci terim (tohum yağmuru) sıfıra otlatılan hücrenin
     emici duruma düşmesini engeller.
   - **Otlatma:** talep-güdümlü; hayvan rezervini `forage_gain` ile hedefe
     dolduracak kadar biyokütle tüketir, ısırık başına `bite` ile sınırlıdır.
     Hücreden alınan miktar hücrenin tuttuğundan fazla olamaz.
   - **Metabolizma:** `cost = (basal_rate + move_rate·speed)·meta_scale·dt`.
   - **Avlanma:** karnivor açken algı menzilindeki en yakın ava yönelir;
     yakalama yarıçapı içinde yakalama olasılığı
     `pred_catch_rate · prey/(prey + pred_half_sat) · dt` ile hesaplanır
     (Holling tip III: düşük av yoğunluğunda sığınak). Başarılı avdan sonra
     `pred_handling_s` işleme süresi uygulanır.
   - **Kaçış:** otçul, algı menzilindeki en yakın karnivordan en yüksek hızıyla
     uzaklaşır.
   - **Üreme:** olgun ve `repro_energy` üstünde birey, fazlalıkla artan oranla
     (`repro_rate · repro_scale · surplus`) yavru üretir; her yavru
     `repro_cost` kadar rezerv harcar. Dereceli tepki, sert aç/kapa
     salınımlarını yumuşatır.
   - **Ölüm:** rezerv sıfırlanınca (açlık) veya `eco_lifespan_s` dolunca.
   - **Dağılma:** `immigrant_floor` altına düşen tür, dünyanın başka
     yamasından `immigrant_rate` ile göçmen alabilir (yerel sıfır-durum tuzağını
     engeller; bol popülasyonda etkisizdir).
6. **Varsayımlar / ihmal edilen süreçler:** Yama homojendir; mekânsal habitat
   mozaiği, göç dalgaları, hastalık, mevsimsellik, sosyal yapı ve tür içi
   rekabet modellenmemiştir. Bitki ızgarası tek bir "besin" soyutlamasıdır.
   Besin ağı ilk release'te tek kenardır (tilki → tavşan). Zaman ölçeği
   sıkıştırılmıştır: "saniye" simülasyon saniyesidir, gerçek ekolojik zaman
   değildir.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** `ws_eco_config_default`
   içindeki tüm değerler planlama değerleridir; `meta_scale` ve `forage_rate`
   tür tablosunu düzenlemeden taşıma kapasitesini ayarlamak için vardır. Hiçbir
   değer ölçülmüş bir saha verisine kalibre edilmemiştir; belirsizlik yüksektir.
   D3 için ölçülmüş popülasyon zaman serileriyle nicel kıyas gerekir.
8. **Sayısal yöntem, kararlılık:** Sabit 0,05 s adım (Euler). Rezerv [0,1];
   bitki hücresi [0, cap]; popülasyon `WS_ECOLOGY_MAX_AGENTS` = 512 ile sınırlı.
   Yakalama olasılığı `dt` ile ölçeklenir ve [0,1]'e kırpılır. Tüm rastgelelik
   tek bir `ws_rng` akışından, sabit ajan yineleme sırası ve indeksle bozulan
   eşitliklerle üretilir → aynı tohum bit-özdeş koşu.
9. **Doğrulama planı:** `tests/test_ecology.c` (13 test): tür tablosu tutarlılığı;
   bitki büyümesinin kapasiteyle sınırlılığı; rezerv/biyokütle korunumu; nüfus
   patlaması yok; av-avcı eşleşmesi ve sınırları; üreme ve ölüm; açlıkta ölüm;
   kaynak şoku; dört tür birlikte kalıcılık (12 tohum, 3600 s); 10 tohum
   determinizm ve farklı tohum ıraksaması; uzun koşu kararlılığı; geçersiz girdi.
10. **Kabul eşikleri ve sınırlar:** Dört tür 12 sabit tohumda 3600 s boyunca
    birlikte hayatta kalır; `alive_count < WS_ECOLOGY_MAX_AGENTS`; biyokütle
    şok kapasitesini aşmaz; iki özdeş tohum bit-özdeş ajan dizisi verir. Bilinen
    sınırlar: kalibre edilmemiş oranlar, tek kenarlı besin ağı, homojen yama.
    Kanıt düzeyi **D2** (korunum, kararlılık ve çok tohumlu testler geçti; nicel
    D3 kıyası yok).
11. **Değişiklik kaydı:** Denklem, parametre veya besin ağı kenarı değişirse bu
    kart, `ws_ecology.*`, `ws_species.*` ve ekoloji testleri birlikte güncellenir;
    dünya sürümü etkileniyorsa `13-YOL-HARITASI.md` kaydı da güncellenir.
