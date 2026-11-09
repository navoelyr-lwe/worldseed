# ADR-009 — CI, dal koruması ve izole GUI runner politikası

**Durum:** Kapalı (kabul edildi)

## Bağlam

Kök `AGENTS.md` ve `12-TEST-KABUL-CI.md`; `main` koruması, PR zorunluluğu, GitHub-hosted runner'larda derleme/test ve gerçek Windows GUI testleri için izole, secretsiz, allowlist'li self-hosted runner gerektirir. Bu ilk görevde altyapı kurulmalıdır.

## Karar

- `main`'e doğrudan push yok; her değişiklik kısa ömürlü dal + PR ile gelir.
- Aynı anda en fazla bir ana PR açık tutulur.
- Zorunlu kontrollerden biri fail/skipped/unknown ise merge edilmez.
- PR derleme, birim ve başsız (headless) testler GitHub-hosted runner'larda çalışır.
- Gerçek Windows GUI testleri ayrı, izole, secretsiz ve allowlist'li self-hosted Windows runner'da çalışır; fork/harici PR kodu bu runner'da çalıştırılmaz.
- Release akışı aynı commit üzerinde derleme, test, paketleme ve smoke testleri doğrular.

## Erişim sınırı (ilk görevde tespit edildi)

İlk görevde kullanılan GitHub entegrasyon token'ı ile:
- Dal (branch) oluşturma, push ve PR açma **çalışır**.
- `main` branch protection, repository ruleset ve Actions izin ayarları API'leri **403 (Resource not accessible by integration)** döndürür; token yalnız kod içeriği yazma yetkisine sahiptir.

Bu nedenle branch protection/ruleset ve Actions izinleri bu token ile otomatik kurulamaz. Uygulanan güvenli alternatifler:
- Koruma sağlanana kadar değişiklikler yalnız PR üzerinden ilerletilir ve doğrudan `main`'e push yapılmaz.
- Zorunlu kontrol kapıları workflow tanımlarıyla uygulanır.
- Erişim sınırı `docs/CI-VE-DAL-KORUMASI.md` içinde sade dille raporlanır.

## Sonuç

- Workflow'lar bu depoya eklenir ve PR'larda çalışır.
- Branch protection kurulumu, yönetici yetkili erişim sağlandığında tek adımda tamamlanacak şekilde belgelenir.

## Geriye dönük uyumluluk

Korumasız aralıkta tek-yazarlı PR akışı ve zorunlu kontrol kapıları korunur; ürün kodu eklenmeden önce en az PR/CI kapıları uygulanır.
