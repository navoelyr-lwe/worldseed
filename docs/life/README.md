# P5–P8 — Canlı biyomekaniği, tür ve bölge model kartları

Bu dizin, `06-CANLI-BIYOMEKANIK.md`, `02-BILIMSEL-MODELLEME.md` ve
`07-BIYOLOJI-EKOLOJI.md` uyarınca zorunlu model kartlarını içerir. Her kart;
kimlik/sürüm, temsil edilen süreç, ölçek, durum değişkenleri, denklem/algoritma,
varsayımlar, parametre kaynağı, sayısal yöntem, doğrulama planı, kabul eşikleri
ve kanıt düzeyini kaydeder. Kanıt düzeyi D0–D4 arasındadır; D1 düzeyindeki bir
model D3 diye tanıtılmaz.

## Süreç model kartları

| Kart | Kapsam | Kanıt düzeyi |
|---|---|---|
| `MODEL-KARTI-REFERANS-TUR.md` | Referans tür (kızıl geyik) anatomi, kütle/atalet, eklemler | D1 (oranlar literatür-bilgili; nicel D3 kıyası yok) |
| `MODEL-KARTI-YURUYUS-KONTROLU.md` | Raibert tarzı ayak yerleştirmeli yürüyüş kontrolcüsü | D2 (sayısal kararlılık ve determinizm testleri geçti) |
| `MODEL-KARTI-HAREKET-MALIYETI.md` | Kas-eşdeğeri motor işi ve hareket maliyeti | D1 (mekanik iş vekili; metabolik D3 kıyası yok) |
| `MODEL-KARTI-ALGI.md` | Duyusal algı (menzil/görüş alanı, en yakın hedef) | D1 (sınır davranışı; ölçülmüş algı D3 kıyası yok) |
| `MODEL-KARTI-HOMEOSTAZ.md` | İç durum ve ihtiyaç homeostazı | D2 (korunum ve kararlılık testleri geçti) |
| `MODEL-KARTI-DAVRANIS.md` | Fayda tabanlı davranış ve sınırlı öğrenme | D2 (senaryo ve çok tohum testleri geçti) |
| `MODEL-KARTI-EKOLOJI.md` | Popülasyon dinamiği, bitki üretimi, av-avcı, üreme/ölüm | D2 (korunum, sınır ve çok tohum testleri geçti) |
| `MODEL-KARTI-POPULASYON.md` | Toplam bölge popülasyonu (P8 FAR/MID vekili) | D2 (korunum, sınır ve determinizm testleri geçti) |

## Tür kartları (P7)

| Kart | Tür | Kanıt düzeyi |
|---|---|---|
| `TUR-KARTI-KIZIL-GEYIK.md` | Kızıl geyik (*Cervus elaphus*) | D2 (beden + ekoloji testleri; nicel D3 kıyası yok) |
| `TUR-KARTI-KIZIL-TILKI.md` | Kızıl tilki (*Vulpes vulpes*) | D2 |
| `TUR-KARTI-AVRUPA-TAVSANI.md` | Avrupa tavşanı (*Oryctolagus cuniculus*) | D2 |
| `TUR-KARTI-YABAN-DOMUZU.md` | Yaban domuzu (*Sus scrofa*) | D2 |

Bilinen ortak sınır: beden modelleri düzlemseldir (sagittal); lateral denge ve
yaw torku henüz yoktur. Ekolojik oranlar kalibre edilmemiştir; besin ağı ilk
release'te tek kenardır (tilki → tavşan).

