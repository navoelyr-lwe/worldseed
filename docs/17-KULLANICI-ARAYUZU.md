# 17 — Kullanıcı arayüzü ve gözlem deneyimi

## Ana deneyim

Uygulama açıldığında kullanıcı yeni dünya oluşturabilir, mevcut dünyayı yükleyebilir veya örnek dünyayı açabilir. Dünya yüklendiğinde varsayılan mod tam ekran otomatik sinematik gözlemdir. Arayüz gerektiğinde gizlenerek görüntü alanı korunur.

Açılışta modal bir **dünya seçim ekranı** gösterilir (canlı uygulamada `M` ile yeniden açılır). Üç seçenek vardır:

- **Yeni dünya oluştur:** taze bir tohumdan sıfırdan dünya kurar (tick 0). Uygulama tohumu duvar saatinden türetir, böylece her oturumda dünya farklıdır.
- **Mevcut dünyayı yükle:** kayıtlı checkpoint'i bit-özdeş geri yükler; kayıt yoksa seçilemez.
- **Örnek dünyayı aç:** sabit bir tohumla gelen örnek dünyayı açar; aynı tohum her zaman aynı dünyayı verir (deterministik).

`↑/↓` (veya `W/S`) imleci taşır, `Enter` seçilen dünyayı açar, `M`/`Esc` iptal eder (geçerli dünya korunur). Bir satıra **sol tıklama** imleci o satıra taşır; vurgulanan satıra ikinci tıklama satırı klavyedeki `Enter` gibi açar. Seçim ekranı gözlemci salt-okunurdur: onaylamadan açıp kapatmak dünyayı değiştirmez. Seçim gerçekleşince oturum yeni dünyayı gösterir ve ekran kapanır.

## Ekran ve kipler

1. **Sinematik gözlem:** Yönetmen gerçek olayları takip eder; HUD en aza indirilir.
2. **Serbest gözlem:** Kullanıcı kamerayı hareket ettirir; dünya değişmez.
3. **Canlı takip:** Kullanıcı bir canlı seçer; kamera takip eder, canlıya komut vermez.
4. **İnceleme:** Canlının türü, yaşı/yaşam evresi, temel ihtiyaçları, sağlık, son olayları ve modelin izin verdiği bilgiler gösterilir.
5. **Dünya haritası:** Bölgeler, habitatlar, kaynaklar ve seçilmiş nüfus verileri.
6. **İstatistik:** Popülasyon, doğum/ölüm, kaynak ve çevre zaman serileri.
7. **Olay zaman çizelgesi:** Dünya olayları ve gerçek simülasyon zamanları.
8. **Replay:** Önceki checkpoint/event kayıtlarından geçmişi yeniden gözlemleme.
9. **Ayarlar:** Çözünürlük, kalite, ses, giriş, kamera, simülasyon hızı ve kayıt ayarları.

### Replay (madde 8)

Replay, kaydedilmiş **gerçek dünya anlık görüntülerinden** geçmişi yeniden gözlemler;
kayıttan oynatılan yapay bir animasyon değildir. Oturum, dünyayı periyodik olarak
tam-durum checkpoint'i biçiminde sınırlı bir halka tamponda (en fazla 24 örnek)
saklar. `R` ile replay'e girildiğinde canlı durum yedeklenir ve en eski saklanan
örneğe dönülür; `,` / `.` ile örnekler arasında adım adım gezinilir; canlı durum
oturum dosyasıyla bit-özdeş biçimde geri yüklenir. Replay etkinken dünya ilerlemez.
Replay salt okunurdur: girip çıkmak otoritatif dünyayı hiç değiştirmez (aynı tohum
ve tick sayısında özet bit-özdeş kalır). HUD, kayıt penceresinin zamanını, canlı
zamanı ve imlecin konumunu ayrı gösterir (`docs/17` "Replay zamanı, yaşayan dünya
zamanından ayrı gösterilir").

## Zaman kontrolleri

Duraklatma, tek adım, normal hız ve hızlandırma seçenekleri bulunur. Hızlandırma simülasyon adımlarını çoğaltır; fizik solver dt'sini keyfi büyütmez. Replay zamanı, yaşayan dünya zamanından ayrı gösterilir.

## Kamera kontrolleri

- Serbest kamera hareketi ve dönüşü.
- Odak/zoom ve kamera yüksekliği.
- Canlı/grup takibi.
- Yönetmen ve serbest kamera arasında geçiş.
- Kamera hızını/ivmesini sınırlayan yumuşatma.
- Kamera çarpışması ve fiziksel engel farkındalığı.

**Uygulandı (kamera çarpışması):** Serbest kameranın gözü altındaki arazi
yüksekliğinin 1.5 m (`WS_SESSION_CAM_CLEARANCE_M`) üzerinde tutulur; kamera yere
gömülmez. Su yüzeyi de bir tabandır: göz su seviyesinin altına inmez. Arazi
yüksekliği ve su seviyesi dünya konumunun saf fonksiyonlarıdır, bu yüzden odak
yamağı dışında da geçerlidir. Ayarlar menüsündeki **Camera collision** satırı bunu
açıp kapatır (varsayılan açık; kalıcı ayar anahtarı `cam_collision`). Çarpışma
gözlemci salt-okunurdur: dünya özetini değiştirmez.

**Uygulandı (fiziksel engel farkındalığı):** Yükseklik tabanına ek olarak göz
katı engellerin (ağaç gövdeleri) dışına yatay olarak itilir. Engel listesi odak
yamağının renderer'ın gerçekten çizdiği ağaçlarını yansıtır (aynı eşit-adım
seçimi, `WS_ENV_MAX_TREES` sınırı), böylece kamera görünmeyen bir ağaca takılmaz.
Gövde yarıçapı renderer'ın koni ölçeğinden türetilir (taban yarıçapı
`0.5 × 0.42 × yükseklik`), bu yüzden çarpışma izi çizilenle uyuşur; itme yarıçapı
`WS_SESSION_CAM_OBSTACLE_MARGIN_M` (0.6 m) pay bırakır. İtme gövdenin kendi
çerçevesinde radyaldir ve yalnız gözü taşır; dünyayı değiştirmez. Engel yoksa
(odak yamağında ağaç yok) davranış yalnız yükseklik tabanına döner.

## Durum panelleri

Bilgiler simülasyonun gerçek state/event verisinden gelir. UI'da tahmin edilen veya eksik bilgi kesin gerçek gibi gösterilmez. Canlının iç durumu doğrudan ölçülemeyen bir şeyse model çıktısı olduğu belirtilir.

## Kullanılabilirlik

- İlk açılışta sade, tam ekran gözlem deneyimi.
- HUD ve bilgi panelleri açılıp kapatılabilir.
- UI kamera görüntüsünü gereksiz kaplamaz.
- Klavye/fare kontrol şeması ayarlanabilir.
- Erişilebilir metin boyutu ve ölçekleme.
- Hatalar sade dille gösterilir; dünya kaydı tehlikedeyse açık uyarı verilir.

## Dışa aktarma

İlk release için kanıt ve tekrar gözlem amacıyla zaman damgalı kare dizisi ve WAV ses çıktısı hedeflenir. MP4/H.264 gibi sıkıştırılmış video üretimi codec/lisans ve ek geliştirme gerektirir; ilk release zorunluluğu değildir. Kullanıcı arayüzündeki “video kaydı” ifadesi uygulanmayan formatı vaat etmemelidir.

Dışa aktarma uygulanmıştır (`worldseed.exe --live-export <dizin> [--live-export-seconds <s>]`).
Canlı ürün, sabit bir kare aralığıyla (1/30 s) koşar, her kareyi **kendi CPU referans
renderer'ımızla** çizer ve sıfır dolgulu `frame-NNNNN.ppm` dosyaları olarak yazar. Aynı
aralık için canlı prosedürel ses manzarası karıştırılır ve karelerle hizalanan tek bir
`soundscape.wav` (48 kHz, stereo, int16) yazılır; böylece WAV'da duyulan bir doğum ya da
avlanma, onu gösteren kareyle aynı zaman dilimindedir. `frames-index.txt` her karenin
dosya adını, dünya zamanını, tick'ini ve karenin SHA-256'sını satır satır kaydeder;
`export-manifest.txt` kare sayısını, kare aralığını, ses örnekleme hızını, ses kare
sayısını ve dünyanın SHA-256 özetini bir arada verir.

Dışa aktarma **deterministiktir**: aynı tohum ve kare sayısıyla iki koşu bit-özdeş
kare dizisi ve bit-özdeş WAV üretir (`tests/test_live.c` `live_export_run`). Dışa aktarma
gerçek bir koşudur (kareler dünyayı ilerletir); otoritatif dünyaya başka bir müdahalesi
yoktur ve aynı tohumdan her zaman aynı çıktıyı verir. Kanıt için `ci.yml`,
`gui-windows.yml` ve `tools/package-windows.ps1` iki koşunun kare ve WAV eşitliğini
zorunlu kapı yapar.
