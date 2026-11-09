# 18 — Güvenlik, lisans, dosya ve paketleme

## Çalışma biçimi

İlk release yerel/çevrimdışı masaüstü ürünüdür. Ağ üzerinden hesap, telemetry, uzaktan komut veya multiplayer zorunluluğu yoktur. Ağ kullanımı ileride ayrı ürün ve güvenlik kararı gerektirir.

## Dosya güvenliği

- Save dosyası boyutu, sürümü, sayımlar ve checksum doğrulanır.
- Bozuk/kasıtlı dosya girdisi fuzz test edilir.
- Dosya yazımı geçici dosya + güvenli değiştirme yaklaşımıyla yapılır.
- Kayıt başarısız olursa son sağlam checkpoint korunur.
- Kullanıcı verisi beklenmedik konuma yazılmaz; dosya yolu UI'da görünür.
- Harici asset paketleri boyut/format/indeks/offset ve bellek taşması açısından doğrulanır.

## Gizlilik

- Kullanıcının yerel dünyası varsayılan olarak yerel kalır.
- Telemetry varsayılan olarak yoktur.
- Kanıt paketlerine kullanıcı dosyası veya özel kişisel veri eklenmez.
- CI/runner loglarında token, gizli anahtar veya yerel kullanıcı yolu yayımlanmaz.

## Lisans ve içerik menşei

- Depo ve C17 kaynak kodu için güvenli varsayılan lisans politikası **tüm hakları saklıdır**; açık kaynak lisansı varsayılan olarak verilmez (bkz. `docs/adr/ADR-010-kaynak-kodu-lisansi.md`). Bu, kullanıcıya yöneltilen bir teknik soru değildir.
- İlk release'e giren mesh, doku, materyal, rig, animasyon ve ses içerikleri kendi C17 varlık üretim hattımızla özgün/prosedürel olarak üretilir. Harici hazır model/texture/animation/audio asset paketi kullanılmaz.
- Bilimsel parametre ve ölçüm verileri yalnız model kartlarında kaynak gösterilerek referans alınabilir; bunlar runtime varlık paketi değildir ve ürüne varlık olarak gömülmez.
- Varsayılan runtime bağımlılık politikası: üçüncü taraf engine ve runtime library yok.
- OS API ve sürücü ayrı platform altyapısıdır; uygulama bunları yeniden dağıtmaz veya değiştirmez.

## Paketleme

- Windows x64 paketinde çalıştırılabilir dosya, gerekli özgün asset paketleri, kullanıcı kılavuzu, sürüm ve SHA-256 manifesti bulunur.
- Geliştirme araçları ve test binary'leri kullanıcı paketine girmez.
- Paket temiz Windows VM'de otomatik açılır ve smoke test edilir.
- GPU desteği varsa gereksinimler açıkça yazılır; GPU bulunmadığında CPU fallback davranışı açıklanır.
- Kayıt dosyası şeması yükseltmeleri migration testinden geçer.

## Release politikası

- Preview/stable release yayınlama yetkisi proje sahibi tarafından OpenHands'e devredilmiştir (bkz. `docs/adr/ADR-007-yetki-devri-ve-release.md`). Ayrıca kullanıcı onayı istenmez.
- Tag/release, `12-TEST-KABUL-CI.md` zorunlu kapıları ve `00-URUN-SOZLESMESI.md` bölüm 8 şartları geçmeden oluşturulmaz. Preview/dev artifact ile kararlı release birbirinden ayrılır.
- Yalnız belge/altyapı işleri ürün release'i sayılmaz; prerelease üretimi için kullanıcıya görünür milestone ve ilgili GUI/golden-scene/performans kapıları gerekir.
