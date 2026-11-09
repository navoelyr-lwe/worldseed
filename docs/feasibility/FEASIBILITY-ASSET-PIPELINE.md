# Fizibilite — Kendi C17 varlık üretim hattı

**Aşama bağı:** P2 (temel), P11 (olgunluk). **Karar kaydı:** ADR-002, ADR-008.

## Amaç

İlk release içeriğinin (mesh, doku, materyal, rig, animasyon, ses) kendi C17 hattımızda özgün/prosedürel üretilebileceğini planlamak ve ilk adımı doğrulamak.

## İlkeler

- İçerik veridir; ürün kodu veya runtime kütüphanesi değildir.
- Harici hazır model/texture/animation/audio paketi kullanılmaz (ADR-008).
- Üretici araçlar, yükleyici, doğrulayıcı ve dönüştürücü C17 ile yazılır.
- Her varlık için üretici aracı, sürümü ve türetme zinciri `ASSET-MANIFEST.csv`'ye yazılır.

## Aşamalı plan

1. **P2 — kod tabanlı prosedürel geometri:** primitifler ve ilk sürümlü mesh biçimi.
2. **P2 — validator:** ölçek, eksik veri, indeks/offset sınırı, normal/tangent doğrulama.
3. **P3 — terrain/bitki generator:** deterministik prosedürel arazi ve bitki yerleşimi.
4. **P5 — canlı beden geometrisi:** referans tür için anatomik segment geometrisi.
5. **P8+ — doku/materyal:** prosedürel doku üretimi ve mip zinciri.
6. **P9+ — ses:** C ile PCM sentez ve örnek yönetimi.

## Biçim taslağı (ilk sürüm)

- Başlık: sihirli sayı, sürüm, endian işareti, bölüm sayısı, her bölüm için offset/boyut/checksum.
- Bölümler: vertex, indeks, normal/tangent, UV, malzeme, skeleton, skin weights, LOD.
- Doğrulama: boyut çarpımı taşma kontrolü, offset aralık kontrolü, checksum.
- Ham C struct diske yazılmaz (bkz. `03-C17-MIMARI.md`).

## Kanıt planı

- Validator unit testleri: bozuk/kısmi/taşma/yanlış sürüm girdileri reddedilir.
- Aynı üretici sürümü ve seed için bit düzeyinde aynı varlık.
- Golden scene girdisi olarak kullanılan varlıkların SHA-256 parmak izi.

## Sınırlar

- Fotogerçekçilik yalnız varlık hattıyla gelmez; ışık/malzeme/biyomekanik/kamera birlikte gerekir (`08-CIZICI-VE-VARLIK-HATTI.md`).
- Bu belge plan aşamasındadır; ilk varlık biçimi P2'de uygulanır ve ADR ile kayda geçirilir.

## P2 uygulaması (ilk adım)

- **Prosedürel geometri:** `src/render/ws_mesh.c` küp/düzlem/küre üretir; harici asset yok.
- **İlk sürümlü biçim:** metin tabanlı `worldseed-mesh 1` (başlık + `vertex`/`index` kayıtları). Ham C struct diske yazılmaz; alanlar açıkça biçimlenir.
- **Validator:** `ws_asset_validate_mesh` indeks aralığı, indeks sayısı 3'ün katı, sonlu konum/normal ve NULL kontrolü yapar; tarayıcı yanlış sürümü (`WS_ERR_UNSUPPORTED`) ve bozuk kaydı (`WS_ERR_PARSE`) reddeder.
- **Determinizm:** aynı üretici girdisi bit düzeyinde aynı varlığı verir (unit test + golden scene kare SHA-256).

İlerideki dilimlerde ikili/sıkıştırılmış biçim, normal/tangent, malzeme bölümü, LOD ve checksum eklenir. Detay `docs/P2-RENDER-TEMELI.md` içindedir.
