# 12 — Otomatik test, kabul ve CI

## Kullanıcı manuel testi yok

Geliştirme ve release kabulünün normal yolu otomatik olmalıdır. Kullanıcıdan klavye/fare ile özellik denemesi, ekran görüntüsü değerlendirmesi veya uzun test çalıştırması istenmez. Kullanıcı yalnız kapsam/ürün yönü gibi proje sahibi kararlarını verir.

## Test katmanları

1. **Unit:** matematik, birim, bellek, veri biçimi, fizik primitive, RNG.
2. **Property/fuzz:** bozuk kayıt, aşırı değer, NaN/Inf, taşma, rastgele geometri/parametre.
3. **Integration:** dünya-çevre-fizik-life-ecology bağları.
4. **Scientific regression:** analitik çözüm, referans veri, korunum, yakınsama.
5. **Determinism:** seed, event order, checkpoint/replay, region regeneration.
6. **Ecosystem:** çoklu tür, kaynak, popülasyon, uzun zaman ve çoklu seed.
7. **Rendering golden scenes:** çözünürlük, referans görüntü, renk/derinlik, frame capture.
8. **Cinematic:** yönetmen salt okunur, olay-kamera eşleşmesi, takip/shot continuity.
9. **Audio:** PCM determinism, örnek sınırları, aygıt dropout.
10. **GUI E2E:** gerçek Windows pencere, girdi, pencere resize, kamera, panel, tam ekran ve kapatma.
11. **Performance:** CPU/GPU/RAM/VRAM, FPS percentile ve sim step latency.
12. **Soak:** uzun saatli simülasyon, bellek büyümesi, world save/reload.
13. **Packaging:** temiz Windows ortamında kurulum/paket açılışı ve varlık doğrulama.

## Test kanıtı

Her kabul edilen işte:

- test komutları ve build SHA;
- compiler/OS/hardware profile;
- ham CSV/JSON ölçüm;
- test log'u;
- gerçek frame/video/audio kanıtı (gereken yerde);
- referans karşılaştırma raporu;
- başarısız ve kapsam dışı kalan testler;
- kabul sonucu ve bilinen sınırlamalar.

Kanıtlar `evidence/<milestone>/<task>/` altında sürümlenir. Büyük videolar Git deposunu şişirmemek için Actions artifact veya release asset şeklinde tutulur; özet ve checksum depoda kalır.

## Görsel kanıt standardı

- Gerçek uygulama tarafından üretilir; konsept görseli kanıt olamaz.
- Seed, scene ID, commit, backend, çözünürlük, kalite preset'i ve cihaz kaydedilir.
- Golden image farkı için SSIM/PSNR/pixel diff gibi teknik ölçüler kullanılabilir; toleranslar sabit ve gerekçeli olmalıdır.
- Görsel regresyon aracı estetik kaliteyi tek başına ispatlamaz. Fotogerçekçilik hedefi için sahne bazlı görsel kalite kontrol listesi ve referans kabul protokolü gerekir.
- Video kanıtı olayın gerçek simülasyon durumundan üretildiğini event ID ile eşleştirir.

## CI güvenliği

- PR derleme/testleri GitHub-hosted runners üzerinde.
- Zorunlu C17 compiler matrix (GCC, Clang, MSVC) ve Debug/Release.
- Self-hosted GUI runner ayrı, izole ve sırları olmayan bir Windows VM olmalı.
- Self-hosted runner'a dış/fork PR kodu kontrolsüz çalıştırılmaz.
- GUI workflow'u yalnız güvenilir dal/ref allowlist veya güvenli onay mekanizmasıyla çalışır.
- `main` korumalı; zorunlu testler geçmeden merge yasak.
- Gerekli testlerden biri skipped/unknown ise başarı sayılmaz.

## Release kapısı

- Tüm zorunlu testler geçer.
- Son release commit'i tüm derleme/testlerden geçmiştir.
- Tam otomatik GUI ve package smoke testleri aynı commit'i doğrular.
- Referans 4K görsel/performance testleri kanıtlanır.
- Uzun süreli testler, kayıt/replay ve save recovery geçer.
- Bilinen kritik/yüksek hata yoktur; kalanlar belgelenmiştir.
- Ürün sözleşmesinin bütün release kriterleri karşılanmıştır.
- Paket manifesti, hash/checksum ve sürüm notu üretilir.

CI yeşili tek başına ürünün görsel veya bilimsel hedefe ulaştığını kanıtlamaz; ilgili kabul kanıtı da gereklidir.
