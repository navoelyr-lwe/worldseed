# P3 — İlk doğal çevre

Bu belge `13-YOL-HARITASI.md` P3 aşamasının durumunu ve kanıtlarını kaydeder.
P3, dünya/çevre katmanlarını (arazi, su, habitat, bitki örtüsü, atmosfer)
üretir ve bunları CPU referans renderer'ına bağlar. P3 hâlâ canlı bir simülasyon
değildir; fizik, canlılar ve etkileşim sonraki aşamalardadır.

## Dilim 1 — Dünya ve çevre katmanları (tamamlandı)

Tüm katmanlar kendi ISO C17 kodumuzdur; hiçbir üçüncü taraf kitaplık, motor veya
hazır varlık paketi kullanılmaz. Ortak deterministik value-noise kaynağı
(`src/core/ws_noise.c`) arazi, su, bitki ve çevre alanlarını besler; aynı seed ve
koordinat her platformda aynı sonucu verir, komşu bölgeler sınırda birleşir.

### Gürültü (`src/core/ws_noise.*`)
- `ws_noise_hash2`: tamsayı kafes hash'i (yalnız tamsayı, platformdan bağımsız).
- `ws_noise_value2`, `ws_noise_fbm2`, `ws_noise_ridged2`: düzgün value noise,
  fraktal Brown hareketi ve sırtlı multifraktal; hepsi [0,1) aralığında.

### Arazi (`src/world/ws_terrain.*`)
- Yükseklik saf bir fonksiyondur: `base_height` + kıtasal fbm + tepe fbm +
  maskeli sırtlı dağ terimi. Böylece bölge dikişi otomatik sağlanır.
- Hücre başına eğim (merkezî farklar) ve yüzey sınıfı (çayır, toprak, kaya,
  kum) sınıflandırması.
- `ws_terrain_height_bilinear` ile nokta yüksekliği; floating-origin
  (region-local) koordinatlar.

### Su (`src/world/ws_hydro.*`)
- Arazi çukurluklarını dolduran akış birikimi, eşik üstünde akarsu ağı.
- `is_river` maskesi, hücre su derinliği, akarsu yüzey yüksekliği.
- `ws_hydro_budget_step`: yağış girişi ve buharlaşma/akış çıkışıyla su bütçesi
  adımı; toplam su kütlesi uzun koşuda sonlu ve negatif olmayan kalır.

### Habitat (`src/world/ws_habitat.*`)
- Yükseklik, eğim, su yakınlığı ve arazi sınıfından habitat haritası:
  orman, çayır/açıklık, tatlı su kenarı, kayalık/yamaç.

### Bitki örtüsü (`src/world/ws_vegetation.*`)
- Habitat ve deterministik hücre hash'ine göre işlev grubu yerleşimi
  (çim, çalı, ağaç). Yerleşim yineleme sırasından bağımsızdır.

### Atmosfer (`src/world/ws_environment.*`)
- Düşük-hassasiyetli güneş konumu modeli (deklinasyon + saat açısı) ve
  gün ışığı ölçüsü.
- Sıcaklık: mevsimsel terim + günlük salınım + yükseklikle lapse oranı.
- Rüzgâr, bulut/nem, yağış ve sis alanları; hepsi deterministik gürültüden.
- Görsel gökyüzü rengi, sis yoğunluğu ve ışık şiddeti bu durumdan türetilir;
  rastgele post-process değildir.

## Dilim 2 — Sahne köprüsü ve paylaşılan katalog (tamamlandı)

- `ws_env_scene` (`src/render/ws_env_scene.*`): dünya katmanlarını renderer'ın
  sahne sözleşmesine çevirir. Arazi tek dokulu mesh (hücre başına yüzey sınıfı
  + ince deterministik renk değişimi), su ikinci mesh (simülasyon yüzey
  yüksekliğinde), ağaçlar örneklenmiş koni meshleri. Kamera, ışık, gökyüzü ve
  sis çevre durumundan gelir.
- `ws_catalog` (`src/render/ws_catalog.*`): sabit showcase sahneleri ile
  prosedürel çevre sahnelerini tek sıralı listede birleştirir. Golden harness
  (`ws_golden`) ve Windows uygulaması bu kataloğu paylaşır; referans karelerle
  uygulama çıktısı birbirinden kopamaz.
- Çevre sahneleri: `terrain_day` (gündüz), `terrain_dusk` (alacakaranlık; güneş
  ufka yakın zaman çevre modelinden seçilir), `terrain_weather` (gündüz
  kapalı/yağışlı; en yüksek yağış+sis, gün ışığında).

## Golden scene genişletmesi

Katalog artık dokuz sahne: `orbit`, `lowlight`, `spheres`, `materials`,
`textured`, `sky`, `terrain_day`, `terrain_dusk`, `terrain_weather`.
`tests/golden/manifest.csv` çevre sahnelerinin de parmak izini ve teknik
ölçütlerini kaydeder.

## Otomatik doğrulama

- `ws_test_world_env` (15 test): gürültü tekrarlanabilirliği, arazi
  tekrarlanabilirliği ve bölge dikişi, seed farkı, akarsu varlığı, su bütçesi
  kütle dengesi, habitat kapsaması, bitki determinizmi, gün/gece, sıcaklık
  lapse oranı, çevre determinizmi, çevre sahnesi kurulumu/tekrarı/uzun koşu
  kararlılığı ve tüm katalog sahnelerinin temiz kurulumu.
- Golden harness dokuz sahneyi üretir; aynı girdi tekrarında kare parmak izi
  değişmez; üretilen kareler commit'lenmiş referansla tolerans içinde eşleşir.
- Uygulama `--selftest` dokuz sahneyi geçer; uygulamanın 320×240 kareleri aynı
  koşudaki golden harness çıktısıyla byte-özdeş olur.
- Yerel kanıt (gcc ve clang Release): dokuz sahnenin kareleri byte-özdeş;
  `ws_test_world_env` 15/15 geçer; ASan/UBSan temiz.

## Bilinen sınırlamalar

- Bu aşamada dünya statiktir: arazi, su ve bitki örtüsü tohumdan üretilir ama
  zamanla değişmez. Su bütçesi adımı vardır, ancak tam akış/taşma dinamiği
  sonraki aşamalardadır.
- Ağaçlar tek koni silüetidir; tür bazlı gövde/dal geometrisi ve mevsimsel
  durum P7 ve sonrasındadır.
- Gökyüzü tek gradyan + sisi; bulut/atmosfer saçılımı yoktur.
- Bitki yerleşimi hücre bazlıdır; büyüme ve kaynak alımı P7'dedir.
- Görsel kalite protokolü (`08` belgesi) ayrıdır; bu aşama estetik iddia etmez.
