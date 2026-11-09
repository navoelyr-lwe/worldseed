# 03 — C17 mimarisi ve teknik sınırlar

## Dil ve bağımlılık politikası

“Yalnız C” ürün kaynak kodu için bağlayıcıdır. CMakeLists, GitHub Actions YAML, proje belgeleri ve test/runner konfigürasyonu programlama dili tercihi değil altyapı tanımıdır. Uygulamanın ürün mantığı, editör/araç mantığı ve ortak motor C ile yazılır.

- Ürün çekirdeği ve motor mantığı ISO C17 (`.c`, `.h`) ile yazılır.
- C++ kullanılmaz.
- Hazır oyun, render, fizik, animasyon, AI, biyoloji veya ses motoru kullanılmaz.
- Üçüncü taraf çalışma zamanı kütüphaneleri eklenmez.
- CMake, derleyici, statik analiz ve test araçları geliştirme araçlarıdır; ürün runtime bağımlılığı değildir.
- Ortak çekirdek Windows/POSIX başlıkları içermez.
- Derleyiciye özel intrinsics yalnız izole ve taşınabilir C fallback'i olan ölçülmüş modüllerde kullanılabilir; her kullanım kayıt altına alınır.

## Platform ilkeleri

Windows Win32 ve Linux/POSIX arayüzleri işletim sistemi sınırında kullanılabilir. Bunlar uygulama içine eklenen oyun motoru/kütüphane değildir; pencere, giriş, saat, dosya, bellek, thread ve aygıt erişimi sağlar. Ürün, GPU sürücüsünü veya işletim sistemini yeniden yazmaz.

İlk kullanıcı uygulaması Windows x64'tür. Linux x86-64 ortak çekirdek derlemesi, başsız simülasyon, test, benchmark ve CI için zorunludur. Platform API'leri ince C adaptörleri arkasında kalır: pencere, giriş, saat, dosya dayanıklılığı, bellek bilgisi, ses aygıtı, iş parçacığı ve GPU aygıt erişimi.

İşletim sistemi API'sini kullanmak hazır oyun motoru kullanmak değildir. Ancak işletim sistemi veya donanım sürücüsü yeniden yazılmayacaktır.

## Mimari katmanlar

```text
PRODUCT / VIEWER
  UI, free camera, cinematic director, replay controls, telemetry
                 |
OBSERVATION LAYER (read-only)
  render snapshot, event feed, audio scene, camera planning
                 |
WORLD SIMULATION AUTHORITY
  world, regions, environment, physics, life, ecology, time, persistence
                 |
PLATFORM ADAPTERS
  Win32 / POSIX, clock, files, input, audio device, GPU API, IPC
```

Görüntü ve sinema katmanı otoritatif dünya belleğine doğrudan yazamaz. Dünyaya kullanıcı komutu verilecekse açık komut arayüzü, yetki ve olay kaydı üzerinden girer.

## Önerilen modüller

- `core`: türler, sonuç kodları, matematik, birimler, günlük, bellek sahipliği.
- `world`: kimlik, tohum, koordinat, bölge, nesne ve dünya zamanı.
- `environment`: arazi, hava, su, ışık koşulu ve çevresel alanlar.
- `physics`: rijit cisim, temas, sürtünme, kısıtlar ve gerektiğinde yumuşak doku.
- `life`: organizma, anatomi, fizyoloji, algı, motor kontrol, hafıza ve davranış.
- `ecology`: bitki, kaynak, popülasyon, besin ağı ve ekolojik akışlar.
- `render`: CPU raster backend, GPU backend adaptörü, sahne/ışık/malzeme ve post-process.
- `assets`: kendi mesh/material/skeleton/animation/texture tanımları ve doğrulayıcı.
- `audio`: PCM üretim, karıştırma, mekânsal sunum ve platform aygıt adaptörü.
- `observer`: kamera, olay seçimi, sinematik plan ve replay gözlemcisi.
- `persistence`: checkpoint, olay günlüğü, dünya şeması ve kurtarma.
- `platform`: OS API'leri.
- `tools`: içerik oluşturma, model doğrulama, benchmark ve kanıt araçları.

## Çekirdek sahiplik ve veri sözleşmesi

- Tek dünya bağlamı açık sahiplik taşır.
- Uzun ömürlü nesne referansları pointer değil kimlik/generation handle kullanır.
- Disk/IPC'ye ham C struct yazılmaz; endian, boyut, sürüm ve alanlar açıkça serileştirilir.
- Bellek tahsislerinde boyut çarpımı taşma kontrolü yapılır.
- Hatalar açık sonuç kodlarıyla döner; sessiz varsayılan yoktur.
- Her modülün init/shutdown veya create/destroy yaşam döngüsü bulunur.
- Genel global değişken kullanılmaz.

## Zaman modeli

Farklı alt sistemler kendi doğrulanmış zaman adımını kullanabilir. Örnek kategoriler: render kare zamanı, rijit cisim fiziği, kas/eklem kontrolü, algı/karar, bitki/ekoloji ve atmosferik süreçler. Bunların hepsi aynı FPS veya aynı adım hızına zorlanmaz. Zaman adımı, solver, örnekleme ve ölçek geçişi model kartında tanımlanır.

Simülasyon hızlandırması simülasyon adımlarını çoğaltarak yapılır; fizik solver'ın dt değeri keyfi büyütülerek kararlılık bozulmaz. Bilgisayar kapalıyken gerçek zamanlı hesaplama yapıldığı iddia edilmez.

## GPU kararı — SABİTLENDİ (ADR-001)

- Ürün/engine/araç/test mantığı ISO C17 ile yazılır.
- GPU hesaplama API'si OpenCL 1.2 uyumlu alt kümedir; GPU kernel kaynakları OpenCL C 1.2 ile yazılır. OpenCL C, C99 tabanlı, paralel kernel için sınırları/uzantıları olan bir C lehçesidir; ISO C17 ile aynı standart değildir. Bu, projede izin verilen tek GPU dili istisnasıdır.
- OpenCL 1.2 API/C kernel alt kümesi seçilmiştir; OpenCL 2.x/3.x opsiyonel özelliklerine bağımlılık kurulmaz. OpenCL 3.x sürücülerinde 1.2 uyumluluk tabanı hedeflenir. Desteklenmeyen özellikler runtime'da sorgulanır ve kullanılmaz.
- C++, CUDA, GLSL, HLSL, Vulkan shader/SPIR-V, OpenGL ve Direct3D render pipeline'ı kullanılmaz.
- GPU yalnız compute backend'dir. İlk kullanım kendi compute-raster/aydınlatma ve görüntü işleme çekirdekleridir; GPU simülasyon otoritesinin tek yazarı yapılmaz.
- CPU C17 renderer ve CPU simülasyon yolu her zaman çalışır ve zorunlu fallback'tir.
- Windows'ta OpenCL sistem GPU sürücüsünün sağladığı `OpenCL.dll`/ICD üzerinden dinamik olarak yüklenir; uygulama OpenCL runtime/driver paketini kendiyle dağıtmaz. Linux'ta sistem loader/ICD kullanılır. Harici OpenCL loader kodu ürün paketine eklenmez.
- Son piksel tamponu Windows'ta yalnızca Win32/GDI DIB sunumuyla pencereye verilir. GDI çizim motoru değildir; yalnız hazır piksel tamponunu sunar. Linux ilk release'te headless image output/test yolunu kullanır.
- GPU yoksa, sürücü bulunamazsa veya cihaz kaybolursa dünya state'i korunur ve CPU fallback'e geçilir. Yetenek tespiti ve geçiş otomatik test edilir.

Khronos'un resmi OpenCL 3.0 açıklaması 1.2 işlevselliğinin uyumluluk tabanı olduğunu ve 1.2 uygulamalarının OpenCL 3.0 cihazlarında çalıştığını belirtir. OpenCL C 1.2 seçimi sürücü çeşitliliğini ve opsiyonel 3.x özelliklerine bağımlılığı azaltmak içindir. Kaynaklar `16-ARASTIRMA-KAYNAKLARI.md` içindedir.

## Paralellik ve deterministik mod

- İlk referans doğru ve anlaşılır tek-yazarlı simülasyon modelidir.
- Ölçümden sonra bağımsız iş yükleri paralelleştirilir.
- Dünya yazımının sırası deterministik komut/olay sınırlarıyla korunur.
- Paralel reduction, RNG stream ve bölge update sırası belgelenir.
- Aynı build/cihaz/seed için tekrar üretilebilir mod zorunludur.
- Farklı CPU/GPU/derleyiciler arasında bit düzeyi eşitlik ancak özel olarak kanıtlanırsa vaat edilir.
- GPU sonucu simülasyon otoritesini etkiliyorsa deterministiklik ve cross-device farkı ayrıca doğrulanır; aksi hâlde GPU öncelikle render ve deterministik olmayan görsel işlerde kullanılır.

## Süreç mimarisi

İlk aşamada tek süreç içinde açık modül sınırları kurulabilir. Simülasyon ve gözlem API'leri baştan ayrılır. Dünya host'unun görüntüleyiciden bağımsız çalışması ölçüm ve hata kurtarma kanıtlandıktan sonra ayrı yerel süreç/IPC olarak uygulanır. IPC mesajları sürümlü, sınırlı, zaman aşımına sahip ve kimlik doğrulamalı yerel erişimle korunur.

## Kod kalitesi

- GCC, Clang ve MSVC C17 derlemeleri.
- `-Wall -Wextra -Wpedantic -Werror`; MSVC `/W4 /WX`.
- ASan/UBSan, statik analiz ve fuzz testleri.
- C API başlıkları bağımsız olarak derlenir.
- Ortak kodda platform başlığı sızıntısı taranır.
- Performans optimizasyonu benchmark öncesi değil, profil kanıtından sonra yapılır.
