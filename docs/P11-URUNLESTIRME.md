# P11 — Ürünleştirme ve ilk release

Bu belge P11 aşamasının durumunu, uygulanan kodu, otomatik kanıtları ve bilinen
sınırlamaları kaydeder. Kapsam ve kabul ölçütleri `docs/13-YOL-HARITASI.md` (P11),
`docs/00-URUN-SOZLESMESI.md` bölüm 8 ve `docs/17-KULLANICI-ARAYUZU.md` içindedir.

## Kapsam

- Kullanıcıya görünür, tam ekran, gerçek zamanlı canlı gözlem uygulaması.
- Görünüm kipleri: sinematik otomatik, serbest, canlı takip, dünya haritası.
- Canlı inceleme paneli, olay zaman çizelgesi, istatistik paneli.
- Zaman kontrolleri (duraklat, hız), kamera kontrolleri, kaydet/yükle, yardım.
- Deterministik replay ve checkpoint doğrulaması.
- Windows x64 paketleme, sürüm, kanıt manifesti ve release akışı.

## Ne yapıldı

### Canlı uygulama — `src/app/ws_live.{h,c}`, `src/app/ws_live_platform.c`

`worldseed.exe` artık iki modda çalışır. Canlı mod (`--live`) pencereli gerçek
zamanlı gözlem uygulamasıdır; `--fullscreen` tam ekran başlatır. Aynı kod yolu
penceresiz de koşar: `--live-headless <saniye>` N saniye dünyayı ilerletip kare
zamanlaması ve dünya özeti yazar; `--live-tour <dizin>` görünüm değiştiren, canlı
seçen, kaydedip geri yükleyen ve her adımda gerçek PPM karesi + manifest yazan
deterministik bir tur çalıştırır. Böylece CI gerçek kare, dünya özeti ve performans
sayısı üretir; iddia ekran görüntüsü olmadan yapılmaz.

Windows'ta sunum katmanı Win32/GDI'dır: CPU referans renderer'ımızın ürettiği
RGBA8 piksel tamponu `StretchDIBits` ile sunulur (GDI bir render motoru değildir,
`docs/03`). Klavye/fare Win32 mesaj kuyruğundan okunur ve oturum komutlarına /
serbest kamera girdisine çevrilir. İç çözünürlük, pencere boyundan bağımsız olarak
CPU renderer'ın etkileşimli kalmasını sağlamak için otomatik ölçeklenir (`--live-render-scale`,
varsayılan `0` = otomatik); kanıt kareleri (headless/tour) her zaman tam boyda
render edilir.

### Oturum — `src/ui/ws_session.{h,c}`

Oturum, pencerenin arkasındaki bütün çalışan üründür: yaşayan dünyayı (P8 host),
odak bölgesinin yüksek detaylı ekosistemini (P7), sinematik gözlemciyi (P9),
prosedürel ses manzarasını (P9) ve renderer kamerasını sahiplenir; kullanıcıya
görünen işlevleri sunar. Gözlemci salt okunurdur (`docs/00`): görünüm, kamera,
seçim, duraklatma, hız ve kaydet/yükle otoritatif simülasyonu değiştirmez. Dünya,
sabit zaman adımında ilerler (render kare hızından bağımsız); yavaş bir kare
simülasyonu değiştirmez (`docs/03`). Her görünür canlı, P5 yürüyüş kontrolcüsüyle
sürülen artiküle bedendir, animasyon klibi değildir.

Odak ekosistemi odak bölgesini izler; dünyanın geri kalanı host'un toplam modeliyle
ilerler (P8). P11 bu oturuma istatistik/veri erişimcileri ekledi: doğum/ölüm,
avlanma, beslenme, açlık/yaş ölümleri ve ekosistemin aşağı örneklenmiş popülasyon
zaman serisi (`ws_session_population_history`).

### HUD — `src/ui/ws_hud.{h,c}`, `src/ui/ws_font.{h,c}`

HUD, render edilen karenin üzerine gözlemcinin göstergelerini çizer: durum çubuğu
(dünya saati, tick, hız, görünüm, nüfus, FPS), canlı inceleme paneli, olay zaman
çizelgesi, dünya haritası, P11'de eklenen istatistik paneli (demografi sayaçları
ve tür başına yığılmış popülasyon grafiği) ve kalıcı ayar paneli (`O` ile açılır;
çözünürlük, iç ölçek, başlangıç görünümü, başlangıç saati, gölge/tam ekran ve
tohumu gösterir). Tüm metin, kendi C17 bitmap
fontumuzla (5×7 glif, ASCII 32..126) çizilir; font varlığı veya üçüncü taraf metin
kütüphanesi yoktur (ADR-008). HUD yalnız sunum tamponuna yazar; dünyaya veya
render sahnesine asla yazmaz.

### Kalıcı ayarlar — `src/platform/ws_settings.{h,c}`

`docs/17` madde 9'un (ayarlar) kalıcı yolunu sağlayan, bağımsız C17 anahtar=değer
metin biçimi. `ws_settings_default`, `ws_settings_parse`, `ws_settings_format`,
`ws_settings_load`, `ws_settings_save` işlevleri çözünürlük, iç render ölçeği,
başlangıç görünümü, başlangıç saati, gölge/tam ekran bayrakları ve tohumu
saklar; bilinmeyen anahtarlar ve bozuk satırlar yok sayılır, aralık dışı değerler
kırpılır. Uygulama açılışta dosyayı okur (açık komut satırı bayrağı her zaman
dosyayı geçersiz kılar) ve pencere temiz kapanışta gözlemcinin son seçimlerini
yazar. Headless/tur kanıt koşuları dosyayı ne okur ne yazar; kanıtlar geliştirici
ayarından bağımsızdır. `ws_session_apply_settings` / `ws_session_get_settings` bu
anlık görüntüyü gözlemciye uygular; dünyaya dokunmadığı için aynı tohum ve tick
sayısında özet bit-özdeş kalır.

### Dosya sistemi yardımcıları — `src/platform/ws_fs.{h,c}`

Tur çıktı dizinlerini oluşturmak için platform izoleli, küçük `ws_fs_make_dirs`
yardımcısı (POSIX `mkdir` / Win32 `_mkdir`). Simülasyon tarafından kullanılmaz.

### Replay — `src/ui/ws_session.{h,c}`, `src/ui/ws_hud.c`

`docs/17` madde 8'in uygulaması. Oturum, dünyayı periyodik olarak tam-durum
checkpoint'i biçiminde sınırlı bir halka tamponda (en fazla 24 örnek; her örnek
yaklaşık 108 KB) saklar. `session_serialize` / `session_deserialize` yardımcıları
kaydedilen örnekleri ve dosya checkpoint'ini aynı bit biçiminde üretir, böylece
replay gerçek kayıtlı durumu yeniden oynatır. `R` replay'i açar: canlı durum
yedeklenir, "şimdi" kayıt kenarı olarak örneklenir ve en eski saklanan örneğe
dönülür; `,` / `.` örnekler arasında adımlar; `WS_CMD_REPLAY_SEEK` normalize konumu
(0..1) doğrudan gösterir; `WS_CMD_REPLAY_LIVE` canlı durumu bit-özdeş geri yükler.
Replay etkinken dünya ilerlemez. HUD, kayıt zamanı, canlı zaman ve imleç konumunu
gösteren bir replay paneli çizer; gözlemci salt okunurdur (aynı tohum ve tick
sayısında özet bit-özdeş kalır).

### Tek adım (zaman kontrolü) — `src/ui/ws_session.{h,c}`, `src/ui/ws_hud.c`

`docs/17` "Zaman kontrolleri" maddesinin tek adım kısmının uygulaması. `WS_CMD_STEP_ONCE`
komutu dünyayı tam olarak bir sabit tick ilerletir (`world_tick`) ve durakdayken de
çalışır; birikmiş kare zamanı sıfırlanır ki sonraki normal adım çift ilerlemesin.
Tek adım dünyayı durakdan çıkarmaz. Replay etkinken aynı tuş "sonraki kayıtlı örnek"
anlamına gelir; böylece tek tuş hem canlı hem replay zaman çizelgesinde ileri adımlar.
Tek adımla ilerletilen dünya, aynı tick sayısını normal kare zamanıyla ilerleten
referans dünyayla bit-özdeş bir SHA-256 özeti verir (sabit zaman adımı eşdeğerliği).

## Kontroller (kullanıcıya görünür)

| Tuş | İşlev |
|---|---|
| `Space` | Duraklat / devam |
| `.` | Tek adım (bir dünya tick'i; replay'de sonraki örnek) |
| `+` / `-` | Hız artır / azalt (0.5× .. 4×) |
| `V` | Görünüm döngüsü (auto / free / follow / map) |
| `N` / `P` | Sonraki / önceki canlıyı seç |
| `G` | İstatistik panelini aç/kapa |
| `T` | Yönlü gölgeleri aç/kapa |
| `O` | Ayar panelini aç/kapa |
| `H` | Yardım katmanını aç/kapa |
| Ok tuşları / `WASD` | Serbest kamerayı uçur |
| Fare sürükle | Serbest görünümde bakış |
| `[` / `]` | Günün saati −/+ 1 saat |
| `S` / `L` | Kaydet / yükle |
| `R` | Replay (kaydedilmiş geçmişi gözlemle) aç/kapa |
| `,` / `.` | (replay'de) önceki / sonraki örnek |
| `F11` | Tam ekran aç/kapa |
| `Esc` / `Q` | Çık |

## Kanıtlar (otomatik)

- **Canlı headless koşu** — `worldseed.exe --live-headless <s> --size WxH`: gerçek
  kare zamanlaması (step/render/hud ayrı), en kötü kare, kapsama, kare SHA-256 ve
  dünya özeti. Bu geliştirme makinesinde (1280×720, 5 s, 300 kare):

  ```
  LIVE_HEADLESS_OK frames=300 sim_seconds=5.0 world_tick=100 avg_ms=407.28
    step_ms=0.28 render_ms=406.79 hud_ms=0.21 worst_ms=450.61 fps=2.5
    coverage=1.0000 pop=50 loaded=49
  ```

  `step_ms≈0.28` gösterir ki otoritatif simülasyon adımı kareden çok daha ucuzdur;
  darboğaz CPU referans rasterleyicisidir (beklenen; `docs/11`). Aynı koşu
  320×180'de `avg_ms=29.54 fps=33.8`, 640×360'ta `avg_ms=105.98 fps=9.4` verir —
  çözünürlük ölçeklemesi doğrusaldır ve pencere modunda otomatik ölçek devrededir.

- **Deterministik tur** — `worldseed.exe --live-tour <dizin> --size WxH`: on bir adım
  (auto gündüz, serbest genel görünüm, geyik takibi, harita, alacakaranlık, tilki
  takibi, gece+istatistik, kaydet→ilerlet→yükle, gölgeler kapalı, gölgeler açık, tek
  adım). Her adım gerçek PPM karesi + SHA-256
  ve manifest yazar. Bu makinede (1280×720):

  ```
  tour step=00 auto_day     sha256=e31c82ce...  coverage=1.0000
  tour step=02 follow_deer  sha256=da72f291...
  tour step=06 auto_night   sha256=e02eb50c...
  tour save_load saved=1 loaded=1 roundtrip=1
  tour single_step paused=1 from=580 to=585 ok=1
  LIVE_TOUR_OK steps=11
  ```

  `roundtrip=1`, checkpoint kaydedildikten sonra dünyanın ilerletilip geri
  yüklendiğinde SHA-256 özetinin birebir aynı olduğunu kanıtlar. `single_step ok=1`,
  durakdayken beş tek adımın dünyayı tam beş tick ilerlettiğini ve duraklamayı
  bozmadığını kanıtlar (`manifest` içinde `single_step_verified=1`).

- **Replay kanıtı** — `worldseed.exe --live-replay <dizin> --size WxH`: dünya
  bir süre ilerletilir, replay'e girilir, en eski örnek, adımlanmış örnek ve en
  yeni örnek gerçek PPM karesi olarak yazılır; canlı dünya geri yüklendikten sonra
  devam ettiği doğrulanır. Manifest `newest_matches_live=1` (en yeni kayıtlı örnek
  canlı dünyayı bit-özdeş yeniden üretir) ve `live_restored=1` (replay'den çıkış
  canlı dünyayı bit-özdeş geri yükler) yazar. `LIVE_REPLAY_OK` çıktısı CI kapısıdır.

- **Tur tekrarlanabilirliği** — Aynı parametrelerle iki tur karesi bit-özdeş olur
  (`cmp` her `step-*.ppm` için başarılı). Bu, "gözlemci salt okunurdur" kuralını
  gerçek çıktı üzerinden doğrular; CI bu karşılaştırmayı zorunlu kapı yapar.

- **Birim testleri** — `tests/test_font.c`, `tests/test_hud.c`, `tests/test_session.c`,
  `tests/test_live.c`, `tests/test_settings.c` (toplam 42 test): font çizimi/metrikleri,
  HUD salt-okunurluk ve panel çizimi (istatistik + ayar + replay panelleri dahil), oturum
  yaşam döngüsü, duraklatma, gözlemci salt-okunurluğu, tohum determinizmi,
  kare-hızı bağımsızlığı, seçim/inceleme, zaman çizelgesi/harita, kaydet/yükle,
  istatistik/demografi, görünüm kipleri, replay (giriş/arama/adımlama/çıkış ve
  salt-okunurluk), canlı uygulama seçenek ayrıştırma + headless/tur/replay koşuları,
  ayar ayrıştırma/biçimlendirme/kaydet-yükle ve CLI↔dosya önceliği.

- **Kanıt manifesti aracı** — `tools/ws_manifest.c`: paketteki her dosya için
  SHA-256 ve boyut yazar; eksik dosya varsa sıfırdan farklı çıkış verir, böylece
  sessizce düşen bir artefakt kapıyı geçemez. `manifest_ok` ve `manifest_missing`
  testleri bu davranışı doğrular.

## CI ve release

- `ci.yml`: Linux sanitizer matrisi ve Windows MSVC işlerine "Live application
  evidence" adımı eklendi — headless koşu (`LIVE_HEADLESS_OK`), tur
  (`LIVE_TOUR_OK`, `save_load_roundtrip=1`) ve iki turun kare-özdeş olduğu `cmp`
  karşılaştırması. Kanıtlar artefakt olarak yüklenir.
- `gui-windows.yml`: izole Windows runner'da gerçek canlı pencere açılır
  (`--live --size 1280x720`), masaüstü ekran görüntüsü alınır, pencere kapatılır;
  ayrıca headless + tur + replay kanıtı alınır. Bu, yalnız Windows'ta çalışan
  Win32/GDI sunum yolunu gerçek pencerede sınar.
- `release.yml`: paket artık canlı tur karelerini (`live/*.png`, `live/manifest.txt`)
  içerir; `worldseed.exe --version` ile sürüm bildirir; `tools/ws_manifest` ile
  `MANIFEST.txt` üretilir; sürüm notu P11 canlı uygulamasını ve kalan sınırlamaları
  açıklar.
- Sürüm: `CMakeLists.txt` proje sürümü ve `src/core/ws_version.h` `0.15.0-preview`
  olarak hizalandı (tek adım zaman kontrolü dilimi); `worldseed.exe --version` aynı
  dizeyi yazdırır.

## Bilinen sınırlamalar

- 4K/30 FPS referans donanım hedefi (`docs/00` bölüm 8) henüz ölçülüp geçilemedi.
  CPU referans renderer'ı 1280×720'de etkileşimli değildir; bu yüzden pencere
  modunda iç çözünürlük otomatik ölçeklenir ve gölgelendirme/pipeline optimizasyonu
  ile GPU yolunun tam sahneye uygulanması sonraki dilimlere bırakılmıştır. Eşik
  sessizce düşürülmemiştir; hedef açık kalır.
- OpenCL GPU hızlandırma yolu bu ortamda gerçek GPU olmadığından yalnız pocl CPU
  ICD ile semantik/yol kanıtına dayanır (P10).
- Ses, prosedürel olarak sentezlenir ve WAV üretir; çalma cihazı arayüzü sonraki
  sürümdedir (`docs/10`, `docs/17`).
- Ayarlar kalıcı olarak `worldseed.cfg` (anahtar=değer) dosyasında saklanır:
  çözünürlük, iç render ölçeği, başlangıç görünümü/saati, gölge/tam ekran ve tohum.
  Açık komut satırı bayrağı dosyayı geçersiz kılar; pencere temiz kapanışta dosyayı
  yazar. Ayarlar ekranındaki ses ve giriş alt bölümleri ile dünya seçim menüsü bu
  dilimde hâlâ komut satırı bayraklarıyla karşılanır; tam etkileşimli ayar menüsü
  (fareyle düzenleme, ses düzeyi kaydırıcıları) sonraki dilimdedir.
- `1.0.0` yalnız `docs/00` bölüm 8'in bütün koşulları geçildiğinde yayımlanır.
