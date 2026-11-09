# 11 — Performans ve donanım politikası

## İlke

CPU/GPU seçimi donanım algılama ve ölçümle yapılır. CPU yolu her zaman işlevsel fallback olarak korunur. Donanımda bulunmayan hızlandırma zorunlu tutulmaz. Minimum sistem gereksinimleri benchmark olmadan ilan edilmez.

## İş yükü profilleri

Her benchmark şunları kaydeder: CPU tam modeli, çekirdek/thread, RAM, GPU/VRAM, depolama, OS, sürücü/API, derleyici, build türü, çözünürlük, sahne seed'i, canlı/bitki/üçgen sayısı, simülasyon ayrıntısı ve sıcaklık/güç durumu biliniyorsa.

### Referans profilleri

- **S0 CPU-only:** Başsız simülasyon, render kapalı.
- **S1 CPU render:** Sabit sahne, 1080p ve 4K ayrı ölçüm.
- **S2 GPU render:** Aynı sahne/kalite hedefi ile GPU backend.
- **S3 World load:** Birden fazla bölge ve çevresel sistem.
- **S4 Life load:** farklı tür ve aktif canlı sayıları.
- **S5 Cinematic load:** sinematik kamera, gölge, bitki ve post-process açık.
- **S6 Long run:** saatlerce veya tanımlı simülasyon günü boyunca headless/GUI test.

## Performans metrikleri

- Ortalama FPS tek başına yeterli değildir; p50/p95/p99 frame time.
- Simülasyon adımı süresi ve gecikme birikimi.
- Fizik, biyoloji, ekoloji, render, audio ve IO ayrı maliyetleri.
- CPU çekirdek kullanımı ve iş kuyruğu bekleme.
- GPU frame/compute süresi ve VRAM.
- RAM, tahsis sayısı, bellek sızıntısı ve parçalanma.
- Yükleme/boşaltma ve kayıt süresi.
- Aktif/uzak bölge, canlı ve geometri sayısı.

## Ürün hedefleri

İlk görsel ürün hedefi: seçilmiş golden scene setinde sabitlenen referans sinematik profilde native 3840×2160 ve en az 30 FPS. Üst seviye donanımda 4K/60 FPS hedefi. Bu sayılar her donanıma garanti değildir; profiller otomatik algılama ve benchmark ile doğrulanır.

İlk planlama aşamasında gerçek cihaz kartı alınmadan minimum GPU/VRAM/CPU/RAM şartı ilan edilmez. Referans cihazın ölçümleri, sahne içeriği ve kalite ayarları sabitlenir. 4K hedefi yalnız çözünürlüğü değil, kabul edilmiş görsel kalite preset'ini kapsar.

## Uyarlanabilir ayrıntı

Donanıma göre: render resolution scale, gölge çözünürlüğü, bitki LOD, uzaklık, efekt kalitesi, görünür geometri, fizik substep/solver bütçesi ve uzak bölge simülasyon ayrıntısı ayarlanabilir. Ayar düşürmek dünya mantığını veya olay geçmişini değiştiremez.

## Çok çekirdek ve GPU

- Önce profil, sonra paralelleştirme.
- Bağımsız region/organism/particle işler kontrollü paralelleştirilebilir.
- Dünya yazım ve olay sırası deterministik sınırlarla korunur.
- CPU ve GPU backend eşdeğer sahne verisini kullanır.
- GPU hesaplamasıyla simülasyon durumu değişiyorsa cross-device determinism ayrıca kanıtlanır.
- GPU kernel dili/API kararı sabittir: OpenCL 1.2 API ve OpenCL C 1.2. Bu bölüm performans ve uyumluluk testini tanımlar; yeni GPU dili seçmez.

## Donanım hata davranışı

GPU yoksa/başlatılamazsa CPU fallback. GPU kaybı veya aygıt resetinde dünya state'i bozulmaz. Bellek yetmezse güvenli kalite düşürme veya açık hata; sessiz veri kaybı yok.


## Sabit ürün donanım profilleri

Bu profiller kullanıcıya teknik soru sormamak için ürün planında sabitlenmiştir. Gerçek donanım otomatik algılanır; kullanıcı profili değiştirebilir.

- **Temel profil:** 6 fiziksel CPU çekirdeği sınıfı, 16 GB RAM, 6 GB VRAM'li veya eşdeğer OpenCL 1.2 GPU. Hedef: 1080p/30 FPS dengeli görsel profil; simülasyon çekirdeği aynı bilimsel kuralları korur.
- **Referans sinematik profil:** 8 fiziksel CPU çekirdeği sınıfı, 32 GB RAM, 12 GB VRAM veya üstü OpenCL 1.2 GPU, SSD/NVMe. Hedef: seçilmiş golden scene setinde native 3840×2160, en az 30 FPS.
- **Üst seviye profil:** referans profilin üzerindeki GPU/VRAM ve CPU kaynakları. Hedef: 4K/60 FPS; bu bir garanti değil, ölçülebilir performans hedefidir.
- **CPU-only fallback:** dünya simülasyonu ve bütün temel işlevler çalışır; görüntü çözünürlüğü/efekt/LOD otomatik düşürülür. CPU-only modda 4K/30 FPS taahhüdü yoktur.

Referans profil bir model/üretici adına bağlanmaz. RTX 3060 sınıfı ve eşdeğer cihazlar ayrıca ölçülebilir; gerçek test sonucu olmadan performans iddiası yapılmaz.

## Release benchmark protokolü

Golden scene 10 dakika ısındırma ve en az 10 dakika ölçümle çalıştırılır. Her profil için çözünürlük, preset, sahne tohumu, aktif canlı sayısı, görünür geometri, CPU/GPU/RAM/VRAM, p50/p95/p99 frame time ve simülasyon adımı gecikmesi kaydedilir. Referans sinematik profilde p95 frame time 33.3 ms veya altı release hedefidir. Tek bir kısa FPS örneği kabul kanıtı sayılmaz.
