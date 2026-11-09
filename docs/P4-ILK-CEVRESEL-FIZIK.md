# P4 — Çevresel fizik

Bu belge `13-YOL-HARITASI.md` P4 aşamasının durumunu ve kanıtlarını kaydeder.
P4, kendi ISO C17 fizik çekirdeğimizi ekler: yerçekimi, rijit cisim
kinematiği/dinamiği, küre teması, sürtünme/restitüsyon ve çevresel kuvvetler
(rüzgâr sürüklemesi, kaldırma kuvveti). Fizik headless'tır ve render'dan
bağımsızdır (`05-FIZIK-MOTORU.md`); görüntü fizik durumundan üretilir ama fizik
doğruluğu render testine dayanmaz.

## Dilim 1 — Fizik çekirdeği (tamamlandı)

Tüm kod kendi ISO C17 kodumuzdur; üçüncü taraf fizik kütüphanesi veya motor
kullanılmaz. Modüller:

- `src/physics/ws_units.h`: SI birim ve sabitler (yerçekimi, hava/su
  yoğunluğu, sıcaklık dönüşümleri) tek yerde.
- `src/physics/ws_physics.*`: `ws_body` (küre), `ws_physics_world`, yarı-kapalı
  (symplectic) Euler entegratörü, yükseklik-alanı zemin teması, küre-küre teması,
  Coulomb sürtünmesi, restitüsyon, hava sürüklemesi ve kaldırma kuvveti.
- `src/physics/ws_wind.*`: çevre modelinden rüzgâr yönü/hızı; yükseklikle güç
  yasası profili (`u(z) = u_ref·(z/z_ref)^0.14`).

Tasarım kararları: sabit zaman adımı (varsayılan 1/200 s) ve sabit solver
iterasyonu; adım render FPS'den bağımsız (kullanıcı dünyayı yalnız tam adımlarla
ilerletir); durum çağıran tarafından sahiplenilen dizide tutulur, sıcak yolda
tahsis yok; `WS_PHYSICS_VERSION` entegratör/temas değişince artırılır.

## Dilim 2 — Sahne köprüsü ve golden scene (tamamlandı)

- `ws_env_scene` (`src/render/ws_env_scene.*`) yeni bir `terrain_physics` sahnesi
  (scene_kind 3) kazanır: en düz kuru iç nokta seçilir, bir küre kaya oraya
  bırakılır ve P4 çözücüsüyle headless olarak dinlenmeye bırakılır. Kamera,
  çözücünün bıraktığı gerçek dinlenme konumuna bakar; yani görüntü komut
  edilmiş bir poz değil, fiziğin sonucudur.
- Katalog artık on sahne: `orbit`, `lowlight`, `spheres`, `materials`,
  `textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`,
  `terrain_physics`. `tests/golden/manifest.csv` yeni sahnenin parmak izini ve
  teknik ölçütlerini kaydeder.
- Diğer dokuz sahne byte-özdeş kalır (regresyon yok).

## Model kartları ve kanıt düzeyi

`docs/physics/` altında zorunlu model kartları vardır
(`02-BILIMSEL-MODELLEME.md`): rijit cisim/entegratör, zemin teması/sürtünme/
restitüsyon, rüzgâr/hava sürüklemesi/kaldırma. Tümü **D2 (sayısal)** olarak
kayıtlıdır: analitik ve korunum testleri geçmiştir; D3 (yayımlanmış ölçüm/
referans model kıyası) henüz yapılmamıştır ve kartlarda bu açıkça belirtilir.

## Otomatik doğrulama

- `ws_test_physics` (11 test): analitik serbest düşüş, statik eğik düzlem, kayan
  eğik düzlem, momentum korunumu, restitüsyonda enerji kaybı, dinlenmede
  penetrasyon yok, adım boyu yakınsaması, kaldırma kuvveti, rüzgâr profili,
  geçersiz girdi reddi, kinetik enerji.
- `ws_test_world_env` içine eklenen test, `terrain_physics` sahnesinin aynı
  seed ile aynı dinlenme konumunu ürettiğini doğrular.
- Golden harness on sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` on sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.
- Yerel kanıt (gcc/clang, Debug+ASan/UBSan ve Release): altı test paketi
  geçer; ASan/UBSan temiz.

## Bilinen sınırlamalar

- Cisimler küre; dönme/açısal momentum yok, bu yüzden kayma yok, cisimler
  yuvarlanmaz yalnız ötelenir. Çok segmentli canlı bedenleri P5'tedir.
- Zemin teması yükseklik-alanı yaklaşımıdır; uçurum kenarı gibi süreksiz
  yüzeylerde normal yaklaşıktır.
- Kaldırma yalnız tam batmada; kısmi batma, dalga ve akıntı yok.
- Rüzgâr profili mühendislik güç yasasıdır; çözülmüş sınır tabakası değil.
- Bu aşamada fizik sahnesi tek bir dinlenme olayını gösterir; sürekli etkileşim,
  çarpışma kümeleri ve yumuşak doku sonraki aşamalardadır.
- Görsel kalite protokolü (`08`) ayrıdır; bu aşama estetik iddia etmez.
