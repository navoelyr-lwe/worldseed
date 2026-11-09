# Mimari karar kayıtları (ADR)

Bu klasör bağlayıcı mimari kararları tek tek dosyalar hâlinde tutar. `15-RISKLER-KARARLAR.md` özet/kapalı kararlar tablosudur; buradaki dosyalar kararın gerekçesini, kapsamını ve geriye dönük uyumluluk planını içerir.

## Karar listesi

| ADR | Konu | Durum |
|---|---|---|
| `ADR-007-yetki-devri-ve-release.md` | Yetki devri ve release yayımlama | Kapalı |
| `ADR-008-varlik-ve-icerik-uretimi.md` | Varlık ve içerik üretimi (özgün C17 hattı) | Kapalı |
| `ADR-009-ci-dal-korumasi-ve-runner.md` | CI, dal koruması ve izole GUI runner politikası | Kapalı |
| `ADR-010-kaynak-kodu-lisansi.md` | Kaynak kodu lisansı ve içerik menşei kaydı | Kapalı |

## Kural

- Yeni bir teknik karar gerektiğinde OpenHands `ADR-NNN-*.md` üretir ve PR ile sunar.
- Karar değişikliği mevcut ürün hedefini düşüremez.
- Kullanıcıya teknik tercih sorusu sorulmaz; bu belgeler kararı sabitler.
