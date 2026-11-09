# P6 — Organizma ve davranış

Bu belge `13-YOL-HARITASI.md` P6 aşamasının durumunu ve kanıtlarını kaydeder.
P6, referans organizmanın algı, iç durum (ihtiyaç/homeostaz), hafıza, hedef,
davranış ve sınırlı öğrenmesini kendi ISO C17 kodumuzla ekler
(`07-BIYOLOJI-EKOLOJI.md`). Hiçbir davranış kameraya veya kullanıcı arayüzüne
bakmaz; algı yalnız dünyadaki gerçek kaynak/tehdit konumlarından ve organizmanın
kendi iç durumundan türetilir. Üçüncü taraf yapay zekâ, davranış ağacı motoru
veya öğrenme kütüphanesi kullanılmaz.

## Dilim 1 — Algı (tamamlandı)

`src/life/ws_perception.*`: her kontrol adımında canlının bulunduğu konum ve
yönelimden yola çıkarak menzil + görüş alanı içindeki **en yakın** besin, su ve
tehdit noktasını seçer. Çıktı her kanal için görülme durumu, mesafe, yön (bearing)
ve dünya konumudur; ayrıca iç durum (enerji, hidrasyon, vücut sıcaklığı,
yorgunluk) kopyalanır. Tükenmiş (değeri sıfır) kaynak algılanmaz. Algı girdisi
yalnız dünya durumudur; kamera/UI girdisi yoktur.

## Dilim 2 — İç durum ve homeostaz (tamamlandı)

`src/life/ws_organism.*`: organizma enerji, hidrasyon, vücut sıcaklığı,
yorgunluk ve sağlık durumunu [0,1] oranlarında tutar. İhtiyaç dürtüleri
(açlık = 1−enerji, susuzluk = 1−hidrasyon, ısı rahatsızlığı = |T−T_konfor|/ölçek,
dinlenme = yorgunluk) türetilir; `ws_organism_most_urgent` eşiği aşan en acil
ihtiyacı belirleyicidir (eşitlikte düşük indeks). Hareket bazal + hıza bağlı
maliyetle rezervleri tüketir; yemek/içmek doldurur; dinlenme yorgunluğu geri
kazandırır; termoregülasyon vücut sıcaklığını aktivite ısısı ve ortam sıcaklığına
göre hedefe gevşetir. Sağlık **yalnızca** bir rezerv tükendiğinde azalır; sağlık
sıfıra inince canlı ölür. "Bedava enerji yok" ve rezerv sınırları testlerle
doğrulanır.

## Dilim 3 — Davranış ve sınırlı öğrenme (tamamlandı)

`src/life/ws_agent.*`: fayda (utility) tabanlı karar katmanı her adımda
ihtiyaçları puanlar, tehdidi kontrol eder ve açık bir davranış kümesinden birini
seçer: `rest`, `seek_food`, `seek_water`, `flee`, `wander`. Karar determinist ve
açıklanabilirdir.

- **Kaçma:** tehdit görüş mesafesine girerse kaçış yönü kilitlenir ve tehdit
  görüş alanından çıksa bile kısa süre (alarm kalıcılığı) devam eder.
- **Arama:** açlık/susuzluk eşiği aşılınca canlı görülen kaynağa yönelir;
  görülmüyorsa hafızadaki son verimli kaynak konumuna döner.
- **Keşif:** dürtüler düşükken ileri doğru küçük rastgele sapmalı gezinme.
- **Etkileşim:** erişim menzilinde ısırık/yudum kadar kaynak tüketilir (kaynak
  değeri düşer, dünya katmanı yeniler) ve organizma rezervi dolar.

**Hafıza ve sınırlı öğrenme:** kısa süreli olay halkası (yedi/on iki anlamlı
olay; tick, tür, konum, değer) ve son verimli besin/su konumunun uzamsal hafızası
tutulur. Öğrenme açıkken canlı, kaynağı görmediğinde hatırladığı verimli noktaya
döner. Kontrol grubu (hafıza kapalı) aynı koşulda kaynağı yeniden rastgele
aramak zorundadır. Kabul deneyi, aynı tohum kümesinde öğrenen canlının toplam
beslenme sayısında kontrolü geçmesini ve tohumların çoğunda kazanmasını ister;
tek bir tohum kabul için kullanılmaz.

## Dilim 4 — Sahne köprüsü ve golden scene (tamamlandı)

- `ws_env_scene` (`src/render/ws_env_scene.*`) yeni bir `organism_forage` sahnesi
  (scene_kind 5) kazanır: en düz kuru noktanın yakınına bir besin ve bir su
  kaynağı yerleştirilir; davranış ajanı ~18 s headless koşar; canlı seçtiği
  davranışa göre kaynağa yürür. Kamera, canlının **kararının onu götürdüğü**
  gerçek konuma ve üzerinde işlem yaptığı kaynağa bakar; görüntü komut edilmiş
  bir poz değil, kararın sonucudur. Canlı gövdesi P5 artiküle bedeniyle, kararı
  verilen yönelimde döndürülerek çizilir; kaynak bir renkli kutu ile işaretlenir.
- Katalog artık on iki sahne: `orbit`, `lowlight`, `spheres`, `materials`,
  `textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`,
  `terrain_physics`, `creature_walk`, `organism_forage`.
- Diğer on bir sahne byte-özdeş kalır (regresyon yok). `organism_forage` da
  dinamik artiküle simülasyon içerdiğinden, derleyiciler arası son-bit `libm`
  farkı nedeniyle `creature_walk` gibi belgelenmiş gevşek toleransla kapılanır.

## Model kartları ve kanıt düzeyi

`docs/life/` altında zorunlu model kartları vardır (`02-BILIMSEL-MODELLEME.md`):
algı, homeostaz, davranış ve sınırlı öğrenme. Kanıt düzeyleri kartlarda açıkça
kayıtlıdır; hiçbir model D1 üzerinden D3 diye tanıtılmaz. Öğrenme başarısı yalnız
çok tohumlu karşılaştırmayla iddia edilir.

## Otomatik doğrulama

- `ws_test_agent` (14 test): homeostaz ve rezerv korunumu (bedava enerji yok),
  yeme/içme doldurma, rezerv tükendiğinde ölüm, ihtiyaç önceliklendirme, algı
  görüş alanı/menzil, tükenmiş kaynağı atlama, tehditten kaçış, açlıkta besin
  arayışı, susuzlukta su arayışı, dinlenmede yorgunluk geri kazanımı, davranış
  determinizmi (kamera bağımsızlığı), çok tohumlu öğrenme-kontrol kıyası,
  geçersiz girdi reddi.
- `ws_test_world_env` içine eklenen test, `organism_forage` sahnesinin aynı
  seed ile aynı kararı ve gövde konumunu ürettiğini doğrular.
- Golden harness on iki sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` on iki sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.
- Yerel kanıt (gcc/clang, Debug+ASan/UBSan ve Release): sekiz test paketi geçer;
  ASan/UBSan temiz.

## Bilinen sınırlamalar

- Algı kabadır: menzil + görüş alanı + "en yakın" seçimi. Retina çözünürlüğü,
  görüş engeli/oklüzyon, işitme ve koku henüz yoktur.
- Homeostaz doğrusal yaklaşımdır; tam biyoenergetik model (oksijen borcu, ısı
  üretimi, sindirim) sonraki aşamalardadır. Hızlar yerel ayarlıdır, ölçülmemiştir.
- Öğrenme küçüktür: uzamsal hafıza + olay hafızası. Pekiştirmeli öğrenme, unutma
  eğrisi ve bellek bütçesi ölçümü sonraki dilimlere bırakılmıştır.
- Davranış kümesi sınırlıdır: dinlenme, arama, kaçma, gezinme. Sosyal tanıma,
  eşleşme, yavru bakımı ve üreme P7 kapsamındadır.
- Canlı yolu kinematiktir; sahne köprüsü P5 sagittal gövdesini kararı verilen
  noktaya/yönelime yerleştirir. Yol gövdesinin P5 fizik bedenine tam bağlanması
  (gövde yolu takip eder) sonraki dilime bırakılmıştır.
- Kamera etkisizliği testi, aynı seed ve aynı girdiyle davranışın birebir
  tekrarlanmasını doğrular; render katmanı davranış durumuna yazmaz.
