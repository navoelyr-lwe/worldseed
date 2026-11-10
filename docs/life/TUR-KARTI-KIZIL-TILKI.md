# Tür kartı — Kızıl tilki (*Vulpes vulpes*)

1. **Kimlik / sürüm / modül:** `life.species.red_fox` v1; `src/life/ws_species.c`
   (`WS_SPECIES_FOX`), `src/life/ws_quadruped.c`, `src/life/ws_ecology.c`.
2. **Temsil edilen süreç ve amaç:** Kızıl tilki ilk release'in avcı (karnivor)
   türüdür (`01-ILK-RELEASE-VE-KAPSAM.md`). Bu kart; fiziksel bedeni, türe özgü
   yürüyüşü ve ekolojik avlanma/yaşam döngüsü parametrelerini kaydeder. Amaç,
   tilkinin ekosistemde gerçek bir besin ağı kenarı olarak tavşanı avlaması ve
   hazır animasyon olmadan fiziksel bedenle hareket etmesidir.
3. **Ölçek:** Uzamsal: gövde uzunluğu ~0,65 m, omuz yüksekliği ~0,38 m, kütle
   ~6 kg. Zamansal: gövde dinamiği 1/500 s; ekoloji adımı 0,05 s.
4. **Durum değişkenleri (SI):** Beden profili (kütle, uzunluklar, duruş, adım
   frekansı, görev faktörü, sürtünme). Ekolojik ajan: konum (m), yönelim (rad),
   enerji rezervi [0,1], yaş (s), avlanma bekleme süresi (s), öldürme sayısı.
5. **Denklemler / algoritma:** Katı cisim gövde dinamiği + türe özgü dört ayaklı
   yürüyüş. Ekolojide avlanma: aç tilki algı menzilindeki en yakın tavşana
   yönelir; yakalama yarıçapı içinde yakalama olasılığı av yoğunluğuna bağlı
   Holling tip III tepkisiyle (düşük av yoğunluğunda sığınak) hesaplanır; başarılı
   avlanmadan sonra tilki bir işleme süresi boyunca doymuş kalır. Tilki
   genelcidir: av kıt olduğunda düşük kaliteli bitkisel/omurgasız besinle
   (düşük `plant_gain`) hayatta kalır.
6. **Varsayımlar / geçerlilik:** Beden düzlemseldir (sagittal). Avlanma tek
   sabit kenardır (tilki → tavşan); ilk release'te besin ağı bu kenarla sınırlıdır.
   Yakalama parametreleri kalibre edilmemiş mühendislik değerleridir.
7. **Parametre kaynağı ve belirsizlik:** `src/life/ws_species.c` içinde açıkça
   kayıtlı. Kütle/boy literatür-bilgili; avlanma oranları ve yarı-doygunluk
   sabiti kalibre edilmemiştir. D3 için ölçülmüş avlanma başarısı ve diyet
   kompozisyonu verisiyle nicel kıyas eklenmelidir.
8. **Sayısal yöntem / kararlılık:** 0,05 s ekoloji adımı; yakalama olasılığı
   `dt` ile ölçeklenir; rezerv ve av yoğunluğu [0,1] aralığında kırpılır. Tilki
   popülasyonu av kaynağıyla sınırlıdır (sınırsız büyüme testi).
9. **Doğrulama planı:** Av-avcı eşleşme testi (avlanma gerçekleşir, tavşan
   sayısı düşer, tilki patlamaz); dört tür birlikte kalıcılık; determinizm
   (bkz. `tests/test_ecology.c`).
10. **Kabul eşikleri ve sınırlar:** Avlanma olayı > 0; tilki popülasyonu
    sınırlı (< 200); tavşan ölümleri > 0. Kanıt düzeyi **D2** (korunum, sınır ve
    çok tohumlu testler geçti; nicel D3 kıyası yok).
11. **Değişiklik kaydı:** Besin ağı kenarı veya avlanma parametreleri değişirse
    bu kart, `ws_species.c` ve ekoloji testleri birlikte güncellenir.
