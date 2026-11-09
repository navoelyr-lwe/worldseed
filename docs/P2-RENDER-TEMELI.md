# P2 — Render ve içerik temeli

Bu belge `13-YOL-HARITASI.md` P2 aşamasının durumunu ve kanıtlarını kaydeder.
P2 hâlâ kullanıcıya görünür tam bir ürün değildir ve görsel kalite iddiası
taşımaz; buradaki renderer zorunlu doğruluk referansı ve CPU fallback yoludur.

## Dilim 1 — Sahne renderer, varlık hattı, golden scene (tamamlandı)

- Sözleşme: `ws_scene` (instanced mesh listesi + kamera + tek yönlü ışık),
  `ws_mesh` prosedürel küp/düzlem/uv-küre üretimi.
- Referans CPU renderer (`src/render/ws_render.c`): model/view/projection,
  yakın düzlem kırpma, perspektif-doğru interpolasyon, karo tabanlı tarama,
  derinlik tamponu, Lambert aydınlatma.
- Sürümlü mesh varlık formatı (`wsmesh`) + yapısal doğrulayıcı ve yükleyici.
- Araçlar: `ws_golden`, `ws_imgdiff`, `ws_assetgen`.
- Golden scene: `orbit`, `lowlight`, `spheres`.

## Dilim 2 — Materyal, aydınlatma, doku (bu dilim)

Bu dilim malzeme/ışık/doku temelini ekler ve golden-scene kanıtını genişletir.

### Matematik
- `ws_vec4` (make/add/sub/scale/dot/lerp), yön vektörü dönüşümü
  (`ws_mat4_mul_vec4_dir`).
- `ws_mat3` (sütun-büyük) ve normal matrisi (`ws_mat4_normal_matrix`),
  düzgün olmayan ölçekte bile doğru normal dönüşümü.

### Malzeme ve aydınlatma
- `ws_material`: doğrusal albedo, roughness, metallic, specular ölçeği ve
  isteğe bağlı albedo dokusu. `ws_material_make` ile güvenli kurulum.
- `ws_light`: yönlü ışık (renk + şiddet), yarım-küre ambient (gökyüzü/zemin
  renkleri) ve gradyan gökyüzü (zenit/ufuk).
- Per-piksel gölgeleme: Lambert diffuse + Blinn-Phong specular + yarım-küre
  ambient. Yüzey öznitelikleri (dünya konumu, dünya normali, albedo) perspektif
  düzeltmeli olarak enterpole edilir.
- Tüm gölgeleme doğrusal uzayda yapılır; çıkışta sRGB kodlama (IEC 61966-2-1).

### Render ayarları
- `ws_render_settings`: pozlama, ton eşleme (0 = kırpma, 1 = Reinhard),
  gökyüzü aç/kapa ve üstel mesafe sisi.
- Gökyüzü: kamera ışınına göre dikey gradyan (ufuk → zenit).
- Sis: derinliğe bağlı üstel zayıflama; gökyüzü rengine (veya arka plan rengine)
  doğru karıştırma.

### Doku
- `ws_texture`: RGBA8, sRGB→doğrusal dönüşüm, bilinear örnekleme, negatif
  kafes koordinatları için güvenli sarma.
- Üreticiler: `ws_texture_make_checker`, `ws_texture_make_noise` (deterministik
  value-noise, seed'li).
- Sürümlü doku varlık formatı (`wstex`) + okuyucu/yazıcı; bozuk ve desteklenmeyen
  sürümler reddedilir.

### Golden scene genişletmesi
Altı sahne: `orbit`, `lowlight`, `spheres`, `materials`, `textured`, `sky`.
`materials` roughness/metallic süpürmesini, `textured` doku örnekleyicisini,
`sky` gradyan gökyüzü + yarım-küre ambient + sis + filmik ton eşlemeyi
çalıştırır. Manifest `tests/golden/manifest.csv` şu alanları kaydeder:

`scene,seed,width,height,triangles_submitted,triangles_drawn,pixels_shaded,coverage,mean_luma,max_luma,render_ms,sha256`

## Dilim 3 — Uygulama ve kullanıcıya görünür önizleme (bu dilim)

Bu dilim, render temelini kullanıcıya görünür çalışan bir uygulamaya bağlar.

### Paylaşılan sahne modülü
- `ws_showcase` (`src/render/ws_showcase.h/.c`): altı referans sahneyi ve
  bunların orijinal varlıklarını (küp/düzlem/küre meshleri, checker/noise
  dokuları) tek bir kaynakta tanımlar. Golden harness (`ws_golden`) ve Windows
  uygulaması bu modülü paylaşır; böylece referans karelerle uygulama çıktısı
  birbirinden kopamaz.
- `ws_golden` bu modüle taşındı; kare çıktıları taşımadan önce ve sonra
  byte-özdeş kaldı (regresyon denetimi).

### Windows uygulaması
- `src/app/worldseed` (hedef adı `worldseed`): `--list`, `--selftest`,
  `--capture <dosya>`, `--scene N`, `--seed N`, `--size WxH`, `--headless`
  seçenekleri.
- Windows'ta gerçek bir Win32 penceresi açılır ve CPU render çıktısı GDI ile
  sunulur (yalnız pencere/girdi ve hazır piksel tamponu sunumu; GDI render
  motoru değildir). Pencere yeniden boyutlandığında sahne yeni çözünürlükte
  yeniden render edilir. Diğer platformlarda ve `--headless`/`--capture`/
  `--selftest` ile pencere açılmaz; CI gerçek kare kanıtı üretir.
- `--selftest` altı sahneyi render eder, her karenin sinyal taşıdığını
  (mean_luma, max_luma) denetler ve `APP_SELFTEST_OK` yazar.

### Uygulama kanıtı
- CI, uygulamayı headless çalıştırır, `--selftest` geçer ve uygulamanın
  320×240 `--capture` çıktısı sürümlenmiş golden referansla byte-özdeş olur.
  Bu, uygulama ile golden harness'ın aynı sahne/renderer yolunu paylaştığını
  kanıtlar.
- Release iş akışı her sahneyi 1280×720 yakalar, `tools/ppm2png.ps1` ile PNG'ye
  çevirir ve pakete (`preview/`) ve release eklerine koyar; bunlar proje
  sahibinin görebileceği gerçek render kanıtıdır.
- İzole Windows runner iş akışı (`gui-windows.yml`) gerçek pencereyi açar,
  masaüstü ekran görüntüsünü (`window.png`) alır ve kanıt olarak yükler. Bu
  iş akışı yalnız self-hosted izole runner kayıtlı olduğunda çalışır; runner
  yokken hiçbir GUI testi "çalıştı" olarak raporlanmaz.

## Otomatik doğrulama

CI (`.github/workflows/ci.yml`) Linux (gcc/clang, Debug/Release) ve Windows
(MSVC, Debug/Release) üzerinde şunları koşar:

- Tüm birim testleri (`ws_test_render` dahil 31 render testi).
- Golden scene: `WS_GOLDEN_OK`; aynı girdi tekrarında kare parmak izi değişmez.
- Golden referans karşılaştırması: üretilen kare sürümlenmiş referansla
  tolerans içinde eşleşir (`ws_imgdiff`).
- Varlık hattı: mesh ve doku varlıkları yeniden üretilebilir ve commit'lenmiş
  kayıtla byte-özdeş; provenance manifesti (`ASSET-MANIFEST.csv`) kaynak/lisans
  alanlarını taşır.
- Uygulama kanıtı: `worldseed --selftest` geçer ve uygulamanın 320×240 kareleri
  golden referansla byte-özdeş.
- Windows MSVC'de golden kare parmak izi determinizmi ayrıca denetlenir.

Yerel kanıt (gcc ve clang Release): altı sahnenin kare parmak izleri birebir
aynı; `ws_test_render` 31/31 geçer; sızıntı yok (ASan/LSan).

## Bilinen sınırlamalar

- Uygulama şimdilik tek bir sabit sahneyi gösteren bir görüntüleyicidir: kamera
  hareketi, sahne değiştirme, ayarlar ve tam ekran sonraki aşamalardadır.
- Bu renderer gölge, yansıma, ortam kapanımı (AO), normal/roughness haritası ve
  gerçek PBR enerji korunumu içermez. Nihai yüksek kaliteli renderer değildir.
- Doku yalnızca albedo içindir; filtrelenmiş mipmap yoktur.
- Gökyüzü tek renk gradyanıdır; bulut/atmosfer saçılımı yoktur.
- Sahne değişimi başlangıç argümanıyladır; çalışma sırasında etkileşim yoktur.
- Görsel kalite protokolü (`08` belgesi) ayrıdır; bu dilim estetik iddia etmez.
