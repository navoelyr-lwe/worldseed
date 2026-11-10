# Model kartı — Prosedürel ses manzarası

1. **Kimlik / sürüm / modül:** `audio.soundscape` v1; `src/audio/ws_audio.c`,
   `src/audio/ws_audio.h`. Olay taşıyıcı: `src/life/ws_life_event.*`. Sahne
   köprüsü: `src/render/ws_env_scene.c` (`soundscape`, scene_kind 9). Kanıt aracı:
   `tools/ws_audio.c`.
2. **Temsil edilen süreç ve amaç:** `10-SES-KALICILIK-TEKRAR.md` ve
   `09-SINEMATIK-YONETMEN.md` uyarınca, kayıtlı örnek veya üçüncü taraf ses çalışma
   zamanı olmadan, dünyanın gerçek olaylarından türeyen prosedürel ses. Amaç, her
   gerçek olayın duyulabilir bir karşılığı olması ve aynı koşunun aynı sesi
   üretmesidir; ses hiçbir olay uydurmaz.
3. **Ölçek:** Örnekleme `WS_AUDIO_SAMPLE_RATE = 48000` Hz, interleaved stereo,
   16-bit PCM. Zaman: çağıran tarafından verilen dünya zamanı `time_s` (s) ve kare
   sayısı. Ses havuzu `WS_AUDIO_MAX_VOICES = 32` eşzamanlı ses.
4. **Durum değişkenleri:** ses havuzu (`ws_audio_voice`: etkin, faz, frekans, zarf
   düzeyi, tepe düzey, azalma, pan); tür-başına son tetikleme zamanı
   (`last_trigger_s[4]`); sayaçlar (`events_played`, `events_skipped`,
   `voices_dropped`, `samples_rendered`).
5. **Denklemler / algoritma (deterministik):**
   - **Olay-ses eşleme:** av 180 Hz/0.80/azalma 3.0, ölüm 120 Hz/0.70/2.0,
     doğum 660 Hz/0.50/5.0, otlama 320 Hz/0.30/8.0. Eşlemesi olmayan tür ses
     çıkarmaz ve sayılmaz.
   - **Tür-başına bekleme:** `time_s − last_trigger_s[kind] < cooldown` ise tetik
     bastırılır (`events_skipped++`); bekleme: av 0.8 s, ölüm 1.0 s, doğum 0.5 s,
     otlama 0.6 s. İlk olay her türde daima çalar (`last_trigger_s` negatif sentinel).
   - **Zarf:** atak `amp += amp_target·200/sr`, tepeye ~5 ms'de ulaşır; sonra
     `amp *= 1 − decay/sr`; `amp < 1e-4` olunca ses kapanır.
   - **Karışım:** her ses `s = amp·sin(phase)`; pan `gl=√((1−pan)/2)`,
     `gr=√((1+pan)/2)`; `phase += 2π·freq/sr` (mod 2π).
   - **Kazanç/doygunluk:** `out = master·x`, sonra yumuşak doygunluk
     `x/(1+|x|)`, `master = 0.32`; örnek `lround(out·32767)`.
   - **Mekânsal:** `pan`, olayın dünya x konumundan `(x/area)·2 − 1` ile türetilir.
6. **Varsayımlar / ihmal edilen süreçler:** Sesler temel sinüstür; harmonik/
   gürültü tabanlı zengin tını, ortam yankısı, engel/soğurma ve Doppler yoktur.
   Hoparlör konumu dinleyiciye sabittir. Rastgelelik yoktur.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** Frekans/düzey/azalma ve
   bekleme süreleri tasarım seçimidir; ölçülmüş akustik veriye kalibre
   edilmemiştir. Belirsizlik orta düzeydedir; algısal kalite testi (D3) yoktur.
8. **Sayısal yöntem, kararlılık:** Faz mod 2π'de tutulur; zarf doğrusal olmayan
   ama sınırlıdır. `master` kazancı ve `x/(1+|x|)` doygunluğu tam ölçeğe taşmayı
   ve sert kırpmayı önler; gerçek koşuda kırpılmış örnek yoktur (test edilir).
   Aynı olay dizisi bit-özdeş PCM üretir.
9. **Doğrulama planı:** `tests/test_cinema.c`: aynı olay dizisinin bit-özdeş PCM
   ürettiği (hash karşılaştırması); eşlemesi olmayan türün ses çıkarmadığı; gerçek
   sinyal (sıfır olmayan örnek) ve geçerli WAV başlığı; sesin gerçek ekoloji
   koşusundan sürüldüğü. `tests/test_world_env.c`: `soundscape` sahnesinin aynı
   tohumla aynı çubukları ve PCM hash'ini ürettiği. `tools/ws_audio` + CI: gerçek
   WAV üretimi, sinyal ve tekrarlanabilirlik (`sha256`).
10. **Kabul eşikleri ve sınırlar:** Karışım sessiz değildir (en az bir sıfır olmayan
    örnek); kırpılmış örnek yoktur; aynı girdi aynı PCM özetini verir. Bilinen
    sınırlar: temel sinüs tınısı, akustik ortam yok, algısal D3 yok. Kanıt düzeyi
    **D2** (determinizm ve sinyal testleri geçti).
11. **Değişiklik kaydı:** Ses eşleme tablosu, bekleme, kazanç veya karışım
    değişirse bu kart, `ws_audio.*`, ilgili testler ve
    `docs/P9-SINEMATIK-YONETMEN-VE-SES.md` birlikte güncellenir.
