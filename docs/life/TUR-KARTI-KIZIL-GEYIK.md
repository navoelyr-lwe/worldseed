# Tür kartı — Kızıl geyik (*Cervus elaphus*)

1. **Kimlik / sürüm / modül:** `life.species.red_deer` v1; `src/life/ws_species.c`
   (`WS_SPECIES_DEER`), `src/life/ws_quadruped.c`, `src/life/ws_ecology.c`.
2. **Temsil edilen süreç ve amaç:** Kızıl geyik ilk release'in yüksek ayrıntılı
   referans türüdür (`01-ILK-RELEASE-VE-KAPSAM.md`). Bu kart; fiziksel beden
   ölçülerini, türe özgü yürüyüşü, duyu/ihtiyaç eşiklerini ve ekolojik yaşam
   döngüsü parametrelerini kaydeder. Amaç, türün hazır animasyon olmadan
   fiziksel beden + türe özgü hareket kontrolüyle hareket etmesi ve ekosistemde
   otçul olarak yer almasıdır.
3. **Ölçek:** Uzamsal: gövde uzunluğu ~1,6 m, omuz yüksekliği ~1,05 m, kütle
   ~110 kg. Zamansal: gövde dinamiği 1/500 s; ekoloji adımı 0,05 s (yerel ayar,
   ölçülmüş değil).
4. **Durum değişkenleri (SI):** Beden profili: kütle (kg), gövde uzunluğu,
   omuz yüksekliği, üst/alt bacak uzunluğu, duruş açıklığı (m), adım frekansı
   (Hz), görev faktörü, sürtünme katsayısı. Ekolojik ajan: konum (m), yönelim
   (rad), enerji rezervi [0,1], yaş (s), kat edilen yol (m), ısırık sayısı.
5. **Denklemler / algoritma:** Katı cisim gövde dinamiği ve kapalı-çevrim
   yürüyüş kontrolcüsü (`06-CANLI-BIYOMEKANIK.md`, yürüyüş kontrolü kartı).
   Ekolojide: enerji rezervi bazal + hıza bağlı metabolik maliyetle azalır;
   otlatma ısırık başına bitki biyokütlesi × `forage_gain` kadar doldurur;
   olgun ve iyi beslenmiş birey oranla ürer; rezerv sıfırlanınca veya yaşam
   süresi dolunca ölür.
6. **Varsayımlar / geçerlilik:** Beden düzlemseldir (sagittal). Değerler
   literatür-bilgili mühendislik tahminleridir; belirli bir yaban popülasyonunun
   ölçümü değildir. Ekolojik oranlar bir oyun/simülasyon ölçeğine (dakikalar)
   sıkıştırılmıştır, gerçek ekolojik zamana göre değildir.
7. **Parametre kaynağı ve belirsizlik:** Profil ve yaşam parametreleri
   `src/life/ws_species.c` içinde açıkça kayıtlıdır. Kütle/boy literatür-bilgili;
   metabolik ve üreme oranları kalibre edilmemiştir (belirsizlik yüksek). D3
   için türe özgü ölçülmüş metabolizma ve demografi verisiyle nicel kıyas
   eklenmelidir.
8. **Sayısal yöntem / kararlılık:** 1/500 s sabit adım (beden), 0,05 s ekoloji
   adımı. Rezerv [0,1] aralığına kırpılır; bitki hücresi sıfırın altına inemez;
   popülasyon ajan dizisi kapasitesiyle sınırlıdır.
9. **Doğrulama planı:** Aynı tohumla bit-özdeş ekolojik koşu; en az 10 sabit
   tohumla dört tür birlikte kalıcılık; av-avcı sınırları; kaynak şoku;
   uzun süreli headless kararlılık (bkz. `tests/test_ecology.c`).
10. **Kabul eşikleri ve sınırlar:** Geyik popülasyonu sınırlı (patlama yok);
    rezerv [0,1]; 3600 s koşuda dört tür birlikte hayatta. Kanıt düzeyi **D2**
    (korunum, kararlılık ve çok tohumlu testler geçti; nicel D3 kıyası yok).
11. **Değişiklik kaydı:** Anatomi veya yaşam parametresi değişirse bu kart,
    `ws_species.c` ve ilgili ekoloji testleri birlikte güncellenir.
