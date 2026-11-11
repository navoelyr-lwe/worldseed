# 00 — Ürün sözleşmesi

## 1. Ürün tanımı

Worldseed Engine; kendi uzamsal, fiziksel, çevresel, biyolojik ve zamansal kuralları içinde çalışan, canlıların algılayıp etkileştiği, ekosistem süreçlerinin birbirini etkilediği, durumunu ve geçmişini koruyan dijital dünyalar üretmek için geliştirilen bağımsız bir C tabanlı dünya motorudur.

Ürün, dünyayı önceden hazırlanmış bir film veya sahne olarak oynatmaz. Simülasyon dünyada ne olacağını belirler. Görüntüleyici dünyayı gösterir. Sinematik gözlemci ise gerçekleşmiş olayları seçip uygun kamera diliyle sunar.

## 2. Değişmez ilkeler

1. **Dünya kamera için yaşamaz.** Kamera hiçbir canlıya hedef, hareket, duygu, olay veya davranış veremez.
2. **Gözlemci salt okunurdur.** Sinematik yönetmen ve görüntüleme katmanı simülasyonun otoritatif durumunu değiştiremez.
3. **Hareket yalnız animasyon değildir.** Görsel hareket, türün beden/eklem/kuvvet/temas ve kontrol modelinden türetilir. Animasyon klipleri varsa yalnız görsel sunum/yardımcı katmandır; fiziksel gerçeğin yerine geçmez.
4. **Bilimsel iddia kanıt ister.** Her modelin kaynağı, varsayımları, ölçeği, parametreleri, geçerlilik alanı ve doğrulaması belgelenir.
5. **C dili ilkesi.** Motor, uygulama, editör/araçlar, test harness'ları ve ürün mantığı ISO C17'dir. GPU hesaplama çekirdekleri için tek ve önceden kararlaştırılmış istisna OpenCL C 1.2'dir (C99 tabanlı C lehçesi). C++, GLSL, HLSL, CUDA, shader DSL, betik tabanlı ürün mantığı ve üçüncü taraf oyun/fizik/render/AI/ses çalışma zamanları kullanılmaz. Bu karar ADR-001'de kapatılmıştır.
6. **Donanıma uyum.** CPU C17 yolu her özellik için işlevsel referans ve fallback'tir. Desteklenen GPU'larda yalnız OpenCL C 1.2 compute kernel'leriyle hızlandırma yapılır. GPU bulunmaması veya sürücünün çalışmaması dünyanın açılmasını engellemez.
7. **Dünya sürekliliği.** Görünmeyen alanlar yok olmaz. Uzak alanlar düşük ayrıntılı modele geçebilir ama nüfus, kaynak, zaman, durum ve geçmiş korunur.
8. **Tekrarlanabilirlik.** Sabit tohum, sürümlü üretim ve kayıt/olay geçmişiyle bilimsel deneyler tekrarlanabilir.
9. **Kanıt olmadan tamamlandı yok.** Kod, yeşil CI, gerçek çalışma çıktısı ve ilgili kabul kriterleri birlikte gerekir.
10. **Kullanıcı manuel testine bağımlılık yok.** Normal geliştirme ve release kabulü otomatikleştirilir; kullanıcıya test görevi verilmez.

## 3. Son kullanıcı deneyimi

Kullanıcı Windows masaüstü uygulamasını açar; yeni dünya oluşturur veya kayıtlı dünyasını yükler. Dünya tam ekran gerçek zamanlı 3D ortamda ilerler. Varsayılan görünüm otomatik sinematik gözlemdir. Kullanıcı istediğinde serbest kamera, canlı seçimi, canlı bilgi paneli, dünya haritası, olay zaman çizelgesi, duraklatma/hız kontrolü ve geçmiş olayları tekrar izleme işlevlerine ulaşır.

Kamera hareketi simülasyonu etkilemez. Pencere odağı, görünürlük veya kamera konumu canlıların kararlarını değiştirmez. Kamera dışındaki bölgelerde de tanımlanan simülasyon ayrıntısı seviyesine göre dünya ilerler.

## 4. Gerçekçilik tanımı

“Gerçekçi” sözü her atomu çözmek anlamına gelmez. Ürün gerçekçiliği aşağıdaki seviyelerde tanımlanır:

- **Makro dünya:** arazi, hava, su, bitki, ışık ve kaynakların tutarlı fiziksel/çevresel modelleri.
- **Organizma:** türüne uygun anatomik temsil, fizyoloji ve yaşam süreçleri.
- **Biyomekanik:** kütle, eklem, kuvvet, kas grupları, temas, denge ve hareket kontrolü.
- **Davranış:** algı, iç durum, hedef, karar, hafıza ve öğrenme.
- **Ekoloji:** kaynak, besin ağı, rekabet, avcı-av, üreme, ölüm ve popülasyon değişimleri.
- **Görüntü:** gerçek zamanlı yüksek kaliteli 3D malzeme, ışık, gölge, atmosfer, çevre ve doğal beden hareketi.
- **Sinematik:** yalnızca gerçekleşmiş olayların anlaşılır, akıcı ve estetik biçimde gözlemlenmesi.

Her seviyenin kendi sayısal hata sınırı ve kabul testleri bulunur. “Bilimsel olarak birebir” gibi sınırsız iddia kullanılmaz.

## 5. Görsel ürün hedefi

İlk büyük release için seçilmiş referans sahnelerde yüksek kaliteli, doğal ve fotogerçekçiliğe yaklaşan çevre/varlık kalitesi hedeflenir. Kalite yalnız 4K çözünürlük değil; anatomi, geometri, malzeme, doku, ışık, gölge, atmosfer, hareket ve kamera bütünlüğü olarak kabul edilir. Referans görüntü grubu en az şu sahneleri kapsar:

1. Gündüz geniş orman/vadi/nehir planı.
2. Çayırda canlıların doğal davranışı.
3. Su kaynağında birden fazla türün etkileşimi.
4. Yakın plan beden/baş/hareket ayrıntısı.
5. Yağmur/sis gibi atmosferik koşul.
6. Gece veya düşük ışık sahnesi.

Ürün çıktı hedefi ekranın doğal çözünürlüğünü desteklemek; yüksek kalite referans profilde native 3840×2160 ve en az 30 FPS, üst seviye profilde 4K/60 FPS'tir. Başlangıç referans profili 8 fiziksel CPU çekirdeği, 32 GB RAM ve en az 12 GB VRAM'li OpenCL 1.2 uyumlu GPU sınıfıdır. Bu hedef tüm bilgisayarlara garanti değildir. Daha mütevazı donanım için 1080p/30 FPS dengeli profil hedeflenir; sistem kaliteyi otomatik ayarlar. Daha düşük donanımda çözünürlük, gölge, uzaklık, bitki yoğunluğu ve simülasyon ayrıntısı uyarlanır; temel dünya kuralları korunur.

Görsel kalite yalnız çözünürlükle ölçülmez. Geometri, doku/malzeme, ışık, gölge, atmosfer, animasyon/biomekanik, kamera, kare kararlılığı ve sahne tutarlılığı birlikte değerlendirilir.

## 6. Bilimsel dürüstlük ve model sınırı

- Evrensel/atomik/moleküler tam Dünya simülasyonu ürün vaadi değildir.
- Kuantum, bütün moleküler etkileşimler, tüm hücrelerin ayrıntılı simülasyonu ve insan bilinci ilk ürün kapsamı dışındadır.
- Gerektiğinde daha ayrıntılı alt-model araştırması yapılabilir; ancak hangi ölçeği çözdüğü açıkça yazılır.
- Veri yoksa parametre “bilinmiyor/varsayım” olarak kaydedilir.
- Kalibre edilmemiş model bilimsel gerçek olarak tanıtılmaz.

## 7. Ürün kapsamı dışı (ilk büyük release)

- Multiplayer ve internet üzerinden ortak dünya.
- Herhangi bir gezegenin eksiksiz, her noktada atom düzeyinde simülasyonu.
- Bütün dünya türlerinin eksiksiz kütüphanesi.
- İnsan bilinci veya insan seviyesinde genel zekâ iddiası.
- Fotogerçekçi doğal dil konuşmasının zorunlu olması.
- Profesyonel film kurgu yazılımı ve sıkıştırılmış sinema dağıtım hattı.
- GPU sürücüsü veya işletim sistemi yazmak.
- Üçüncü taraf motor/kütüphane bağımlılığı.

## 8. Release tamamlandı tanımı

İlk büyük release ancak aşağıdakilerin tümü kanıtlanınca tamamlanır:

- 2 km × 2 km başlangıç referans bölgesi, tek ılıman karma orman/orman-çayır ekosistemi ve birbirine bağlı en az dört habitat gerçek çalışan üründe bulunur.
- Kızıl geyik, kızıl tilki, Avrupa tavşanı ve yaban domuzu için türe özgü beden, eklem, kütle/atalet, kuvvet/aktüatör, temas ve hareket kontrol modelleri bulunur; ilk referans türde daha yüksek biyomekanik ayrıntı doğrulanır.
- Bitki, kaynak ve hayvan etkileşimleri çalışan ekolojik döngü oluşturur.
- Dünya kamera dışında da ilerler ve yeniden yüklenince sürekliliği korunur.
- Sinematik yönetmen gerçek olayları değişmeden gösterir ve tekrar oynatma çalışır.
- Doğal ses manzarası release ürününün parçasıdır: rüzgâr/yaprak, yağmur, su ve habitat ambiyansı ile tür ve davranışla ilişkili hayvan sesleri gerçek simülasyon durumundan üretilir; sahte olay veya önceden kaydedilmiş sahne akışı dünyadaki olayların yerine geçmez.
- Ses mekânsal sunumda kaynak yönü ve mesafeye ek olarak engel/örtülme ve ortamın yayılım etkilerini hesaba katar; kullanılan akustik modelin varsayımları ve kanıt seviyesi belgelenir.
- Windows ürününde gerçek zamanlı ses aygıtı çıkışı bulunur. Ses aygıtının kaybı, taşması veya devre dışı kalması dünya simülasyon adımını bloke etmez.
- Aynı seed, model sürümü ve olay akışı aynı deterministik ses çıktısını üretir; WAV dışa aktarma ve canlı ses yolu aynı ses kaynaklarını/karışım kurallarını kullanır.
- Referans sahneler hedef görsel kalite kriterlerini karşılar.
- Referans donanımda golden scene seti native 4K/30 FPS hedefini karşılar. Hedef geçilmezse optimizasyon ve kalite profili çalışması sürer; sessizce kabul eşiği düşürülemez.
- Tüm otomatik testler, uzun süre testleri, güvenlik ve paket doğrulamaları geçer.
- Release paketi temiz Windows kurulumunda otomatik doğrulanır.
