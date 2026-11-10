# P7 — Ekosistem ve temsilî türler

Bu belge `13-YOL-HARITASI.md` P7 aşamasının durumunu ve kanıtlarını kaydeder.
P7, ilk release'in dört temsilî türünü (`01-ILK-RELEASE-VE-KAPSAM.md`) ve
`07-BIYOLOJI-EKOLOJI.md`'nin istediği ekolojik bağlantıları ekler: bitki
üretimi, otçul tüketimi, avcı-av enerji aktarımı, üreme, ölüm ve popülasyon.
Hiçbir davranış kameraya veya kullanıcı arayüzüne bakmaz. Üçüncü taraf ekoloji,
popülasyon, yapay zekâ veya oyun motoru kullanılmaz; tüm kod ISO C17'dir.

## Dilim 1 — Tür kataloğu (tamamlandı)

`src/life/ws_species.*`: dört tür için sabit, literatür-bilgili katalog. Her tür;
gerçek fiziksel beden profili (kütle, gövde uzunluğu, omuz yüksekliği, bacak
uzunlukları, duruş açıklığı, adım frekansı, görev faktörü, sürtünme) ve türe özgü
yürüyüş hızı taşır. P5 dört ayaklı modülü bu profilden gövdeyi kurar. Ayrıca
ekolojinin gerektirdiği yaşam döngüsü ve beslenme parametreleri (diyet, en
yüksek hız, algı menzili, ısırık, metabolik oranlar, üreme eşiği/maliyeti/oranı,
yaşam süresi) kayıtlıdır.

- Kızıl geyik (*Cervus elaphus*) — otçul, yüksek ayrıntılı referans tür.
- Kızıl tilki (*Vulpes vulpes*) — karnivor, genelci; tek besin ağı kenarı.
- Avrupa tavşanı (*Oryctolagus cuniculus*) — otçul, tilkinin avı. (Sözleşmede
  adı "Avrupa tavşanı" olarak sabittir; kod ve kartlar buna uydurulmuştur.)
- Yaban domuzu (*Sus scrofa*) — omnivor.

Her tür için ayrı tür kartı `docs/life/TUR-KARTI-*.md` içindedir; kanıt düzeyi
D2'dir (beden ve ekoloji testleri geçti; nicel D3 kıyası yoktur).

## Dilim 2 — Popülasyon ve enerji modeli (tamamlandı)

`src/life/ws_ecology.*`: `07-BIYOLOJI-EKOLOJI.md` uyarınca deterministik,
headless, ajan tabanlı ekosistem. Kaba bir bitki-kaynak ızgarası üzerinde çalışır.

- **Bitki üretimi:** her hücrede lojistik büyüme (`plant_capacity` sınırı) artı
  düşük bir tohum-yağmuru terimi (sıfıra otlatılan hücre emici duruma düşmez).
- **Otlatma:** talep-güdümlü; hayvan rezervini doldurana kadar ısırık başına
  `bite` kadar biyokütle tüketir. Tüketim metabolizmayla sınırlıdır, kaynak
  sınırsız madencilikle tüketilemez.
- **Metabolizma:** bazal + hıza bağlı maliyet; rezerv [0,1] aralığında tutulur.
- **Üreme:** olgun ve `repro_energy` üstünde birey, fazlalıkla artan oranla yavru
  üretir; her yavru `repro_cost` kadar rezerv harcar. Dereceli tepki, sert
  aç/kapa salınımlarını yumuşatır.
- **Ölüm:** açlık (rezerv sıfır) veya yaşam süresi.
- **Dağılma:** çok düşük sayıya inen tür, modelin dışındaki dünyadan göçmen
  alabilir (yerel sıfır-durum tuzağını engeller; bol popülasyonda etkisiz).

Bütün rastgelelik tek bir `ws_rng` akışından, sabit ajan yineleme sırası ve
indeksle bozulan eşitliklerle gelir; aynı tohum bit-özdeş koşu verir.

## Dilim 3 — Av-avcı eşleşmesi ve kaçış (tamamlandı)

- **Avlanma:** aç bir karnivor (tilki) algı menzilindeki en yakın ava (tavşan)
  yönelir. Yakalama olasılığı av yoğunluğuna bağlıdır; Holling tip III tepkisi
  düşük av yoğunluğunda yakalamayı bastırır (av sığınağı), böylece avcı avı yok
  etmeden birlikte kalıcı olurlar. Başarılı avdan sonra işleme süresi uygulanır.
- **Kaçış:** otçul, algı menzilindeki en yakın karnivordan en yüksek hızıyla
  uzaklaşır.
- **Enerji aktarımı:** avlanan avın rezervinin bir kısmı avcıya geçer; kayıp
  ölümle ilişkilendirilir. Besin ağı ilk release'te tek kenardır (tilki → tavşan).

## Dilim 4 — Sahne köprüsü ve golden scene (tamamlandı)

- `ws_env_scene` (`src/render/ws_env_scene.*`) yeni bir `ecosystem` sahnesi
  (scene_kind 6) kazanır: ekosistem headless koşar (600 s simülasyon zamanı);
  hayatta kalan her türden bir temsilci (determinizm için en düşük indeks) alınır
  ve kameranın baktığı noktanın çevresine, görüş yönüne dik bir sıraya
  yerleştirilir; her temsilci P5 fiziksel bedeniyle kısa bir yürüyüş sonrası
  pozuyla çizilir. Kamera **hayatta kalan bireylerin simülasyonun bıraktığı
  gerçek konumlarına** bakar; görüntü script edilmiş bir dizilim değil, popülasyon
  sonucunun özetidir. Temsilciler, dört türün aynı karede okunabilmesi için
  tür-bazlı renklerle çizilir.
- Katalog artık on üç sahne: `orbit`, `lowlight`, `spheres`, `materials`,
  `textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`,
  `terrain_physics`, `creature_walk`, `organism_forage`, `ecosystem`.
- Diğer on iki sahne byte-özdeş kalır (regresyon yok). `ecosystem` da dinamik
  artiküle simülasyon içerdiğinden, derleyiciler arası son-bit `libm` farkı
  nedeniyle `creature_walk` gibi belgelenmiş gevşek toleransla kapılanır.

## Model kartları ve kanıt düzeyi

`docs/life/` altında P7 kartları vardır: `MODEL-KARTI-EKOLOJI.md` (popülasyon
dinamiği) ve dört tür kartı (`TUR-KARTI-*`). Kanıt düzeyleri kartlarda açıkça
kayıtlıdır; hiçbir model D1/D2 üzerinden D3 diye tanıtılmaz. Ekolojik oranlar
ölçülmüş saha verisine kalibre edilmemiştir.

## Otomatik doğrulama

- `ws_test_ecology` (13 test): tür tablosu tutarlılığı; bitki büyümesinin
  kapasiteyle sınırlılığı; rezerv ve biyokütle korunumu (bedava enerji yok);
  nüfus patlaması yok; av-avcı eşleşmesi ve sınırları; üreme ve ölüm olayları;
  açlıkta ölüm; kaynak şoku; dört tür birlikte kalıcılık (12 sabit tohum,
  3600 s); 10 tohum determinizm ve farklı tohumun ıraksaması; uzun koşu
  kararlılığı; geçersiz girdi reddi.
- `ws_test_world_env` içine eklenen test, `ecosystem` sahnesinin aynı tohumla
  aynı temsilcileri ve gövde pozlarını ürettiğini doğrular.
- Golden harness on üç sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` on üç sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.
- Yerel kanıt (gcc/clang, Debug+ASan/UBSan ve Release): dokuz test paketi geçer;
  ASan/UBSan temiz.

## Bilinen sınırlamalar

- Ekosistem tek bir homojen yamada (300 m) çalışır; mekânsal habitat mozaiği,
  göç dalgaları, hastalık, mevsimsellik ve sosyal yapı modellenmemiştir.
- Besin ağı ilk release'te tek kenardır (tilki → tavşan). Domuz omnivordur ama
  diyeti bitki ızgarasıyla temsil edilir; geyik ve tavşan arasında rekabet
  mekanizması henüz yoktur.
- Ekolojik oranlar ölçülmüş veriyle kalibre edilmemiştir; zaman ölçeği
  sıkıştırılmıştır. D3 için ölçülmüş popülasyon zaman serileriyle nicel kıyas
  gerekir.
- Beden modelleri düzlemseldir (sagittal); lateral denge ve yaw torku yoktur.
- Sahne köprüsü P5 sagittal gövdesini simülasyon sonucu konuma/yönelime
  yerleştirir; gövdenin ekoloji yolunu adım adım takip etmesi (gövde yolu
  izler) sonraki dilime bırakılmıştır.
- Kamera etkisizliği testi, aynı tohum ve aynı girdiyle popülasyonun ve
  temsilcilerin birebir tekrarlanmasını doğrular; render katmanı ekoloji
  durumuna yazmaz.
