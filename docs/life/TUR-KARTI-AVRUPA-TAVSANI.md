# Tür kartı — Avrupa tavşanı (*Oryctolagus cuniculus*)

1. **Kimlik / sürüm / modül:** `life.species.european_rabbit` v1;
   `src/life/ws_species.c` (`WS_SPECIES_RABBIT`), `src/life/ws_quadruped.c`,
   `src/life/ws_ecology.c`.
2. **Temsil edilen süreç ve amaç:** Avrupa tavşanı ilk release'in küçük otçul
   türü ve tilkinin avıdır (`01-ILK-RELEASE-VE-KAPSAM.md`, `07-BIYOLOJI-EKOLOJI.md`).
   Bu kart; fiziksel bedeni, türe özgü yürüyüşü ve ekolojik yaşam döngüsü
   parametrelerini kaydeder. Tür adı sözleşmeye uygun olarak Avrupa tavşanı
   (*Oryctolagus cuniculus*) alınmıştır.
3. **Ölçek:** Uzamsal: gövde uzunluğu ~0,40 m, omuz yüksekliği ~0,18 m, kütle
   ~1,8 kg. Zamansal: gövde dinamiği 1/500 s; ekoloji adımı 0,05 s.
4. **Durum değişkenleri (SI):** Beden profili (kütle, uzunluklar, duruş, adım
   frekansı, görev faktörü, sürtünme). Ekolojik ajan: konum (m), yönelim (rad),
   enerji rezervi [0,1], yaş (s), ısırık sayısı, yavru sayısı.
5. **Denklemler / algoritma:** Katı cisim gövde dinamiği + türe özgü dört ayaklı
   yürüyüş. Ekolojide otlatma: ısırık başına bitki biyokütlesi tüketilir ve
   `forage_gain` kadar rezerve dönüşür; olgun ve iyi beslenmiş birey yüksek
   üreme oranıyla yavru üretir; metabolik maliyet rezervi tüketir. Tavşan bir
   avcı algıladığında en yüksek hızıyla kaçar.
6. **Varsayımlar / geçerlilik:** Beden düzlemseldir (sagittal). Tavşan gerçekte
   kazıcı bir türdür; ilk release'te yuva/kazma modeli yoktur. Ekolojik oranlar
   simülasyon ölçeğine sıkıştırılmıştır.
7. **Parametre kaynağı ve belirsizlik:** `src/life/ws_species.c` içinde açıkça
   kayıtlı. Kütle/boy literatür-bilgili; üreme ve metabolizma oranları kalibre
   edilmemiştir. D3 için ölçülmüş demografi (yavru/yıl, ölüm oranı) verisiyle
   nicel kıyas eklenmelidir.
8. **Sayısal yöntem / kararlılık:** 0,05 s ekoloji adımı; rezerv [0,1]; bitki
   hücresi sıfırın altına inemez; popülasyon ajan dizisiyle sınırlıdır.
9. **Doğrulama planı:** Üreme ve ölüm olayları; açlıkta ölüm; av-avcı
   eşleşmesinde tavşanın av olması; dört tür birlikte kalıcılık; determinizm
   (bkz. `tests/test_ecology.c`).
10. **Kabul eşikleri ve sınırlar:** Yavru sayısı artar; ölüm olayı > 0;
    avlanmada tavşan ölümleri > 0; dört tür birlikte 3600 s hayatta. Kanıt
    düzeyi **D2** (korunum, sınır ve çok tohumlu testler geçti; nicel D3 kıyası
    yok).
11. **Değişiklik kaydı:** Beden, üreme veya av parametreleri değişirse bu kart,
    `ws_species.c` ve ekoloji testleri birlikte güncellenir.
