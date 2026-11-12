# CI, dal koruması ve runner düzeni

Bu belge ilk görevde kurulan geliştirme altyapısını ve tespit edilen erişim sınırını açıklar. Karar kaydı: ADR-009.

## İş akışları (`.github/workflows/`)

| Dosya | Ne yapar | Nerede çalışır |
|---|---|---|
| `ci.yml` | Derleme + birim testleri (GCC/Clang × Debug/Release × sanitizer), MSVC Windows derleme/test, plan bütünlüğü, README bağlantı kontrolü | GitHub-hosted (ubuntu, windows) |
| `gui-windows.yml` | Gerçek Windows GUI/headless smoke + tur yeniden üretimi + yönlü gölge farkı + native 4K referans-sahne perf + **native 4K canlı ürün döngüsü GPU benchmark'ı** (P12); izole, secretsiz, allowlist'li self-hosted runner | Self-hosted `worldseed-gui` (yalnız `main` ve aynı depo PR'ları) |
| `release.yml` | Plan kapısı + Windows x64 paketleme (hosted birincil, self-hosted Windows yedek; aynı `tools/package-windows.ps1`) + prerelease yayını | GitHub-hosted + release; yedek yol self-hosted `worldseed-gui` |

## Zorunlu kapılar

- Zorunlu kontrollerden biri fail/skipped/unknown ise merge edilmez.
- `main`'e doğrudan push yok; kısa ömürlü dal + PR.
- Aynı anda en fazla bir ana PR açık.
- GUI iş akışı yalnız güvenilir ref (`main`) için çalışır; fork/harici PR kodu izole runner'da çalıştırılmaz.

## Branch protection kurulumu (yönetici erişimi gerektirir)

Depo private ve klasik branch protection / ruleset API'si mevcut token için 403 döndürüyor. Yönetici yetkili erişim sağlandığında aşağıdaki ayarlar uygulanır:

1. `main` için: PR zorunlu, en az 1 onay, "Require status checks to pass" açık, zorunlu kontroller = `ci.yml` işleri (`linux-matrix`, `windows-msvc`, `plan-integrity`, `docs-links`).
2. "Require branches to be up to date before merging" açık.
3. Doğrudan push ve force-push kapalı; dal silme sınırlı.
4. "Require conversation resolution before merging" açık.

Ayar komutu (yönetici token'ı ile):

```sh
gh api -X PUT repos/navoelyr-lwe/worldseed/branches/main/protection --input - <<'JSON'
{
  "required_status_checks": {"strict": true, "contexts": ["plan integrity", "docs links", "linux gcc Debug on"]},
  "enforce_admins": true,
  "required_pull_request_reviews": {"required_approving_review_count": 1},
  "restrictions": null
}
JSON
```

**Son doğrulama (2026-11-12):** `main` için klasik branch-protection okuması (`GET .../branches/main/protection`) ve ruleset okuma/yazma (`GET`/`POST .../rulesets`) uçları hâlâ `403 Resource not accessible by integration` döndürüyor; ayarlar kurulamadı. Bu, yapılmış gibi işaretlenmez. Koruma, yönetici erişimi sağlanana kadar PR akışı + zorunlu workflow kapılarıyla uygulanır.

## İzole Windows GUI runner gereksinimleri

- Disposable, izole Windows VM; kalıcı kullanıcı verisi yok.
- Depo sırları (secrets) runner'da bulunmaz.
- Yalnız bu depo ve güvenilir ref için allowlist'li.
- Ağ erişimi en aza indirilir; fork/harici PR kodu asla çalıştırılmaz.
- Runner yoksa GUI işi "çalıştı" sayılmaz; gerçek pencere testi runner kaydıyla başlar.

## GUI testinin kapsamı (P2)

`gui-windows.yml` runner kayıtlı olduğunda şunları yapar:

1. Windows x64 Release derlemesi ve tüm birim testleri.
2. `ws_smoke` ile penceresiz smoke testi (`WS_SMOKE_OK`).
3. `worldseed.exe --selftest` ile altı sahnenin render doğrulaması (`APP_SELFTEST_OK`).
4. Gerçek Win32 penceresini açar, 5 saniye bekler, masaüstü ekran görüntüsünü
   `evidence/gui/app/window.png` olarak kaydeder ve pencereyi kapatır.
5. `ws_perf` ile native 3840×2160 **referans sahne** mikro-benchmark'ı (p50/p95/p99).
6. `worldseed.exe --live-bench 600 --live-gpu --size 3840x2160` ile native 4K
   **canlı ürün döngüsü** GPU benchmark'ı: gerçek dünya adımı + ekoloji + yaşayan
   aktörler + kamera + render + HUD, en az 10 dakika **gerçek duvar saati**;
   `path=gpu`, `cpu_fallback_frames=0`, `gpu_errors=0` zorunludur (cihaz her kareyi
   çizmezse adım başarısız olur) ve p95 kabul değeri (`p95_ok`) raporlanır. Ham kare
   zamanları `evidence/gui/live/gpu-4k/frames.csv` içindedir. Bu, 5. maddedeki
   referans-sahne ölçümünden ayrıdır: P12 kabul eşiği bu canlı ölçümle değerlendirilir.

Bu, gerçek pencereyi çalıştıran ve gerçek görüntü üreten bir GUI testidir; yalnız
headless çalıştırma değildir. Runner kayıtlı değilken iş akışı manueldir ve hiçbir
GUI testi "çalıştı" olarak raporlanmaz.

## Erişim sınırı özeti

- Çalışan: dal oluşturma, push, PR açma, PR birleştirme.
- Kısıtlı (403): branch protection, ruleset, Actions izinleri, repo ayarları.
- Bu nedenle koruma, yönetici erişimi sağlanana kadar PR akışı + workflow kapılarıyla uygulanır.

## Geçmiş altyapı olayı: GitHub-hosted runner erişimi (2026-11-10)

2026-11-10'da bu depoda **tüm GitHub-hosted işler sıfır adımla anında düşüyor**
(`runner_id=0`, hiçbir adım çalışmıyor). Durum bağımsız olarak doğrulandı:

- `main` dalındaki değişmemiş kodda da aynı imza oluşuyor (önceki başarılı `main`
  koşusu 19:55, sonraki koşular 20:30+ hepsi anında fail).
- Tek adımlı minimal bir `echo` iş akışı da tüm hosted etiketlerde
  (`ubuntu-latest`, `ubuntu-22.04`, `ubuntu-24.04`, `windows-2022`) sıfır adımla
  düşüyor.
- Self-hosted işler normal çalışıyor: izole Windows GUI işi (adım 1-12) ve
  self-hosted etiketli bir probe işi başarıyla tamamlandı; self-hosted Linux
  etiketi kuyrukta kaldı (kayıtlı runner yok).
- GitHub durum sayfası kesinti göstermiyor; bu, hesap/depo düzeyinde bir runner
  erişim kısıtı (ör. Actions dakika/spending limiti veya hosted runner izni).

Bu durum 10 Kasım 2026 tarihli bir olay kaydıdır; kalıcı/güncel runner durumu olarak okunmamalıdır. 11 Kasım 2026'da `main` commit'i `f6d15ed8c54ef51183626aafb565ff817d0b45c0` için CI run `42974024398` başarıyla tamamlandı. Bu çalıştırmada Linux GCC/Clang Debug/Release, Windows MSVC Debug/Release, OpenCL CPU kernel, plan bütünlüğü ve doküman bağlantısı işleri başarılıdır. Aynı commit için izole Windows GUI run `42974024303` ve release run `42974038527` da başarılıdır.

Bu nedenle 11 Kasım 2026 denetim anındaki gözlem: GitHub-hosted CI işleri yeniden çalışır görünmektedir; ancak tek bir başarılı run gelecekteki tüm runner erişimini garanti etmez. Yeni workflow koşuları gerçek sonuçlarına göre değerlendirilmelidir. Önceki 10 Kasım kesintisi tarihsel kayıt olarak korunur.

P12 gölge kanıtı ve release paketleme için izole Windows self-hosted yedek yolu da korunur. Branch protection/ruleset erişimi ayrı konudur: bağlı entegrasyonla branch-protection okuması bu denetimde 403 döndürdü; sunucu tarafı koruma ayarları etkin mi bu denetimde doğrulanamadı. Yetki sağlanmadan ayarların kurulmuş veya doğrulanmış olduğu varsayılmamalıdır.

## Güncel izole Windows GUI durumu (2026-11-12)

`main` commit'i `333005e890f273e6cd388d786f9b3e1eb34f3707` (PR #66) için:

- CI run `43191597724`: **başarılı**.
- İzole Windows GUI run `43191597624`: **başarılı**; `gui-smoke` artefaktı ID
  `13936337466` üretildi ve içindeki gerçek GPU kanıtı doğrulandı:
  - `evidence/gui/gpu/gpu-check.txt`: cihaz `NVIDIA GeForce RTX 3060 Laptop GPU`
    (OpenCL 3.0 CUDA, 30 birim); CPU referansı 96 üçgen 256×192 ok; CPU/GPU
    `semantic_match=ok` (`diff_pixels=3`, `diff_fraction=0.000061`,
    `max_channel_delta=1`); döşemeli çekirdek `tiled bit_identical=1`
    (`tiled_ms=0.73`); `device_loss_recovered=1` ve CPU fallback ok.
  - `evidence/gui/perf/perf-4k.txt`: native 3840×2160 **referans sahne** ölçümü
    (CPU referans yolu).
- Bu koşu, 2026-11-12'de kaydedilen "runner lost communication" kesintisinin
  ardından runner'ın yeniden çalıştığını gösterir; kesinti geçmiş olay kaydıdır.

Not: yukarıdaki GPU kanıtı **referans sahne** mikro-benchmark'ıdır; P12 kabul
eşiği artık canlı ürün döngüsü ölçümüyle (`--live-bench`, bu PR) değerlendirilir.

## Canlı ürün döngüsü GPU kabulü (2026-11-12, PR #67)

`gui-windows.yml` artık native 4K canlı ürün döngüsünü zorunlu adım olarak ölçer.
PR #67 dalı `feat/live-loop-gpu-benchmark` için:

- CI run `43209725312`: **başarılı**.
- İzole Windows GUI run `43209725164`: **başarılı**; adım 18 "Live product loop
  GPU benchmark on isolated runner" geçti. `evidence/gui/live/gpu-4k-live.txt`:
  - `device="NVIDIA GeForce RTX 3060 Laptop GPU" version="OpenCL 3.0 CUDA"
    units=30 global_mem_gb=6.0`, 16 CPU iş parçacığı, Windows.
  - `3840×2160 seed=20260101 gpu_render=1 shadows=0 duration_s=600`.
  - **`path=gpu frames=30730 wall_s=600.01 avg_ms=19.53 p95_ms=21.96 p99_ms=25.89`
    (51,2 FPS), `gpu_frames=30730 cpu_fallback_frames=0 gpu_errors=0`.**
  - `p95_ok=1` — native 4K/30 FPS hedefi (p95 ≤ 33,3 ms) bu cihazda karşılanır.
  - Ham kare zamanları: `evidence/gui/live/gpu-4k/frames.csv` (30730 satır).
- Adım, `path=gpu`, `cpu_fallback_frames=0`, `gpu_errors=0` ve `p95_ok=1`
  koşullarını zorunlu geçit yapar; cihaz her kareyi çizmezse veya p95 hedefi
  tutmazsa iş düşer. Bir CPU ölçümü asla GPU sonucu olarak raporlanmaz.
- Sınır: ölçüm `shadows=0` ve 6 GB cihazda alındı; 12 GB sınıfı referans donanım
  ve gölge-açık canlı ölçüm ayrıca üretilmelidir.
