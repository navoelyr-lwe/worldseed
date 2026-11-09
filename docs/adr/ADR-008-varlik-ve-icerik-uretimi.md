# ADR-008 — Varlık ve içerik üretimi

**Durum:** Kapalı (kabul edildi)

## Bağlam

`08-CIZICI-VE-VARLIK-HATTI.md`, `14-OPENHANDS-CALISMA-KURALLARI.md` ve `15-RISKLER-KARARLAR.md` eski sürümlerinde "kamu malı/CC0 veya ticari kullanıma açık hazır veri varlıkları kullanılabilir" ifadeleri vardı. Kök `AGENTS.md` ise bağlayıcı olarak ilk release içeriğinin dışarıdan hazır paket olarak alınmayacağını, kendi C17 içerik/varlık hattımızla özgün üretileceğini söylüyor. Bu çelişki giderilmelidir.

## Karar

- İlk release'e giren mesh, doku, materyal, rig, animasyon ve ses içerikleri kendi C17 varlık üretim hattımızla özgün/prosedürel olarak üretilir veya proje içinde tanımlanır.
- Harici hazır model/texture/animation/audio asset paketi kullanılmaz.
- Harici executable, script, plugin, kütüphane, engine veya runtime eklenmez.
- Bilimsel parametre ve ölçüm verileri yalnız model kartlarında kaynak gösterilerek referans alınabilir; bunlar runtime varlık paketi değildir ve ürüne varlık olarak gömülmez.
- Üretilen her varlığın üretici aracı, sürümü ve türetme zinciri `ASSET-MANIFEST.csv` dosyasına yazılır.

## Gerekçe

- Lisans belirsizliğini ve hukuki riski tamamen ortadan kaldırır.
- İçerik bütünlüğünü, tekrar üretilebilirliği ve sürüm izini garanti eder.
- C-only, bağımsız runtime hedefiyle birebir tutarlıdır.

## Sonuç

- `08`, `14` ve `15` belgeleri bu kararla güncellendi.
- İçerik üretim kapasitesi yeni bir mühendislik riski olarak `15-RISKLER-KARARLAR.md` risk tablosuna eklendi.

## Geriye dönük uyumluluk

Bu karar ürün görsel kalite hedefini düşürmez; içeriğin kendi hattımızda üretilmesini gerektirir. Üretim hattı P2'den itibaren adım adım kurulur.
