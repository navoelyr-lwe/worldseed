# 04 — Dünya, coğrafya ve çevre sistemleri

## Hedef

Tohumla üretilebilen, farklı habitatlar içeren, kamera görüşünden bağımsız gelişen ve yaşanmış değişiklikleri koruyan doğal dünya.

## Coğrafi veri modeli

- Hiyerarşik bölge kimliği ve yerel koordinatlar.
- Küresel koordinat hassasiyetini koruyan region-local / floating-origin yaklaşımı.
- Deterministik prosedürel arazi ve içerik üretimi.
- Yükseklik, eğim, yüzey sınıfı ve hidrografik bağlantılar.
- Bölge üretici sürümü ve world seed.
- Komşuluk, yükleme/boşaltma ve geçiş sözleşmeleri.
- Yaşanmış değişikliklerin prosedürel başlangıç verisinden ayrı saklanması.

## Arazi

İlk sürümde seçilen referans ekosistem için yükseklik alanı, kaya/zemin sınıfları, yamaç, çukur, kıyı, patika ve doğal geçişler oluşturulur. Erozyon ve jeolojik süreçler ilk release'te seçilmiş basit modellerle temsil edilir; tam tektonik/gezegen jeolojisi kapsam dışıdır.

## Su

- Kaynak, akarsu, göl ve su birikimi.
- Arazi eğimi ve havza bağlantısı.
- Basit akış, seviye ve taşma modeli.
- Buharlaşma/yağışla su bütçesi.
- Canlıların su tüketimi ve erişimi.
- Görsel yüzey ile fiziksel su modeli arasında veri sözleşmesi.

Su görüntüsü yalnızca shader efekti değildir; suyun simülasyon durumu ile görsel yüzeyi tutarlı olmalıdır. İlk sürümde akışkanlar için tam Navier–Stokes çözümü zorunlu değildir; kullanılan yaklaşım ve geçerlilik sınırı belgelenir.

## Atmosfer ve hava

İlk sürüm: güneş konumu, gün-gece, sıcaklık alanı, rüzgâr, bulut/yağış durumu, nem ve sis/atmosfer görünümü. Daha gelişmiş bulut dinamiği, konveksiyon, basınç alanı ve iklim döngüleri sonraki nesillerdir.

Görsel yağmur, rüzgâr veya sis fiziksel/çevresel durumla ilişkili olmalıdır. Rastgele post-process efekti bilimsel hava modeli diye sunulamaz.

## Bitki

Bitkiler tür/işlev grubu bazında su, ışık, sıcaklık ve besin koşullarına göre büyür. İlk release için sınırlı sayıda bitki işlev grubu ve görsel varlık tipi yeterlidir. Bitki model kartları büyüme, kaynak alımı, mevsimsel durum ve canlı tüketimi parametrelerini tanımlar.

## Habitat ve biyom

İlk release tek ana biyom içinde birden çok alt habitatı kapsar. Habitat geçişleri sıcaklık, su, yükseklik, zemin ve bitki örtüsü alanlarından türetilir. V2+ yeni biyomlar ekler.

## Dünya üretimi ve süreklilik testleri

- Aynı seed/üretici sürümü/adres aynı başlangıç bölgesini verir.
- Komşu bölgelerde dikiş, su seviyesi ve yükseklik sıçraması sınanır.
- Negatif koordinat ve dünya sınırı test edilir.
- Bölge boşalt/yükle çevrimi durum kaybettirmez.
- Kamera taşınırken simülasyon adımı durmaz.
- Uzaklaştırma/yakınlaştırma ayrıntı seviyesi değişiminde nesneler zıplamaz.
- Uzun süreli yağış/su/bitki testlerinde kütle ve kaynak dengesi ölçülür.
