# P9 — Sinematik yönetmen ve ses

Bu belge `13-YOL-HARITASI.md` P9 aşamasının durumunu ve kanıtlarını kaydeder.
P9, `09-SINEMATIK-YONETMEN.md`'nin istediği gözlemciyi ve `10-SES-KALICILIK-TEKRAR.md`'nin
istediği kendi C17 ses üretimini ekler: gerçek olay akışını tüketen, dünyayı
değiştirmeyen bir yönetmen; olay-tabanlı çekim planlama ve kamera kontrolü; ve
gerçek olaylarla tetiklenen prosedürel bir ses manzarası. Hiçbir katman dünyaya
yazmaz; yönetmen ve ses yalnız gözlemcidir. Üçüncü taraf ses/yönetmen/kamera
motoru kullanılmaz; tüm kod ISO C17'dir ve yalnız C17 standart kütüphanesine
(`math.h`, `string.h`, `stdio.h`) dayanır.

## Dilim 1 — Yaşam olayları: yönetmenin tek girdisi (tamamlandı)

`src/life/ws_life_event.*`: simülasyonun gerçekten ürettiği ayrık, gözlenebilir
olayları taşır. Yalnız P7 ekolojisinin gerçekten ürettiği dört tür vardır:
otlama (`ATE`), doğum (`BORN`), ölüm (`DIED`) ve av (`KILLED`). Olay, paylaşılan
`WS_EVENT_LIFE` tipinde (`src/world/ws_event.h`) sabit küçük-endian alanlarla
kodlanır (tür, aktör, konu, x, y, değer, tick). Böylece olay hem journal'a yazılır
hem deterministik replay'e uyar; işaretçi taşımaz.

- **Uydurma yok:** Ekolojide su katmanı olmadığı için "su içme" olayı yoktur ve
  yönetmen onu uydurmaz (`docs/09` "olay yaratmaz").
- **Sıralı akış:** Her olay artan `sequence` taşır; gözlemci eski olayları yok sayar.

## Dilim 2 — Yönetmen: olay keşfi, planlama, kamera (tamamlandı)

`src/cinema/ws_director.*`: dünyanın otoritesi değildir. `ws_director_observe`
yalnız `WS_EVENT_LIFE` kabul eder; başka tip `WS_ERR_INVALID_ARGUMENT` döner.
Yönetmenin dünyaya yazan tek bir çağrısı yoktur (derleme/mimari testiyle
doğrulanır: yönetmen açık/kapalı aynı koşu bit-özdeş olay akışı üretir).

Katmanlar (`docs/09` "Yönetmen katmanları"):

1. **Olay keşfi:** yeni olayları olay günlüğünden okur.
2. **Önceliklendirme:** olay türüne göre önem puanı (av 1.0, ölüm 0.8, doğum
   0.6, otlama 0.35).
3. **Konu seçimi + süreklilik:** mevcut çekimi en az `min_shot_s` tutar
   (histerezis); yalnız `min_shot_s` sonrası ve yeni olay önemce `cut_cost`
   kadar üstünse keser; `max_shot_s` sonrası bayat çekimi yeniler. Böylece
   görüntü olaylar arasında strobe yapmaz.
4. **Çekim planlama:** olay türünden çekim tipi (av → takip, ölüm → yakın,
   doğum → orta, otlama → geniş). Azimut çekim tipinin sabit fonksiyonudur
   (kare başına jitter yok).
5. **Kamera kontrolü:** göz, planlanan görünüme hız (`max_speed`), ivme
   (`max_accel`) ve yaw dönüş hızı (`max_turn_rate`) sınırları içinde sürülür.
6. **Süreklilik ölçümü:** kesmeler arası hız/ivme/dönüş ve takip hatası
   (`ws_director_metrics`) ölçülür; P9 yumuşaklık testleri bunları kapılar.

Ek olarak:

- **Occlusion:** `ws_director_set_ground` ile bir yükseklik örnekleyici verilirse,
  göz ile konu arasındaki görüş çizgisi taranır ve göz engeli aşacak kadar
  yükseltilir. Yalnız kamera hareket eder; arazi/bitki kaldırılmaz (`docs/09`
  "fiziksel duvar/bitki kaldırılmaz").
- **Takip:** `ws_director_track` ile konunun güncel dünya konumu verilir; kamera
  donmuş olay noktası yerine hareket eden canlıyı izler. Bu salt okunurdur.
- **Serbest kamera:** `WS_DIRECTOR_FREE` modunda yönetmen kontrolü bırakır
  (kamera sabit kalır); dünya her iki durumda da etkilenmez.
- **Kesme (cut):** bilinçli çekim değişiminde göz sıçrar, hareket geçmişi
  sıfırlanır ve kesme sayılır; sıçrama "hareket" olarak ölçülmez.

## Dilim 3 — Ses manzarası: olay-tabanlı, deterministik (tamamlandı)

`src/audio/ws_audio.*`: kayıtlı ses veya üçüncü taraf ses çalışma zamanı yoktur.
Her ses kendi C17 kodumuzla üretilir: küçük sabit ses havuzunda (`WS_AUDIO_MAX_VOICES`)
sabit perdeli sinüs osilatörleri, atak/azalma zarfı, interleaved 16-bit PCM'e
48 kHz'de karıştırma.

- **Olay eşleme:** her gerçek olay türü kendi sesine eşlenir (av 180 Hz, ölüm
  120 Hz, doğum 660 Hz, otlama 320 Hz). Gerçek olay yoksa ses de yoktur.
- **Mekânsal sunum:** `pan`, olayın dünya x konumundan türetilir (−1 sol … +1 sağ).
- **Tür-başına bekleme:** aynı tür en fazla `kind_cooldown` penceresi kadar sık
  çalar; sürü otlaması sesi drone'a çevirmez, tekrarlar `events_skipped` sayılır.
- **Ana kazanç + yumuşak doygunluk:** `master` kazancı ve `x/(1+|x|)` yumuşak
  doygunluğu, çok ses üst üste geldiğinde sert kırpma (buzz) yerine yumuşak
  sınırlama sağlar. Gerçek WAV çıktısında kırpılmış örnek yoktur (test edilir).
- **Determinizm:** Aynı olay dizisi bit-özdeş PCM üretir; test PCM'i hash'ler.
- **Kanıt:** `tools/ws_audio` gerçek bir ekoloji koşusundan WAV yazar, sinyal
  taşıdığını ve tekrarlanabilirliğini (`sha256`) doğrular. CI bu WAV'ı artefakt
  olarak saklar.

## Dilim 4 — Sahne köprüsü (tamamlandı)

`src/render/ws_env_scene.*` iki yeni sahne kazanır:

- **`director_shot` (scene_kind 8):** ekoloji headless koşar; yönetmen gerçek
  olayları gözler, çekimi planlar ve kamerayı sürer. Seçilen hayvan, olayın
  gerçek konumunda kameraya dönük olarak çizilir. Görüntü yönetmenin kararıdır,
  script edilmiş poz değildir. Sahne, olay sayısı, kesme, hız/dönüş üst sınırı ve
  çekim açıklaması gibi koşu özetini taşır.
- **`soundscape` (scene_kind 9):** ekoloji koşar; her gerçek olay türü bir sesi
  tetikler. Karışım adım adım render edilip hash'lenir; her tür, koşuda kaç kez
  oluştuğuna göre yükselen bir gösterge çubuğu olarak çizilir. Görüntü, dünyanın
  gerçekten ürettiği ses manzarasıdır.

Katalog artık on altı sahne: `orbit`, `lowlight`, `spheres`, `materials`,
`textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`,
`terrain_physics`, `creature_walk`, `organism_forage`, `ecosystem`, `world_stream`,
`director_shot`, `soundscape`. Diğer on dört sahne byte-özdeş kalır (regresyon yok).

## Model kartları ve kanıt düzeyi

`docs/cinema/` ve `docs/audio/` altında P9 kartları vardır:
`MODEL-KARTI-YONETMEN.md` (olay-tabanlı yönetmen) ve `MODEL-KARTI-SES-MANZARASI.md`
(prosedürel ses manzarası). Kanıt düzeyi D2'dir (determinizm, sınır ve
davranış testleri geçti); nicel sinematografi/akustik kıyası yoktur.

## Otomatik doğrulama

- `ws_test_cinema` (10 test): yönetmenin dünya otoritesi olmadığı (yönetmenli ve
  yönetmensiz koşu bit-özdeş olay özeti); sahte olay üretmediği ve çekimin gerçek
  olay kimliğine bağlandığı; çekim önceliği ve histerezis; kamera hız/ivme/dönüş
  sınırları; kesmeler arası süreklilik (sıçrama yok); mod kontrolü ve serbest
  kamera; occlusion'ın yalnız kamerayı yükselttiği; sesin deterministik olduğu;
  sesin gerçek sinyal taşıdığı ve WAV başlığının geçerli olduğu; sesin gerçek
  ekoloji koşusundan sürüldüğü.
- `ws_test_world_env` içine eklenen iki test: `director_shot` sahnesinin aynı
  tohumla aynı çekimi, aynı konu pozunu ve aynı koşu özetini ürettiği;
  `soundscape` sahnesinin aynı tohumla aynı çubukları, aynı özeti ve aynı PCM
  hash'ini ürettiği.
- Golden harness on altı sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` on altı sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.
- `tools/ws_audio` gerçek bir ekoloji koşusundan WAV üretir; sinyal taşır ve
  tekrarlanabilir. Gerçek pencere ekran görüntüsü `director_shot` sahnesinden
  alınır (GUI workflow).

## Bilinen sınırlamalar

- Yönetmen tek konuyu izler; çok aktörlü ilişki kadrajı (ör. av-avcı ikilisini
  birlikte çerçeveleme) sonraki dilimdedir.
- Çekim geçişleri sert kesmedir; yumuşak geçiş (dissolve/geçiş eğrisi) henüz
  yoktur. Kamera hareketi sınırlı ve ölçülür, ancak planlar arası geçiş kamerayı
  ışınlamaz (uçmaz); kesme açıkça kullanılır.
- Ses osilatörleri temel sinüstür; enstrüman/gürültü tabanlı zengin ses bankası
  ve ortam akustiği (yankı/engel) sonraki dilimdedir.
- Occlusion tek bir yükseklik alanı örnekler; bitki/nesne örtüsü görüş testine
  dâhil değildir.
- Kullanıcı arayüzünde canlı serbest kamera etkileşimi (`docs/17`) sonraki
  ürün dilimindedir; bu dilimde mod API'si hazırdır.
