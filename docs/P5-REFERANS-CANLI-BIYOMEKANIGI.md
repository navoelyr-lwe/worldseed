# P5 — Referans canlı biyomekaniği

Bu belge `13-YOL-HARITASI.md` P5 aşamasının durumunu ve kanıtlarını kaydeder.
P5, ilk referans türün (kızıl geyik, *Cervus elaphus*) görünür hareketini hazır
animasyondan değil, kendi ISO C17 fiziksel beden + eklem + kas-eşdeğeri aktüatör
+ hareket kontrol modelinden üretir (`06-CANLI-BIYOMEKANIK.md`). Üçüncü taraf
motor, hazır iskelet, hazır animasyon klibi veya hazır hareket verisi kullanılmaz.

## Dilim 1 — Artiküle beden ve yarı-kapalı (symplectic) çözücü (tamamlandı)

- `src/life/ws_creature.*`: gövde zinciri (torso, boyun, baş, ikişer segmentli
  ön/arka bacaklar), her gövde için kütle/atalet/yarı-ölçü, eklemler (açı
  sınırı, rest açısı, motor kp/kd ve tork bütçesi) ve ayak temas sensörleri.
  Çözücü: yarı-kapalı Euler + sıralı impuls (sequential impulse) temas ve eklem
  çözümü, Baumgarte konum düzeltmesi. Durum çağıran tarafından sahiplenilen
  dizilerde tutulur; sıcak yolda tahsis yok.
- Model düzlemseldir (sagittal): sağ/sol bacaklar ayrı dinamik değildir. Bu,
  dilimin belgelenmiş kapsamıdır; lateral denge, yaw torku ve dönüş dinamiği
  sonraki dilimlere bırakılmıştır.
- Tüm geometri, kütle ve atalet tür profilinden (`ws_species_profile`)
  türetilir; hiçbir dış varlık veya klip kullanılmaz.

## Dilim 2 — Kapalı-çevrim yürüyüş kontrolcüsü (tamamlandı)

`src/life/ws_quadruped.c` içinde Raibert tarzı ayak yerleştirmeli kapalı-çevrim
yürüyüş kontrolcüsü vardır. Denetim tasarımı (nihai):

- **Nötr nokta (feed-forward):** `x_td = hip_x + 0.5·v_des·T_st`. İstenen hız
  beslemesi ile ayak, gövdenin önüne yerleştirilir.
- **Hız hatası düzeltmesi:** `+ kv·(v_des − v_actual)`. Hız yavaşsa ayak daha
  ileri, hızlıysa daha geri konur (negatif geri besleme; işaret hatası
  düzeltilmiştir).
- **Stance ayak geri çekilmesi:** duruş fazında ayak gövde-göreli olarak sürüş
  hızında geriye çekilir; ayak yalnız "çok hızlı" durumunda öne dikilir.
- **Gövde yüksekliği referansı:** `ground_hip + p->shoulder_h` (gerçek hip
  yüksekliği değil), böylece çömelme geri besleme döngüsü oluşmaz.
- Faz durumu yalnız tohumlanır (`phase=0; stance=0; initialised=1`); çağıranın
  ayarladığı kazançlar/kaldırma ezilmez.

Sonuçlar: düz zeminde `v ≈ 0,8 m/s`, gövde dik (torso ~0,93–1,0 m), %3 eğimde
tırmanır; düz, eğimli ve hafif engebeli zeminde kararlıdır.

`ws_gait_state`: `phase`, `td_x[2]`, `lo_x[2]`, `stance[2]`, `foot_lift`, `kv`,
`heading`, `yaw_rate`, `initialised`. `ws_quadruped_walk_steer` yol başlığını
yaw komutuyla ilerletir; yaw şu an kinematiktir (düzlemsel dilim bacak
kuvvetlerinden yaw torku üretmez), bu sınır model kartında kayıtlıdır.

## Dilim 3 — Fizyoloji bağlantısı: hareket maliyeti (tamamlandı, kısmi)

`ws_creature` motor mekanik işini biriktirir (`motor_work`, J) ve anlık motor
gücünü tutar (`motor_power`, W). Kas enerji kullanımı, standart pozitif-iş
maliyet vekili olarak motor işinin mutlak değeriyle yaklaştırılır. Bu, metabolik
enerji modelinin ilk bağlantısıdır; tam fizyoloji (açlık, yorgunluk, yaralanma,
sıcaklık, su) sonraki aşamalardadır. Model kartı sınırı kaydeder.

## Dilim 4 — Sahne köprüsü ve golden scene (tamamlandı)

- `ws_env_scene` (`src/render/ws_env_scene.*`) yeni bir `creature_walk` sahnesi
  (scene_kind 4) kazanır: en düz kuru iç nokta seçilir, referans geyik oraya
  yerleştirilir ve P5 kontrolcüsüyle headless olarak ~12 s yürütülür. Kamera,
  simülasyonun bıraktığı gerçek gövde konumuna yandan bakar; yani görüntü komut
  edilmiş bir poz değil, simülasyonun sonucudur. Gövde segmentleri yönlü
  kutularla çizilir; bacaklar sol/sağ kopyalanır (dinamik düzlemseldir).
- Katalog artık on bir sahne: `orbit`, `lowlight`, `spheres`, `materials`,
  `textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`,
  `terrain_physics`, `creature_walk`. `tests/golden/manifest.csv` yeni sahnenin
  parmak izini ve teknik ölçütlerini kaydeder.
- Diğer on sahne byte-özdeş kalır (regresyon yok).
- Derleyiciler arası (gcc/clang/MSVC) golden karşılaştırması: on statik sahne
  tüm derleyicilerde piksel-özdeş kalır. `creature_walk` dinamik bir katı cisim
  simülasyonu olduğu için, MSVC CRT ile glibc'nin son-bit `libm` (sin/cos/atan2/
  acos) farkları uzun koşuda yarım piksel mertebesinde bir poz kaymasına büyür.
  Kare yine aynı sahneyi gösterir (aynı geometri, örtü, ışık); bu yüzden statik
  sahneler sıkı toleransla (mean≤3, ssim≥0.99), `creature_walk` ise belgelenmiş
  daha gevşek toleransla (ssim≥0.80) kapılanır. Eksik veya belirgin yanlış
  yerleşmiş canlı bu kapıyı yine de geçemez.

## Model kartları ve kanıt düzeyi

`docs/life/` altında zorunlu model kartları vardır (`02-BILIMSEL-MODELLEME.md`):
referans tür ve anatomi, yürüyüş kontrolcüsü, hareket maliyeti. Kanıt düzeyleri
kartlarda açıkça kayıtlıdır; anatomik oranlar ve yürüyüş parametreleri
literatüre dayandırılmıştır ama D3 (yayımlanmış ölçümle nicel kıyas) henüz
tamamlanmamıştır ve kartlar bunu belirtir.

## Otomatik doğrulama

- `ws_test_life` (13 test): referans beden kurulumu, dengeli duruş, kütle merkezi
  destek çokgeni içinde, ileri yürüyüş kararlılığı, eğimde duruş, eğimde
  yürüyüş, engebeli zeminde yürüyüş, dönüş/yönlendirme, enerji maliyeti, eklem
  sınırlarına uyum, determinizm, uzun koşuda taşma yok, geçersiz girdi reddi.
- `ws_test_world_env` içine eklenen test, `creature_walk` sahnesinin aynı seed
  ile aynı gövde konumunu ürettiğini doğrular.
- Golden harness on bir sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` on bir sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.
- Yerel kanıt (gcc/clang, Debug+ASan/UBSan ve Release): yedi test paketi geçer;
  ASan/UBSan temiz.

## Bilinen sınırlamalar

- Model düzlemseldir; sağ/sol bacaklar, lateral denge ve yaw torku yoktur.
  Dönüş yalnız yol-başlığı (kinematik) düzeyindedir.
- Kas modeli kuvvet-uzunluk/hız ilişkisini basitleştirir; tendon/elastisite ve
  yumuşak doku yoktur.
- Hareket maliyeti motor işinin mutlak değeriyle yaklaştırılır; tam metabolik
  model (oksijen borcu, ısı, yorgunluk) sonraki aşamalardadır.
- Zemin profili yürüyüş hattı boyunca örneklenir; sahne köprüsü en düz noktayı
  seçer, dik/engebeli arazi kasıtlı olarak kaçınılır.
- Eğimde duruşta ayak yüksekliği küçük bir sınır çevrimi (≈1 cm, yavaş) yapar:
  temas çözücüsü ile konum düzeltmesi arasındaki dönüşümlü etkileşimden kaynaklanır.
  Canlı dengede kalır ve ayak sürekli destek içindedir; sınır çevrimi giderilmesi
  sonraki dilime (temas izleme/slop ayarı) bırakılmıştır.
- Anatomik oran ve yürüyüş parametreleri literatür bilgisine dayanır ama D3
  nicel kıyası yapılmamıştır.
- Görsel kalite protokolü (`08`) ayrıdır; bu aşama estetik iddia etmez.
