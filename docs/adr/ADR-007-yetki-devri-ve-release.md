# ADR-007 — Yetki devri ve release yayımlama

**Durum:** Kapalı (kabul edildi)

## Bağlam

Proje planının bazı eski ifadeleri tag/release oluşturmanın "proje sahibi onayı" gerektirdiğini yazıyordu (`18-GUVENLIK-LISANS-PAKET.md`). Kök `AGENTS.md` ve `14-OPENHANDS-CALISMA-KURALLARI.md` ise yayımlama yetkisinin proje sahibi tarafından OpenHands'e devredildiğini belirtiyor. Bu iki ifade çelişiyordu.

## Karar

- Preview/stable release yayımlama yetkisi proje sahibi tarafından OpenHands'e devredilmiştir.
- OpenHands, tüm zorunlu kalite kapıları (`12-TEST-KABUL-CI.md`) ve `00-URUN-SOZLESMESI.md` bölüm 8 şartları geçtiğinde release/prerelease'i ayrıca onay sormadan yayımlar.
- `1.0.0` stable yalnız ürün sözleşmesindeki bütün koşullar geçince yayımlanır.
- Yalnız belge/altyapı işleri ürün release'i sayılmaz.
- Gizli anahtar/hesap yetkisi, ücretli hizmet satın alma veya GitHub dışı geri döndürülemez idari işlem gerektiren durumlarda OpenHands işlemi yapamaz; gerçek engeli sade dille raporlar.

## Sonuç

- `18-GUVENLIK-LISANS-PAKET.md` release politikası bölümü bu kararla güncellendi.
- Belgelerdeki "proje sahibi onayı gerekir" ifadeleri geçersizdir.

## Geriye dönük uyumluluk

Bu karar ürün kalite kapılarını düşürmez; yalnız yayımlama yetkisini netleştirir.
