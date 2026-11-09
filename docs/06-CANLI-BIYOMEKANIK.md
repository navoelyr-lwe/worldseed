# 06 — Canlı bedenleri ve biyomekanik

## Hedef

Her canlı türünün görünür hareketi türüne uygun fiziksel beden, kütle, eklem, kuvvet ve kontrol modelinden üretilir. Hazır animasyonlar yalnız görüntüyü taklit etmek için kullanılamaz.

## Referans tür stratejisi

İlk önce bir referans tür seçilir ve onun modeli doğrulanır. Önerilen ilk referans, dört ayaklı orta/büyük memelidir; yürüyüş, koşu, dönüş, hızlanma, frenleme, zemin teması ve dengeyi sınamak için uygundur. Tür seçimi M0 araştırmasıyla kesinleşir.

Sonra farklı hareket sınıfları eklenir: küçük dört ayaklı, kuş/uçuş veya kanatlı hareket, etçil takip/av davranışı. Türlerin aynı rig/parametre setini paylaşması zorunlu değildir. **Release içinde bulunan hiçbir tür yalnızca hazır animasyonla hareket edemez.** Her tür için kendi anatomik segmentleri, kütle/atalet, eklem sınırı, kuvvet/aktüatör, temas ve hareket kontrol modeli gerekir. İlk referans tür en ayrıntılı doğrulanır; diğer türler de kendi tür kartı ve hareket testlerini geçer.

## Beden katmanları

1. İskelet/segment hiyerarşisi.
2. Eklem türleri, hareket sınırları ve eklem kuvvet/torkları.
3. Kas grupları veya kas eşdeğeri aktüatörler.
4. Kas kuvveti uzunluk/hız ilişkisi için uygun model seviyesi.
5. Tendon/elasticity gibi seçilmiş yumuşak bileşenler.
6. Beden kütlesi, atalet tensörü ve ağırlık merkezi.
7. Ayak/pati/kanat/yüzey temas modeli.
8. Denge ve motor kontrol.
9. Fiziksel bedenin görsel mesh deformasyonu.

## Hareket üretimi

Yürüme/koşma/yüzme/uçma farklı fiziksel rejimlerdir. Her biri için adım/kanat/vuruş frekansı, temas fazı, kuvvet, enerji maliyeti ve kontrol stratejisi türüne göre tanımlanır. Hareket klipleri ana hareket kaynağı olamaz. Gerekirse ölçülmüş hareket verisi model kalibrasyonu için referans olarak kullanılır; lisans ve kaynak kaydı tutulur.

## Fizyoloji bağlantısı

Hareket maliyeti metabolik enerji tüketimine bağlanır. Açlık, yorgunluk, yaralanma, sıcaklık ve su durumu kas kuvvetini, dayanıklılığı ve hareket seçimini etkileyebilir. Görsel nefes/beden hareketi iç modelin durumunu yansıtmalıdır.

## Görsel doğruluk

- Anatomik oran ve eklem eksenleri türle tutarlı.
- Mesh/skeleton/physics transform eşleşmesi.
- Ayakların zeminden sebepsiz kaymaması.
- Bedenin ağırlık merkezinin fiziksel davranışla uyumlu olması.
- Ani poz/ölçek değişimi ve eklem kırılması olmaması.
- Deri/kürk/tüy görselliği fiziksel mesh'ten ayrıdır ama beden hareketini örtmemelidir.

## Doğrulama

- Statik duruş ve denge testi.
- Yürüme/koşu hız aralıkları ve adım frekansı karşılaştırması.
- Kütle merkezi ve ayak yerleşimi ölçümü.
- Dönüş, yokuş, düşük sürtünme ve engel geçişi.
- Yorulma/yük/yaralanma etkisi.
- Bilimsel hareket veri seti veya yayınlanmış ölçümle kalibrasyon.
- Birden fazla tohumda uzun süreli davranış.

## Bilimsel sınır

İlk release bütün kas liflerini, motor nöronlarını, kemikleri ve her yumuşak dokuyu moleküler düzeyde çözmez. Referans tür için seçilmiş kas-iskelet ve kontrol düzeyinde model geliştirir. Ayrıntı seviyesi model kartında yazılır.
