# ADR-011 — Sahne sözleşmesi, CPU renderer ve ilk varlık formatı

**Durum:** Kapalı (kabul edildi)

## Bağlam

`08-CIZICI-VE-VARLIK-HATTI.md` kendi CPU referans/fallback renderer'ımızı, ortak sahne sözleşmesini ve kendi varlık üretim hattımızı zorunlu kılar. P2'nin ilk diliminde bu yolları somut bir sözleşmeye ve dosya biçimine bağlamak gerekiyordu. Hiçbir harici render motoru, shader dili veya hazır asset paketi kullanılamaz.

## Karar

- **Sahne sözleşmesi:** Dünya/simülasyon katmanı renderer'a doğrudan bağlanmaz; yalnız `ws_scene` (kamera, yönlü ışık, malzeme, örnek mesh listesi) üretir. Renderer sahneyi yalnız okur, dünyayı değiştiremez. Aynı sahne CPU ve (gelecekte) GPU backend'ine verilebilir.
- **CPU renderer:** `ws_render_scene` zorunlu referans/fallback yoldur. Boru hattı: model/view/projection, yakın-düzlem kırpma, perspektif bölme, karo tabanlı tarama, perspektif-doğru nitelik interpolasyonu, derinlik tamponu, Lambert aydınlatma. `float` derinlik, RGBA8 doğrusal renk.
- **İlk varlık formatı:** Metin tabanlı, sürümlü `worldseed-mesh 1` biçimi (`vertex`/`index` kayıtları). Ham C struct diske yazılmaz. Yükleyici yapısal doğrulama yapar (indeks aralığı, indeks sayısı 3'ün katı, sonlu konum/normal).
- **Golden scene:** Sürümlenmiş sabit sahneler üretilir; kare SHA-256, üçgen sayısı ve kapsam manifeste yazılır. Görsel regresyon kendi C17 metriklerimizle (ortalama fark, PSNR, SSIM) ve toleransla denetlenir; piksel-bit eşitliği beklenmez.
- **Determinizm:** Aynı sahne, seed ve üretici sürümü aynı kareyi ve aynı varlığı vermelidir. CI bunu kare parmak izi ve asset byte karşılaştırmasıyla denetler.

## Gerekçe

- C17-only ve bağımsız runtime hedefiyle tutarlıdır; harici bağımlılık yoktur.
- Sahne sözleşmesi CPU/GPU arasında ortak semantik eşitlik sağlar (bit eşitliği değil).
- Sürümlü, doğrulanan varlık formatı ilerideki doku/materyal/LOD bölümleri için genişletilebilir taban verir.

## Sonuç

- `src/render` altında `ws_mesh`, `ws_asset`, `ws_scene`, `ws_render`, `ws_image` modülleri eklendi.
- Araçlar: `ws_golden` (golden scene + manifest), `ws_imgdiff` (toleranslı karşılaştırma), `ws_assetgen` (prosedürel varlık + `ASSET-MANIFEST.csv`).
- CI Linux ve Windows işlerinde golden scene, toleranslı referans karşılaştırması ve asset determinizm adımlarını çalıştırır.

## Geriye dönük uyumluluk

Bu karar ürün görsel kalite hedefini düşürmez; PBR/HDR, gölge, atmosfer, su, doku ve bitki LOD sonraki P2 dilimlerinde bu taban üzerine eklenir. Varlık formatı sürümlüdür; `1` sürümü ilerideki bölümlerle geriye uyumlu genişletilecektir.
