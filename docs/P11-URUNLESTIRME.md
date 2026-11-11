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
ve tür başına yığılmış popülasyon grafiği) ve etkileşimli ayar menüsü (`O` ile
açılır). Ayar menüsü gezilebilir ve değiştirilebilir: `↑/↓` (veya `W/S`) imleci
taşır, `←/→` (veya `A/D`) seçili satırı değiştirir, `Enter` uygular, `B`
varsayılanlara döndürür, `O`/`Esc` kapatır. Satırlar çözünürlük (genişlik/
yükseklik), iç render ölçeği, başlangıç görünümü, günün saati, simülasyon hızı,
gölge, istatistik/yardım katmanları, metin boyutu, ana ses düzeyi, kamera hızı,
fare hassasiyeti ve "şimdi kaydet"/"dosyadan geri yükle"dir. Tüm metin, kendi C17
bitmap fontumuzla (5×7 glif, ASCII 32..126) çizilir; font varlığı veya üçüncü
taraf metin kütüphanesi yoktur (ADR-008). HUD yalnız sunum tamponuna yazar;
dünyaya veya render sahnesine asla yazmaz.

### Dünya seçimi — `src/ui/ws_session.{h,c}`, `src/ui/ws_hud.c`, `src/app/ws_live.{h,c}`

`docs/17` "Ana deneyim" maddesinin uygulaması: "uygulama açıldığında kullanıcı
yeni dünya oluşturabilir, mevcut dünyayı yükleyebilir veya örnek dünyayı
açabilir." Uygulama açılışında modal bir dünya seçim ekranı gösterilir (`M` ile
yeniden açılır): üç satır — **yeni dünya oluştur**, **mevcut dünyayı yükle**,
**örnek dünyayı aç**. `↑/↓` (veya `W/S`) imleci taşır, `Enter` seçilen dünyayı
açar, `M`/`Esc` seçimi iptal eder (geçerli dünya korunur). İmleç varsayılan olarak
kayıt varsa "yükle" satırına, yoksa "yeni" satırına gelir.

- **Yeni dünya** — çağıranın verdiği taze bir tohumla dünyayı sıfırdan kurar
  (tick 0, boş olay zaman çizelgesi, boş replay kaydı). Uygulama tohumu duvar
  saatinden türetir (`ws_live_new_world_seed`), böylece her oturumda dünya
  farklıdır; oturum yine de verilen tohumdan deterministik kurar.
- **Mevcut dünyayı yükle** — kayıtlı checkpoint'i bit-özdeş geri yükler
  (`ws_session_load`); kayıt yoksa satır seçilemez ve onaylama temiz bir
  `WS_ERR_NOT_FOUND` döner.
- **Örnek dünyayı aç** — sabit `WS_SESSION_SAMPLE_SEED` tohumuyla örnek dünyayı
  kurar; aynı tohum + tick sayısı bit-özdeş özet verir (deterministik).

Dünyayı yeniden kurma tek bir yerde toplanır (`world_reinit`): yeni dünya ve
örnek dünya aynı yolu paylaşır, böylece tutarlı kalır. Seçim ekranının kendisi
gözlemci salt-okunurdur: açıp kapatmak (onaylamadan) dünya özetini değiştirmez.
Seçim gerçekleşince oturum yeni dünyayı gösterir; ekran kapanır.

### Kalıcı ayarlar — `src/platform/ws_settings.{h,c}`

`docs/17` madde 9'un (ayarlar) kalıcı yolunu sağlayan, bağımsız C17 anahtar=değer
metin biçimi. `ws_settings_default`, `ws_settings_parse`, `ws_settings_format`,
`ws_settings_load`, `ws_settings_save` işlevleri çözünürlük, iç render ölçeği,
başlangıç görünümü, başlangıç saati, gölge/tam ekran bayrakları ve tohumu
saklar; bilinmeyen anahtarlar ve bozuk satırlar yok sayılır, aralık dışı değerler
kırpılır. Uygulama açılışta dosyayı okur (açık komut satırı bayrağı her zaman
dosyayı geçersiz kılar) ve pencere temiz kapanışta gözlemcinin son seçimlerini
yazar. Etkileşimli ayar menüsü de aynı dosyaya yazar ("şimdi kaydet" satırı) ve
"dosyadan geri yükle" satırıyla yeniden okur. Headless/tur kanıt koşuları dosyayı
ne okur ne yazar; kanıtlar geliştirici ayarından bağımsızdır.
`ws_session_apply_settings` / `ws_session_get_settings` bu anlık görüntüyü
gözlemciye uygular; dünyaya dokunmadığı için aynı tohum ve tick sayısında özet
bit-özdeş kalır. Menünün ses düzeyi satırı kendi C17 ses karıştırıcımızın ana
kazancını (`ws_audio_set_master_gain`) ölçekler; kamera hızı ve fare hassasiyeti
satırları serbest kamera hareketini/hassasiyetini çarpan olarak etkiler.

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

### Serbest kamera yer çarpışması — `src/ui/ws_session.{h,c}`

`docs/17` "Kamera kontrolleri" maddesinin kamera çarpışması/fiziksel engel
farkındalığı kısmının uygulaması. Serbest kameranın gözü, altındaki arazi
yüksekliğinin `WS_SESSION_CAM_CLEARANCE_M` (1.5 m) üzerinde tutulur ve su yüzeyinin
altına inmez; arazi yüksekliği ve su seviyesi dünya konumunun saf fonksiyonları
olduğundan (`ws_terrain_height_at`) kamera odak yamağının dışına çıksa bile yere
gömülmez. Bu davranış ayarlar menüsündeki "Camera collision" satırıyla açılıp
kapatılabilir (varsayılan açık); kapalıyken kullanıcı inceleme için yere inebilir ama
göz mutlak datanın üstünde kalır. Kalıcı ayarlara eklendi (`cam_collision`). Gözlemci
salt okunurdur: kamera hareketi dünya özetini değiştirmez (`tests/test_session.c`
`session_camera_terrain_collision`).

### Dışa aktarma — `src/app/ws_live.{h,c}`, `src/ui/ws_session.{h,c}`

`docs/17` "Dışa aktarma" maddesinin uygulaması: zaman damgalı kare dizisi ve eşleşen
WAV ses çıktısı. `worldseed.exe --live-export <dizin> [--live-export-seconds <s>]`
canlı ürünü sabit 1/30 s kare aralığıyla koşar, her kareyi kendi CPU referans
renderer'ımızla çizer ve HUD'ı bindirip `frame-NNNNN.ppm` olarak yazar. Aynı aralık
için oturumun canlı prosedürel ses manzarası `ws_session_render_audio` ile karıştırılır
(48 kHz, stereo, int16) ve karelerle hizalanan tek bir `soundscape.wav` üretilir; WAV
oturumun **gerçek** yaşam olaylarından sürülür (sabit kayıt değil). `frames-index.txt`
her karenin dosya adını, dünya zamanını, tick'ini ve SHA-256'sını; `export-manifest.txt`
ise kare sayısını, aralığı, ses örnekleme hızını/kare sayısını ve dünyanın SHA-256
özetini kaydeder. Karıştırma yalnız ses durumunu ilerletir, dünyayı değiştirmez
(`tests/test_session.c` `session_audio_export_readonly`); aynı tohumla iki koşu
bit-özdeş kare dizisi ve bit-özdeş WAV üretir (`tests/test_live.c` `live_export_run`).

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
| `M` | Dünya seçim ekranı (yeni / mevcut / örnek) |
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

- **Deterministik tur** — `worldseed.exe --live-tour <dizin> --size WxH`: on altı adım
  (auto gündüz, serbest genel görünüm, geyik takibi, harita, alacakaranlık, tilki
  takibi, gece+istatistik, kaydet→ilerlet→yükle, gölgeler kapalı, gölgeler açık, tek
  adım, ayarlar menüsü, dünya seçimi, fare ile menü seçimi, kamera çarpışması, ağaç
  kaçınma). Her
  adım gerçek PPM karesi + SHA-256 ve manifest yazar. Bu makinede (1280×720):

  ```
  tour step=00 auto_day     sha256=e31c82ce...  coverage=1.0000
  tour step=02 follow_deer  sha256=da72f291...
  tour step=06 auto_night   sha256=e02eb50c...
  tour save_load saved=1 loaded=1 roundtrip=1
  tour single_step paused=1 from=580 to=585 ok=1
  tour menu_click settings_row=1 moved=1 activated=1 world_row=1 world_moved=1 readonly=1
  tour camera_collision on=1 eye_y=10.268 ground=8.768 clearance=1.500 ok=1 readonly=1
  tour tree_avoidance obstacles=351 min_gap=2.441 ok=1 readonly=1
  LIVE_TOUR_OK steps=16
  ```

  `roundtrip=1`, checkpoint kaydedildikten sonra dünyanın ilerletilip geri
  yüklendiğinde SHA-256 özetinin birebir aynı olduğunu kanıtlar. `single_step ok=1`,
  durakdayken beş tek adımın dünyayı tam beş tick ilerlettiğini ve duraklamayı
  bozmadığını kanıtlar (`manifest` içinde `single_step_verified=1`). `menu_click`,
  fareyle bir menü satırının seçildiğini (`moved=1`), ikinci tıklamanın satırı
  uyguladığını (`activated=1`), dünya seçim satırının da tıklamayla seçildiğini
  (`world_moved=1`) ve tüm bunların dünyayı değiştirmediğini (`readonly=1`) doğrular
  (`manifest` içinde `menu_click_verified=1`). `camera_collision`, serbest kamera
  yere doğru sürüldüğünde gözün arazinin `WS_SESSION_CAM_CLEARANCE_M` (1.5 m)
  üzerinde durduğunu (`ok=1`) ve dünyayı değiştirmediğini (`readonly=1`) doğrular
  (`manifest` içinde `camera_collision_verified=1`). `tree_avoidance`, kamera gerçek
  bir ağaç gövdesine doğru sürüldüğünde gözün gövdenin `radius + 0.6 m` koruma
  yarıçapının dışında kaldığını (`ok=1`, bu makinede `min_gap=2.441`) ve dünyayı
  değiştirmediğini (`readonly=1`) doğrular (`manifest` içinde
  `tree_avoidance_verified=1`).

- **Dünya seçimi kanıtı** — `worldseed.exe --live-select <dizin> --size WxH`
  (docs/17 "Ana deneyim": yeni dünya / mevcut dünyayı yükle / örnek dünyayı aç):
  dünya bir süre ilerletilir ve bir checkpoint kaydedilir, ardından seçim ekranı
  açılır. Örnek dünya sabit bir tohumla (`WS_SESSION_SAMPLE_SEED`) yeniden
  kurulur ve **bit-özdeş** olarak yeniden üretilir (`sample_reproducible=1`);
  kaydedilmiş dünya seçimi checkpoint'i bit-özdeş geri yükler (`load_restored=1`);
  yeni dünya sıfır tick ile başlar (`new_fresh=1`); seçim ekranını açıp kapatmak
  dünyayı değiştirmez (`readonly=1`). `LIVE_SELECT_OK` çıktısı ve
  `select-manifest.txt` CI kapısıdır. Seçim ekranı gerçek pencerede uygulama
  açılışında gösterilir ve izole Windows koşucusunda ekran görüntüsü alınır
  (`evidence/gui/app/world-select-window.png`).

- **Replay kanıtı** — `worldseed.exe --live-replay <dizin> --size WxH`: dünya
  bir süre ilerletilir, replay'e girilir, en eski örnek, adımlanmış örnek ve en
  yeni örnek gerçek PPM karesi olarak yazılır; canlı dünya geri yüklendikten sonra
  devam ettiği doğrulanır. Manifest `newest_matches_live=1` (en yeni kayıtlı örnek
  canlı dünyayı bit-özdeş yeniden üretir) ve `live_restored=1` (replay'den çıkış
  canlı dünyayı bit-özdeş geri yükler) yazar. `LIVE_REPLAY_OK` çıktısı CI kapısıdır.

- **Dışa aktarma kanıtı** — `worldseed.exe --live-export <dizin> [--live-export-seconds <s>]`
  (docs/17 "Dışa aktarma"): canlı ürün 1/30 s kare aralığıyla koşar, her kareyi kendi
  CPU referans renderer'ımızla çizip `frame-NNNNN.ppm` olarak yazar ve aynı aralık için
  canlı prosedürel ses manzarasını karelerle hizalanan tek bir `soundscape.wav` olarak
  karıştırır. `frames-index.txt` her karenin dünya zamanını, tick'ini ve SHA-256'sını;
  `export-manifest.txt` kare sayısını, aralığı, ses örnekleme hızını/kare sayısını ve
  dünyanın SHA-256 özetini kaydeder. Bu makinede (320×180, 1 s):

  ```
  LIVE_EXPORT_OK frames=30 wav=.../soundscape.wav frames=48000 nonzero_audio=1
  ```

  Aynı tohumla iki dışa aktarma koşusu bit-özdeş kare dizisi ve bit-özdeş WAV üretir
  (`cmp` her `frame-*.ppm` ve `soundscape.wav` için başarılı); `ci.yml`,
  `gui-windows.yml` ve `tools/package-windows.ps1` bu eşitliği zorunlu kapı yapar.
  Ses karıştırmanın dünyayı değiştirmediği `tests/test_session.c`
  `session_audio_export_readonly` ile, dışa aktarma koşusunun uçtan uca çalıştığı
  `tests/test_live.c` `live_export_run` ile doğrulanır.

- **Tur tekrarlanabilirliği** — Aynı parametrelerle iki tur karesi bit-özdeş olur
  (`cmp` her `step-*.ppm` için başarılı). Bu, "gözlemci salt okunurdur" kuralını
  gerçek çıktı üzerinden doğrular; CI bu karşılaştırmayı zorunlu kapı yapar.

- **Birim testleri** — `tests/test_font.c`, `tests/test_hud.c`, `tests/test_session.c`,
  `tests/test_live.c`, `tests/test_settings.c` (toplam 47 test): font çizimi/metrikleri,
  HUD salt-okunurluk ve panel çizimi (istatistik + ayar + replay + dünya seçimi panelleri
  dahil), oturum yaşam döngüsü, duraklatma, gözlemci salt-okunurluğu, tohum determinizmi,
  kare-hızı bağımsızlığı, seçim/inceleme, zaman çizelgesi/harita, kaydet/yükle,
  istatistik/demografi, görünüm kipleri, replay (giriş/arama/adımlama/çıkış ve
  salt-okunurluk), dünya seçimi (yeni/örnek/yükle + salt-okunurluk + determinizm),
  canlı uygulama seçenek ayrıştırma + headless/tur/replay/dünya-seçimi koşuları,
  ayar ayrıştırma/biçimlendirme/kaydet-yükle ve CLI↔dosya önceliği.

- **Kanıt manifesti aracı** — `tools/ws_manifest.c`: paketteki her dosya için
  SHA-256 ve boyut yazar; eksik dosya varsa sıfırdan farklı çıkış verir, böylece
  sessizce düşen bir artefakt kapıyı geçemez. `manifest_ok` ve `manifest_missing`
  testleri bu davranışı doğrular.

## CI ve release

- `ci.yml`: Linux sanitizer matrisi ve Windows MSVC işlerine "Live application
  evidence" adımı eklendi — headless koşu (`LIVE_HEADLESS_OK`), tur
  (`LIVE_TOUR_OK`, `save_load_roundtrip=1`), dünya seçimi (`LIVE_SELECT_OK`) ve iki
  turun kare-özdeş olduğu `cmp` karşılaştırması. Kanıtlar artefakt olarak yüklenir.
- `gui-windows.yml`: izole Windows runner'da gerçek canlı pencere açılır
  (`--live --size 1280x720`), masaüstü ekran görüntüsü alınır, pencere kapatılır;
  ayrıca dünya seçimi ekranı gerçek pencerede açılıp görüntülenir
  (`--live --size 1280x720`, `world-select-window.png`); headless + tur + dünya
  seçimi + replay kanıtı alınır. Bu, yalnız Windows'ta çalışan Win32/GDI sunum
  yolunu gerçek pencerede sınar.
- `release.yml`: paket artık canlı tur karelerini (`live/*.png`, `live/manifest.txt`)
  içerir; `worldseed.exe --version` ile sürüm bildirir; `tools/ws_manifest` ile
  `MANIFEST.txt` üretilir; sürüm notu P11 canlı uygulamasını ve kalan sınırlamaları
  açıklar.
- Sürüm: `CMakeLists.txt` proje sürümü ve `src/core/ws_version.h` `0.19.0-preview`
  olarak hizalandı (serbest kamera yer çarpışması dilimi); `worldseed.exe --version`
  aynı dizeyi yazdırır.

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
  çözünürlük, iç render ölçeği, başlangıç görünümü/saati, gölge/tam ekran, tohum,
  metin boyutu, ana ses düzeyi, kamera hızı, fare hassasiyeti ve kamera çarpışması.
  Açık komut satırı bayrağı dosyayı geçersiz kılar; pencere temiz kapanışta dosyayı
  yazar.
  Etkileşimli ayar menüsü (`O`, `docs/17` madde 9) bu dilimde tamamlandı: menü
  gezilebilir ve değiştirilebilir, kaydet/geri yükle satırları dosyayı kullanır,
  çözünürlük satırı pencereyi yeniden boyutlandırır (tam ekranda değil). Dünya
  seçimi (`docs/17` "Ana deneyim") bu dilimde tamamlandı: uygulama açılışında yeni
  dünya / mevcut dünyayı yükle / örnek dünyayı aç seçim ekranı gösterilir. Menü ve
  seçim satırlarında fareyle tıklayarak seçme bu dilimde tamamlandı (`docs/17`
  madde 9): bir sol tıklama üzerine geldiği satırı seçer, vurgulanan satıra ikinci
  tıklama satırı klavyedeki `Enter` gibi uygular; HUD kendi çizdiği geometriyle
  vuruş sınaması yapar, oturum API'si (`ws_session_settings_select_row`,
  `ws_session_world_select_row`) platformdan bağımsız ve test edilebilirdir.
- `1.0.0` yalnız `docs/00` bölüm 8'in bütün koşulları geçildiğinde yayımlanır.
