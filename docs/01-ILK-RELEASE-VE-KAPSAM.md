# 01 — Release kapsamı ve ürün ufku

## İlk büyük release (Worldseed Engine 1.0)

İlk release bütün Dünya'yı ve bütün türleri içermez. Kendi içinde tamamlanmış ve uzun süre yaşayabilen, bilimsel temelli bir doğal ekosistemi içerir.

### Dünya

- Yaklaşık 2 km × 2 km ölçekte, tek ana ekosistem: ılıman karma orman/orman-çayır mozaiği.
- Birbiriyle bağlantılı en az dört habitat: orman içi, çayır/açıklık, tatlı su kenarı ve kayalık/yamaç.
- 2×2 km ürün başlangıç sözleşmesidir; daha geniş dünya için bölge akışı kullanılır.
- Prosedürel başlangıç üretimi ve tekrarlanabilir tohum.
- Arazi, su kaynakları, bitki örtüsü, gün/gece ve temel hava durumu.
- Kamera görüşünden bağımsız dünya ilerlemesi.
- Kalıcı bölge ve çevre değişiklikleri.

### Canlılar

- İlk release türleri sabittir: kızıl geyik (Cervus elaphus), kızıl tilki (Vulpes vulpes), Avrupa tavşanı (Oryctolagus cuniculus) ve yaban domuzu (Sus scrofa).
- İlk release'e kuş uçuşu eklenmez; uçuş ayrı biyomekanik ailesi olduğu için sonraki nesle bırakılır. Dört türün tamamı fiziksel beden ve türe özgü hareket kontrolüyle hareket eder.
- Kızıl geyik ilk yüksek ayrıntılı referans biyomekanik türüdür. Kızıl tilki, Avrupa tavşanı ve yaban domuzu da ayrı segment/kütle/atalet, eklem, aktif kuvvet/aktüatör, ayak teması ve türe özgü gait kontrolü içeren sadeleştirilmiş ama fiziksel modellerle çalışır. Hiçbir tür yalnızca animasyon klibiyle hareket ettirilemez.
- Her türün duyuları, ihtiyaçları, hareket kabiliyeti, yaşam döngüsü ve davranışları belgelenir.
- En az bir besin ağı bağlantısı ve canlılar arası gerçek ekolojik etkileşim kanıtlanır.

### Ekoloji

- Bitki büyümesi ve kaynak yenilenmesi için seçilmiş temel süreçler.
- Beslenme ve su arama.
- Avcı-av/tehlike ilişkisi.
- Enerji/sağlık/üreme/yaşam döngüsünün seçilmiş kısmı.
- Popülasyon ve kaynak istatistikleri.
- Uzun süreli başsız testte ekolojik çöküş/sonsuz kaynak/negatif stok gibi hataların yakalanması.

### Görüntü ve sinema

- Kendi 3D render pipeline'ımız.
- Yüksek kaliteli arazi, su, bitki, canlı modeli ve malzeme hattı.
- Dinamik ışık, gölge, atmosfer ve post-process.
- Fizik modelinden beslenen beden hareketi.
- Otomatik sinematik yönetmen, serbest kamera ve canlı takip.
- Gerçek olay zaman çizelgesi ve replay.
- Referans yüksek kalite profilinde native 4K/30 FPS; üst seviye profilde 4K/60 FPS hedefi. Daha düşük sınıfta otomatik dengeli profil ve 1080p/30 FPS hedefi.

### Kullanıcı ürünü

- Windows x64 uygulaması ve çevrimdışı yerel dünya.
- Yeni dünya oluşturma, kaydetme, yükleme ve kurtarma.
- Simülasyon duraklatma ve hız kontrolü.
- Kamera modları ve canlı inceleme paneli.
- Olay geçmişi, istatistik panelleri ve dünya haritası.
- Kullanıcı test yapmadan otomatik test ve release kanıtı.

## Sonraki ürün nesilleri

| Nesil | Genişleme hedefi | Release kapısı |
|---|---|---|
| 1.0 | Bir tamamlanmış referans ekosistem ve sinematik deneyim | Ürün sözleşmesindeki tüm 1.0 kriterleri |
| 2.0 | Birden fazla ana biyom, daha fazla tür, gelişmiş ekoloji ve daha büyük bölgeler | Türler arası yeni ekolojik ağ ve ölçek testleri |
| 3.0 | Daha ileri biyomekanik/fizyoloji, gelişmiş öğrenme, genetik ve evrimsel süreçler | Bilimsel referanslı model doğrulaması ve nesiller boyu deney |
| 4.0 | Daha gelişmiş GPU yolu, görsel kalite, içerik üretim araçları ve daha büyük dünya | Referans sahne görsel ve performans kabulü |
| 5.0 | Çoklu kalıcı dünya, uzun zaman ölçekleri ve ileri ölçeklenebilirlik | Dünya sürekliliği, uzun süre ve kaynak bütçesi testleri |

Bu tablo takvim vaadi değildir. Bir sonraki neslin kapsamı, önceki neslin kanıtları ve yeni karar kaydıyla kesinleşir.

## V1.0'a kesinlikle sığdırılmayacak işler

- Gezegenin tüm kıtalarının eksiksiz simülasyonu ve ilk release'te kuş uçuşu biyomekaniği.
- Deniz tabanı, tektonik, küresel iklim ve tüm okyanus biyolojisinin ayrıntılı çözümü.
- Yüzlerce tür için ayrı yüksek doğruluklu kas-iskelet modeli.
- İnsan benzeri bilinç veya kendi kendine kod yazan canlılar.
- Ağ üzerinden çok kullanıcılı dünya.
- Kullanıcıların sınırsız mod/eklenti yüklediği genel eklenti sistemi.

Bunlar gelecekte ayrı araştırma veya ürün kararlarıdır.
