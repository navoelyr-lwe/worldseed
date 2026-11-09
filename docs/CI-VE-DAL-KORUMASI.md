# CI, dal koruması ve runner düzeni

Bu belge ilk görevde kurulan geliştirme altyapısını ve tespit edilen erişim sınırını açıklar. Karar kaydı: ADR-009.

## İş akışları (`.github/workflows/`)

| Dosya | Ne yapar | Nerede çalışır |
|---|---|---|
| `ci.yml` | Derleme + birim testleri (GCC/Clang × Debug/Release × sanitizer), MSVC Windows derleme/test, plan bütünlüğü, README bağlantı kontrolü | GitHub-hosted (ubuntu, windows) |
| `gui-windows.yml` | Gerçek Windows GUI/headless smoke; izole, secretsiz, allowlist'li self-hosted runner | Self-hosted `worldseed-gui` (yalnız `main`) |
| `release.yml` | Plan kapısı + Windows x64 paketleme + prerelease yayını | GitHub-hosted + release |

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

Bu, gerçek pencereyi çalıştıran ve gerçek görüntü üreten bir GUI testidir; yalnız
headless çalıştırma değildir. Runner kayıtlı değilken iş akışı manueldir ve hiçbir
GUI testi "çalıştı" olarak raporlanmaz.

## Erişim sınırı özeti

- Çalışan: dal oluşturma, push, PR açma, PR birleştirme.
- Kısıtlı (403): branch protection, ruleset, Actions izinleri, repo ayarları.
- Bu nedenle koruma, yönetici erişimi sağlanana kadar PR akışı + workflow kapılarıyla uygulanır.
