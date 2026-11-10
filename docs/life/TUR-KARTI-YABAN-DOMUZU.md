# Tür kartı — Yaban domuzu (*Sus scrofa*)

1. **Kimlik / sürüm / modül:** `life.species.wild_boar` v1; `src/life/ws_species.c`
   (`WS_SPECIES_BOAR`), `src/life/ws_quadruped.c`, `src/life/ws_ecology.c`.
2. **Temsil edilen süreç ve amaç:** Yaban domuzu ilk release'in büyük omnivor
   türüdür (`01-ILK-RELEASE-VE-KAPSAM.md`). Bu kart; fiziksel bedeni, türe özgü
   yürüyüşü ve ekolojik yaşam döngüsü parametrelerini kaydeder. Amaç, domuzun
   ekosistemde yavaş hareket eden, geniş algılı bir omnivor olarak yer alması ve
   hazır animasyon olmadan fiziksel bedenle hareket etmesidir.
3. **Ölçek:** Uzamsal: gövde uzunluğu ~1,5 m, omuz yüksekliği ~0,85 m, kütle
   ~80 kg. Zamansal: gövde dinamiği 1/500 s; ekoloji adımı 0,05 s.
4. **Durum değişkenleri (SI):** Beden profili (kütle, uzunluklar, duruş, adım
   frekansı, görev faktörü, sürtünme). Ekolojik ajan: konum (m), yönelim (rad),
   enerji rezervi [0,1], yaş (s), ısırık sayısı, yavru sayısı.
5. **Denklemler / algoritma:** Katı cisim gövde dinamiği + türe özgü dört ayaklı
   yürüyüş. Ekolojide otlatma: ısırık başına bitki biyokütlesi tüketilir ve
   `forage_gain` kadar rezerve dönüşür; olgun ve iyi beslenmiş birey ürer;
   metabolik maliyet rezervi tüketir. Domuz omnivordur; ilk release'te besin
   kaynağı bitki ızgarasıdır.
6. **Varsayımlar / geçerlilik:** Beden düzlemseldir (sagittal). Domuz gerçekte
   leş, omurgasız ve toprak kazma kaynaklarını da kullanır; ilk release'te diyet
   bitki ızgarasıyla temsil edilir. Ekolojik oranlar simülasyon ölçeğine
   sıkıştırılmıştır.
7. **Parametre kaynağı ve belirsizlik:** `src/life/ws_species.c` içinde açıkça
   kayıtlı. Kütle/boy literatür-bilgili; üreme ve metabolizma oranları kalibre
   edilmemiştir. D3 için ölçülmüş demografi ve diyet verisiyle nicel kıyas
   eklenmelidir.
8. **Sayısal yöntem / kararlılık:** 0,05 s ekoloji adımı; rezerv [0,1]; bitki
   hücresi sıfırın altına inemez; popülasyon ajan dizisiyle sınırlıdır.
9. **Doğrulama planı:** Dört tür birlikte kalıcılık; kaynak şoku altında sınırlı
   popülasyon; determinizm (bkz. `tests/test_ecology.c`).
10. **Kabul eşikleri ve sınırlar:** Domuz popülasyonu sınırlı (patlama yok);
    rezerv [0,1]; 3600 s koşuda dört tür birlikte hayatta. Kanıt düzeyi **D2**
    (korunum, sınır ve çok tohumlu testler geçti; nicel D3 kıyası yok).
11. **Değişiklik kaydı:** Beden veya yaşam parametresi değişirse bu kart,
    `ws_species.c` ve ekoloji testleri birlikte güncellenir.
