# P2 — Render ve içerik temeli (durum ve kanıt)

Bu belge `13-YOL-HARITASI.md` P2 aşamasının ilk dilimini kaydeder: 3B sahne sözleşmesi, referans CPU sahne renderer'ı, prosedürel mesh üretimi, sürümlü mesh varlık formatı + doğrulayıcı, görüntü metrikleri ve otomatik golden-scene kanıtı. Bu dilim hâlâ kullanıcıya görünür tam bir ürün değildir; görsel kalite iddiası taşımaz.

## P2 (bu dilim) çıktıları

- Matematik: `ws_vec4`, `ws_mat4` (çarpım, look-at, perspektif), vektör çarpım/normalize/lerp.
- Mesh: `ws_vertex` (pozisyon, normal, UV, renk) ve prosedürel küp/düzlem/küre üreticileri.
- Varlık hattı: sürümlü metin mesh formatı (`worldseed-mesh 1`), tarayıcı/yükleyici/yazıcı ve yapısal doğrulayıcı.
- Sahne sözleşmesi: kamera, yönlü ışık, malzeme, örnek (instance) listesi (`ws_scene`).
- Renderer: `ws_render_scene` — MVP dönüşümü, yakın-düzlem kırpma, perspektif bölme, karo tabanlı tarama, perspektif-doğru nitelik interpolasyonu ve Lambert aydınlatma.
- Görüntü: PPM okuma/yazma, karşılaştırma metrikleri (ortalama/tepe fark, PSNR, SSIM), kapsam (coverage) ölçümü.
- Araçlar: `ws_golden` (golden scene üretimi + manifest) ve `ws_imgdiff` (toleranslı karşılaştırma).

## Sahne sözleşmesi (mimari sınır)

- Dünya/simülasyon katmanı renderer'a doğrudan bağlanmaz; yalnız `ws_scene` snapshot'ı üretir.
- Renderer dünya durumunu değiştiremez; sahne salt-okunurdur.
- Aynı sahne CPU ve (gelecekte) GPU backend'lerine verilebilir; beklenen eşitlik geometri/semantik düzeyinde ve tanımlı toleransta, piksel-bit eşitliği değil.

## Golden scene seti (bu dilim)

| Sahne | İçerik | Metrik |
|---|---|---|
| `orbit` | küp + küre + zemin, 3/4 kamera | üçgen, kapsam, SHA-256 |
| `lowlight` | tek küp, düşük yoğunluk/ortam ışığı | üçgen, kapsam, SHA-256 |
| `spheres` | üç küre + zemin | üçgen, kapsam, SHA-256 |

Her sahne `tests/golden/manifest.csv` içinde çözünürlük, seed, üçgen sayısı, kapsam ve kare SHA-256 değerini kaydeder. CI hem aynı girdi tekrarında kare parmak izinin değişmediğini hem de üretilen karenin sürümlenmiş referansla tolerans içinde eşleştiğini denetler.

## Kabul kapıları ve kanıtlar

- Birim/entegrasyon testleri: `tests/test_render.c` (matematik, mesh, varlık, renderer, görüntü) — CI `render` suite.
- Karo boyutu değişmezliği: aynı sahne 8 ve 64 karo için bit düzeyinde aynı çıktı (unit test).
- Derinlik sıralaması: yakın nesne uzak nesneyi örter (unit test).
- Determinizm: aynı sahne iki kez bit düzeyinde aynı; GCC/Clang ve Debug/Release arasında aynı kare SHA-256 (yerel kanıt).
- Golden-scene: `ws_golden` `WS_GOLDEN_OK` üretir; manifest kare parmak izlerini yazar.
- Toleranslı görsel regresyon: `ws_imgdiff` referans kareye karşı ortalama fark ≤ 3.0 ve SSIM ≥ 0.99 kapısını uygular.
- Platform: Linux GCC/Clang (Debug ASan/UBSan + Release) ve Windows MSVC (Debug/Release) aynı adımları çalıştırır.

## Bilinen sınırlar (sonraki P2 dilimleri)

- Yalnız Lambert aydınlatma; PBR/HDR, gölge haritası, atmosfer, su, doku ve bitki LOD henüz yok.
- Mesh üreticileri düz renk ve tek malzeme kullanır; UV var ama doku örnekleme yok.
- Perspektif bölme sonrası yalnız yakın-düzlem kırpma yapılır; yan düzlem kırpma henüz yok (tam ekran dışı üçgenler tarama aşamasında kırpılır).
- Golden scene seti `08-CIZICI-VE-VARLIK-HATTI.md` hedef listesinin ilk üç sahnesidir; orman, su, yağmur/sis, gece ve motion blur sonraki dilimlerdedir.
- Performans iddiası yok; p95 frame-time ölçümü gerçek renderer ile sonraki dilimde yapılır.
