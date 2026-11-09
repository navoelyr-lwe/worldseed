# Model kartı — Referans tür: kızıl geyik (anatomi ve beden)

1. **Kimlik / sürüm / modül:** `life.species.red_deer` v1;
   `src/life/ws_quadruped.c` (`ws_species_red_deer`, `ws_quadruped_build`);
   `src/life/ws_creature.*`.
2. **Temsil edilen süreç ve amaç:** İlk referans türün (kızıl geyik, *Cervus
   elaphus*) fiziksel bedeni: gövde zinciri (torso, boyun, baş, ikişer segmentli
   ön/arka bacaklar), kütle/atalet, eklem açı sınırları ve ayak temas sensörleri.
   Amaç, türüne uygun hareketin hazır animasyon olmadan, fiziksel bedenden
   üretilmesini sağlamak.
3. **Ölçek:** Uzamsal: gövde uzunluğu ~1,6 m, omuz yüksekliği ~1,05 m; kütle
   ~110 kg. Zamansal: 1/500 s sabit adım.
4. **Durum değişkenleri (SI):** Her gövde: konum (m), açı (rad), hız (m/s),
   açısal hız (rad/s), kütle (kg), atalet (kg·m²), yarı-ölçü (m). Her eklem:
   rest açısı, min/max açı (rad), motor kp/kd, tork bütçesi (N·m). Her ayak:
   temas noktası, sürtünme katsayısı.
5. **Denklemler / algoritma:** Katı cisim dinamiği (yarı-kapalı Euler) + eklem
   nokta çözümü + sıralı impuls temas çözümü (bkz. yürüyüş kontrolü kartı).
   Segment yarı-ölçüleri, düzgün yoğunluklu katı kutu varsayımıyla kütle ve
   ataleti verir.
6. **Varsayımlar / geçerlilik:** Model düzlemseldir (sagittal); sağ/sol bacaklar
   ayrı dinamik değildir. Kas modeli kuvvet-uzunluk/hız ilişkisini
   basitleştirir. Kemik/kas/tendon ayrıntı düzeyi tek segment-kutu düzeyindedir.
7. **Parametre kaynağı:** Profil değerleri (kütle, gövde uzunluğu, omuz
   yüksekliği, bacak uzunlukları, duruş açıklığı, adım frekansı, görev faktörü,
   sürtünme) kızıl geyik için literatür-bilgili mühendislik değerleridir;
   `ws_species_red_deer` içinde açıkça kayıtlıdır. D3 için türe özgü ölçülmüş
   anatomi/yürüyüş verisiyle nicel kıyas eklenmelidir.
8. **Sayısal yöntem / kararlılık:** 1/500 s sabit adım, 30 çözücü iterasyonu,
   Baumgarte konum düzeltmesi 0,2. Uzun koşu testi (60 s) taşma olmadan geçer.
9. **Doğrulama planı:** Statik duruşta denge ve kütle merkezinin destek çokgeni
   içinde olması; ileri yürüyüşte kararlılık; eğimde duruş/yürüyüş; eklem
   sınırlarına uyum; determinizm; uzun koşuda enerji taşması olmaması.
10. **Kabul eşikleri ve sınırlar:** Statik duruşta gövde dik (|açı| < ~0,2 rad);
    yürüyüşte ileri hız ≈ 0,8 m/s; eklem açıları sınır ± 0,15 rad tolerans
    içinde; 60 s koşuda kinetik enerji < 4000 J; determinizm birebir (aynı
    girdi → aynı bit çıktısı). Kanıt düzeyi **D1** (oranlar literatür-bilgili;
    nicel D3 kıyası yok).
11. **Değişiklik kaydı:** Anatomi/mass/eklem şeması değişirse kart ve ilgili
    testler güncellenir.
