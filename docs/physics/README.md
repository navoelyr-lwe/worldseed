# P4 — Çevresel fizik model kartları

Bu dizin `02-BILIMSEL-MODELLEME.md` standardındaki zorunlu model kartlarını
tutar. Her kart, `05-FIZIK-MOTORU.md` kapsamındaki bir alt modeli belgeler ve
kanıt seviyesini (D0–D4) açıkça kaydeder. Uygulama ISO C17'dir; kod
`src/physics/` altındadır ve `tests/test_physics.c` ile doğrulanır.

| Model | Modül | Kanıt |
|---|---|---|
| Rijit cisim dinamiği ve entegratör | `src/physics/ws_physics.c` | D2 |
| Zemin teması, sürtünme ve restitüsyon | `src/physics/ws_physics.c` | D2 |
| Rüzgâr profili | `src/physics/ws_wind.c` | D2 |
| Hava sürtünmesi ve kaldırma kuvveti | `src/physics/ws_physics.c` | D2 |
| SI birim ve sabitler | `src/physics/ws_units.h` | D1 |

## Ortak kapsam ve geçerlilik

Bu alt modeller "tam bilimsel genel fizik çözücüsü" iddiası taşımaz
(`05-FIZIK-MOTORU.md`). Kapsam seçilmiş çevresel süreçlerdir: düşen/yuvarlanan
rijit cisim, zemin teması, sürtünme/restitüsyon, rüzgâr sürüklemesi ve kaldırma
kuvveti. Canlı bedenleri, eklem zincirleri ve yumuşak doku P5'te bu ilkelin
üzerine kurulacaktır.

## Ortak doğrulama çerçevesi

- Analitik serbest düşüş, eğik düzlem ve restitüsyon kıyasları (D2).
- Momentum/enerji davranışı ve uzun koşu kararlılığı.
- Adım boyu ve solver iterasyonu yakınsaması.
- Geçersiz parametre (dt ≤ 0, iterasyon < 1, NULL) reddi.
- Tüm testler ASan/UBSan altında temiz (Debug) ve optimizasyonda (Release) geçer.

Düzeyler D2'de durur: analitik ve korunum testleri geçmiştir, ancak yayımlanmış
deneysel veri veya güvenilir bir referans modelle doğrudan karşılaştırma (D3)
henüz yapılmamıştır. D3 için ölçüm verisi kaynağı seçilip kart güncellenmelidir.
