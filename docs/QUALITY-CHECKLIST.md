# Ürün görsel kalite kontrol listesi ve referans kabul protokolü

Bu belge `docs/12-TEST-KABUL-CI.md`'nin "Görsel kanıt standardı" ve
`docs/08-CIZICI-VE-VARLIK-HATTI.md`'nin "Golden scene seti" bölümlerinin
gerektirdiği **sahne bazlı görsel kalite kontrol listesini** ve **referans kabul
protokolünü** tanımlar. Tek başına golden hash veya piksel farkı estetik kaliteyi
kanıtlamaz; bu nedenle altı ürün referans sahnesi hem teknik regresyon kapısından
(golden) hem de bu kontrol listesinden geçer.

## İki katmanlı görsel kabul

1. **Teknik regresyon (golden):** `tests/golden/*.ppm` referanslarına toleranslı
   karşılaştırma (`tools/ws_imgdiff`, mean abs diff + SSIM) geometri/doku/ışık
   kaymasını yakalar. Bkz. `ci.yml` "Golden scenes" ve "Golden reference
   comparison" adımları.
2. **Görsel kalite kontrol listesi (bu belge):** her referans sahnesi için
   ölçülebilir kalite sinyalleri ve eşikler. Otomatik kanıt `tools/ws_quality`
   ile üretilir; `ci.yml` "Product visual-quality checklist" adımı zorunlu kapı
   yapar. Golden hash tek başına bu kapıyı geçmez.

## Altı ürün referans sahnesi

Ürün sözleşmesi (`docs/00-URUN-SOZLESMESI.md` §5) ve araştırma yol haritası
(`docs/19-GERCEK-DURUM-VE-ARASTIRMA-YOL-HARITASI.md`) altı referans sahne
tanımlar. Bunlar gerçek katalog sahnelerine eşlenir; kalite kapısı bu eşlemeyi
kullanır:

| # | Ürün referans sahnesi | Katalog sahnesi | Neyi kanıtlar |
|---|---|---|---|
| 1 | Gündüz geniş orman/vadi/nehir planı | `terrain_day` | Geniş ölçek, arazi+su+bitki, gökyüzü gradyanı |
| 2 | Çayırda canlıların doğal davranışı | `ecosystem` | Çoklu tür bedenleri, habitat dokusu |
| 3 | Su kaynağında tür etkileşimi | `organism_forage` | Su kenarı, ajana bağlı gerçek konum |
| 4 | Yakın plan beden/hareket | `creature_walk` | Artiküle beden, yüzey yakınlığı |
| 5 | Yağmur/sis atmosferi | `terrain_weather` | Atmosferik derinlik, ışık saçılımı |
| 6 | Gece/düşük ışık | `terrain_dusk` | Gölge/kontrast, karanlık bant |

## Ölçülebilir kalite sinyalleri

`src/render/ws_quality.{h,c}` her karenin gerçek piksel tamponundan ve onu üreten
sahneden şu **deterministik** ölçüleri hesaplar:

- **Geometri:** `objects` (sahnedeki nesne sayısı), `geometry_coverage`
  (gölgelenen piksel / toplam piksel), `coverage` (arka plandan farklı piksel).
- **Işık/ton:** `mean_luma`, `max_luma`, `luma_stddev` (gölgeleme kontrastı),
  `dark_fraction`, `blown_fraction` (aşırı pozlama).
- **Materyal/renk:** `mean_saturation` (HSV doygunluk), `colour_variety` (4-bit
  kanal kuantalamasıyla ayrık renk sayısı).
- **Atmosfer:** `sky_gradient` (üst bant ile alt bant ortalama parlaklık farkı;
  gradyan gökyüzü düz arka plandan ayrılır).

## Kalite boyutları ve kanıt eşlemesi

`docs/13-YOL-HARITASI.md` P12 kabulü **geometri/malzeme/ışık/gölge/atmosfer/
hareket** kalite kontrol listesi ister. Her boyut nerede kapılandığı:

| Boyut | Ölçü / kapı | Nerede |
|---|---|---|
| Geometri | `objects`, `geometry_coverage` | `ws_quality` (bu belge) |
| Malzeme | `mean_saturation`, `colour_variety` | `ws_quality` |
| Işık | `mean_luma`, `max_luma`, `blown_fraction` | `ws_quality` |
| Gölge | `luma_stddev`, `dark_fraction` + `shadows` golden sahnesi | `ws_quality` + `ws_golden` |
| Atmosfer | `sky_gradient` + sis/sky sahneleri | `ws_quality` + `ws_golden` |
| Hareket | kare-arası değişim (canlı tur, deterministik tur adımları) + biyomekanik | `worldseed --live-tour` + `tests/test_life.c` |

Hareket boyutu tek karede ölçülemez; bu yüzden altı ürün referans sahnesi
(durağan kareler) bu boyutu kapsamaz. Hareket, canlı turun kare-arası adımları
(`step-*` kareleri bit-özdeş tekrar eder) ve P5/P7 biyomekanik/ekoloji testleriyle
ayrı kapılanır. Bu kontrol listesi altı durağan referans sahnenin geometri,
malzeme, ışık, gölge ve atmosfer boyutlarını otomatik ölçer.

## Sahne başına eşikler ve gerekçe

Eşikler sabit referans karelerden kalibre edilmiştir ve **ihtiyatlı**dır: eksik,
ışıksız veya yıkanmış bir kareyi eler; meşru bir tasarım değişikliğini elemez.
Gerçek ölçümler (2026-11-09, 640×360, seed 20260101) parantez içinde:

| Sahne | geom_cov | coverage | stddev | variety | sat | sky_grad | objects | max_blown |
|---|---|---|---|---|---|---|---|---|
| `terrain_day` | ≥0.90 (1.00) | ≥0.55 (1.00) | ≥10 (18.0) | ≥20 (37) | ≥0.15 (0.25) | ≥0.50 (0.84) | ≥3 (674) | ≤0.02 |
| `ecosystem` | ≥0.70 (0.81) | ≥0.50 (1.00) | ≥8 (12.0) | ≥20 (45) | ≥0.15 (0.33) | ≥4.0 (8.6) | ≥4 (718) | ≤0.02 |
| `organism_forage` | ≥0.85 (1.00) | ≥0.50 (1.00) | ≥7 (10.5) | ≥20 (37) | ≥0.15 (0.37) | ≥8.0 (15.4) | ≥4 (686) | ≤0.02 |
| `creature_walk` | ≥0.80 (1.00) | ≥0.45 (1.00) | ≥6 (8.6) | ≥20 (35) | ≥0.15 (0.37) | ≥8.0 (14.0) | ≥4 (685) | ≤0.02 |
| `terrain_weather` | ≥0.90 (1.00) | ≥0.40 (1.00) | ≥12 (24.0) | ≥20 (40) | ≥0.15 (0.26) | ≥1.0 (2.1) | ≥3 (674) | ≤0.02 |
| `terrain_dusk` | ≥0.90 (1.00) | ≥0.40 (1.00) | ≥15 (35.0) | ≥20 (37) | ≥0.15 (0.29) | ≥1.5 (3.4) | ≥3 (674) | ≤0.02 |

Ayrıca her sahne için **determinizm** kapısı vardır: aynı seed ile iki render
bit-özdeş olmalıdır (bu, ürünün "kamera dışı durumu koru" ve "deterministik
simülasyon" ilkeleriyle uyumludur).

## Otomatik kanıt

- **Araç:** `tools/ws_quality <dizin> [seed]` altı sahneyi gerçek katalogdan
  kurar, her birini render eder, ölçer ve `quality.csv` + konsol özeti yazar.
  Herhangi bir sahne eşiği veya determinizm kapısını geçemezse çıkış kodu
  sıfırdan farklı olur ve `WS_QUALITY_FAIL` yazılır.
- **Birim testleri (`tests/test_render.c`):**
  - `quality metrics react to frame` — düz bir kare sıfır kontrast/doygunluk ve
    tek renk kovası verir.
  - `quality specs are six product scenes` — tam olarak altı sahne ve beklenen
    kimlikler.
  - `quality check gates each metric` — her eşik gerçekten kapı işlevi görür:
    eşiğin altındaki metrik doğru hata bitini üretir, üstündeki geçer.
  - `quality checklist passes on product scenes` — altı ürün sahnesi gerçek
    katalogdan kurulup kontrol listesini geçer.
- **CI kapısı:** `ci.yml` "Product visual-quality checklist" adımı
  `WS_QUALITY_OK`, altı `verdict=PASS` ve `reproducible=1` arar.
- **Windows paketi:** `tools/package-windows.ps1` aynı aracı çalıştırıp
  `evidence/quality/` çıktısını pakete koyar.

## Referans kabul protokolü

Bir render özelliği "görsel kalite açısından tamam" sayılmak için:

1. `ws_golden` bit-özdeşliği korunur (değişmezlik kanıtı).
2. `ws_quality` altı sahnede `WS_QUALITY_OK` verir (kalite kapısı).
3. Aynı seed iki koşuda bit-özdeş kare üretir (determinizm).
4. Yeni/opt-in özellikler mevcut golden kareleri değiştirmez; yalnız eklemeli
   olur (`docs/08`).

Teknik regresyon (adım 1) ile kalite kontrol listesi (adım 2-3) birlikte
kullanılır; ikisi birbirinin yerine geçmez.

## Bilinen sınırlamalar

- Ölçüler sahnenin **teknik** kalite sinyalleridir; insan estetik yargısının
  yerine geçmez. Amaç, kaymayı ve eksik geometri/ışık/atmosferi otomatik
  yakalamaktır.
- Eşikler 640×360 referans çözünürlüğünde kalibre edilmiştir. Çözünürlük
  arttıkça `colour_variety` büyür, `coverage`/`geometry_coverage` benzer kalır;
  kapı sabit 640×360 koşar, bu yüzden eşikler çözünürlükten bağımsız kararlıdır.
- Fotogerçekçilik hedefi (tüy/kürk, deri altı saçılımı, küresel aydınlatma)
  ayrı içerik/araştırma dilimleridir (`docs/19`); bu kontrol listesi onları
  kanıtlamaz, yalnız mevcut referans sahnelerin teknik kalitesini kapılar.
