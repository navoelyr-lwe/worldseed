# 08 — Kendi 3D çizicimiz ve varlık üretim hattı

## Ürün hedefi

Worldseed Engine kendi 3D görüntüleme motorunu ve kendi çalışma zamanı varlık sistemini geliştirir. Dış oyun/render motoru veya üçüncü taraf render runtime kullanılmaz.

## Render backend mimarisi

GPU yolu OpenCL 1.2 API + OpenCL C 1.2 compute kernel'leriyle sınırlıdır. Kendi tiled CPU rasterizer'ımız referans ve fallback; GPU compute-rasterizer/aydınlatma backend'i hızlandırma yoludur. Başka shader dili veya grafik API'si kullanılmaz. Windows GDI yalnızca sonuç piksel tamponunu sunar. GPU 4K hedefini karşılamazsa önce profil, algoritma ve içerik optimizasyonu yapılır; başka dile sessiz geçiş yoktur.

- CPU renderer: doğruluk referansı, fallback ve headless görüntü testleri.
- GPU renderer: desteklenen donanımda gerçek zamanlı yüksek kaliteli görüntü.
- Ortak render scene/snapshot sözleşmesi: dünya ve canlı modülleri doğrudan GPU API'sine bağlanmaz.
- Aynı sahne girdisi CPU ve GPU backend'lerine verilebilir; piksel eşitliği değil, geometri/semantik eşitlik ve tanımlı toleranslar beklenir.
- Renderer dünya durumunu değiştiremez.

## Görsel özellik hedefleri

İlk release için gerekenler aşama aşama:

- 3D model, kamera, clipping ve rasterization.
- Derinlik tamponu ve görünürlük.
- PBR benzeri malzeme parametreleri (albedo, roughness, metalness/uygun yüzey parametreleri, normal).
- HDR/linear renk ve doğru ton eşleme.
- Güneş, gökyüzü ve ortam ışığı.
- Gölge haritaları ve kademeli kalite.
- Atmosferik sis ve uzaklık etkisi.
- Su yüzeyi, yansıma/kırılma için maliyet kontrollü yaklaşım.
- Bitki yoğunluğu, LOD ve instancing.
- Mesh LOD ve view/frustum/occlusion culling.
- Beden skinning/deformasyon; gerçek fizik transform'larından türetilir.
- Alan derinliği, motion blur, exposure ve renk işleme.
- 4K çıktı, dinamik kalite profili ve çözünürlük ölçekleme.

Her görsel efekt performans bütçesine ve görsel kabul testine bağlıdır.

## Varlık/asset pipeline

Varlık dosyaları (mesh, doku, malzeme, rig, hareket ve ses) ürün kodu veya runtime kütüphanesi değildir; içerik verisidir. Motorun varlık yükleyicisi, dönüştürücüsü, doğrulayıcısı ve proje içi üretim araçları C17 ile yazılır.

İlk release'e giren mesh, doku, materyal, rig, animasyon ve ses içerikleri kendi C17 varlık üretim hattımızla özgün/prosedürel olarak üretilir veya proje içinde tanımlanır. Harici hazır model/texture/animation/audio asset paketi kullanılmaz. Harici çalıştırılabilir kod, script, plugin, kütüphane, engine veya runtime eklenemez. Bilimsel parametre ve ölçüm verileri yalnız model kartlarında kaynak gösterilerek referans alınabilir; bunlar runtime varlık paketi değildir ve ürüne varlık olarak gömülmez. Bu, kök `AGENTS.md`'deki kendi C17 varlık üretim ilkesiyle birebir aynıdır (bkz. ADR-008).

Yüksek görsel kalite yalnız çizici yazmakla gelmez. Kendi sürümlü varlık tanımlarımız ve üretim araçlarımız gerekir:

- Mesh/topoloji, normal/tangent, UV ve LOD verisi.
- Malzeme ve yüzey parametreleri.
- Doku ve mip seviyeleri.
- Skeleton, joint mapping ve skin weights.
- Animation/pose yardımcı verileri (fiziksel hareketi ikame etmez).
- Bitki/terrain procedural generator.
- Varlık doğrulama, ölçek, eksik veri ve lisans/menşei raporu.
- Paketleme ve sürüm geçişi.

İlk başta kod tabanlı prosedürel geometri ile başlanır. Sonraki aşamada C ile kendi asset compiler/validator ve temel authoring araçları geliştirilir. İçerik seçimi teknik görevdir; proje sahibine asset lisansı veya format sorusu yöneltilmez. Üretilen her varlığın üretici aracı, sürümü ve türetme zinciri `ASSET-MANIFEST.csv` dosyasına yazılır. Proje içinde üretilmeyen hazır model/texture/animation/audio paketi eklenmez; yalnızca kaynağı/lisansı belgelenmiş bilimsel ölçüm ve parametre verileri model kartlarında referans alınabilir.

## Fotogerçekçiliğin gerçek maliyeti

“Yalnız C kodu” kararı yüksek kaliteli 3D varlık ihtiyacını ortadan kaldırmaz. Mesh, doku, anatomi, kürk/tüy, rig, yüzey ve çevre çeşitliliği ayrı içerik mühendisliği gerektirir. Motor kodu ve asset araçları C17 kalır; içerik verisi kendi varlık hattımızda özgün/prosedürel olarak üretilir. Hiçbir veri varlığı harici engine/runtime bağımlılığı oluşturamaz. Kodun C olması tek başına fotogerçekçilik sağlamaz.

Fotogerçekçilik yalnız shader veya 4K ile oluşmaz. Yüksek kaliteli geometri, doğru anatomik oranlar, tüy/kürk, malzemeler, doku, ışık, çevre çeşitliliği, biyomekanik ve sinematik kamera birlikte gerekir. Bu nedenle içerik üretimi ayrı bir üretim hattı ve release kabul kapısıdır. İlk release'te seçilmiş referans canlılar ve golden scene'ler için yüksek ayrıntılı varlıklar önceliklendirilir; tüm dünya aynı anda en yüksek ayrıntıda olmak zorunda değildir.

## Golden scene seti

Görsel regresyon ve kalite için sürümlü sabit test sahneleri:

1. Orman/vadi gün ışığı geniş plan.
2. Çayır ve bitki yoğunluğu.
3. Su ve yansıma.
4. Yakın plan canlı yüzeyi ve hareketi.
5. Yağmur/sis.
6. Gece/düşük ışık.
7. Hızlı canlı hareketi ve motion blur.

Her sahne çözünürlük, kamera, ışık, seed, backend ve beklenen metrikleri kaydeder. Görsel karşılaştırma teknik regresyonu bulur; tek başına estetik kaliteyi matematiksel olarak kanıtlamaz. Bu nedenle referans görsel hedefleri, teknik kriterler ve release kalite incelemesi birlikte kullanılır.
