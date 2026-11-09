# P5 — Canlı biyomekaniği model kartları

Bu dizin, `06-CANLI-BIYOMEKANIK.md` ve `02-BILIMSEL-MODELLEME.md` uyarınca
referans tür için zorunlu model kartlarını içerir. Her kart; kimlik/sürüm,
temsil edilen süreç, ölçek, durum değişkenleri, denklem/algoritma, varsayımlar,
parametre kaynağı, sayısal yöntem, doğrulama planı, kabul eşikleri ve kanıt
düzeyini kaydeder. Kanıt düzeyi D0–D4 arasındadır; D1 düzeyindeki bir model D3
diye tanıtılmaz.

| Kart | Kapsam | Kanıt düzeyi |
|---|---|---|
| `MODEL-KARTI-REFERANS-TUR.md` | Referans tür (kızıl geyik) anatomi, kütle/atalet, eklemler | D1 (oranlar literatür-bilgili; nicel D3 kıyası yok) |
| `MODEL-KARTI-YURUYUS-KONTROLU.md` | Raibert tarzı ayak yerleştirmeli yürüyüş kontrolcüsü | D2 (sayısal kararlılık ve determinizm testleri geçti) |
| `MODEL-KARTI-HAREKET-MALIYETI.md` | Kas-eşdeğeri motor işi ve hareket maliyeti | D1 (mekanik iş vekili; metabolik D3 kıyası yok) |

Bilinen ortak sınır: model düzlemseldir (sagittal); lateral denge ve yaw torku
henüz yoktur. Yürüyüş hattı zemin profili yürüyüş hattı boyunca örneklenir.
