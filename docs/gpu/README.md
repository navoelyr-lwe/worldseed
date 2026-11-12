# P10 — GPU ve performans model kartları

Bu dizin, `11-PERFORMANS-DONANIM.md` ve ADR-001 uyarınca P10 GPU/performans
katmanının model kartlarını içerir. Kartlar; kimlik/sürüm, temsil edilen süreç,
ölçek, durum değişkenleri, algoritma, varsayımlar, parametre kaynağı, sayısal
yöntem, doğrulama planı, kabul eşikleri ve kanıt düzeyini kaydeder.

| Kart | Kapsam | Kanıt düzeyi |
|---|---|---|
| `MODEL-KARTI-GPU-RASTER.md` | OpenCL C 1.2 düz gölgeli rasterleme kernel'i ve CPU semantik eşleşmesi | D2 (determinizm, tolerans ve device-loss testleri geçti) |
| `MODEL-KARTI-TAM-SAHNE-GPU.md` | OpenCL C 1.2 tam sahne render yolu (tüm gölgeleme ardışık düzeni) ve CPU referans karşılaştırması | D2 (17 sahne, yapısal fark 0; device-less fallback bit-özdeş) |
| `MODEL-KARTI-ZAMANLAYICI.md` | Çok çekirdekli iş zamanlayıcı: deterministik parallel-for | D2 (seri/paralel bit-özdeşlik testi geçti) |

GPU otoritatif simülasyonu etkilemez; yalnız kararı verilmiş kareyi hızlandırır.
