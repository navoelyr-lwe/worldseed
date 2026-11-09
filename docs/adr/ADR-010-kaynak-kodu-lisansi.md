# ADR-010 — Kaynak kodu lisansı ve içerik menşei kaydı

**Durum:** Kapalı (kabul edildi)

## Bağlam

`18-GUVENLIK-LISANS-PAKET.md` eski sürümünde "C kaynak kodu için lisans kararı proje sahibi tarafından yayın öncesi verilir" ifadesi vardı. Bu, teknik soru sorma yasağı ve yayımlama yetkisi devri ile çelişiyordu.

## Karar

- Depo ve C17 kaynak kodu için güvenli varsayılan: **tüm hakları saklıdır** (açık kaynak lisansı verilmez). Bu, proje sahibinin haklarını koruyan en ihtiyatlı seçimdir ve kimseye lisans/dağıtım hakkı vermez.
- Bir açık kaynak veya ticari dağıtım lisansı ancak proje sahibi açıkça belirtirse `LICENSE` dosyasıyla sabitlenir. OpenHands varsayılan olarak yeni bir lisans vermez.
- Teknik soru yöneltilmez: varsayılan lisans politikası bu belgede sabittir.
- İlk release içeriği kendi C17 varlık hattımızda üretilir (bkz. ADR-008). Bu nedenle harici içerik lisans allowlist'i gerekmez.
- Bilimsel parametre ve ölçüm verileri model kartlarında kaynak, DOI/kalıcı bağlantı, erişim tarihi ve veri lisansı ile kaydedilir; ürüne varlık olarak gömülmez.
- Varsayılan runtime bağımlılık politikası: üçüncü taraf engine ve runtime library yok.
- OS API ve sürücü ayrı platform altyapısıdır; uygulama bunları yeniden dağıtmaz veya değiştirmez.

## Gerekçe

- Yayımlama ve teknik karar yetkisini OpenHands'e devreden üst kararlarla tutarlıdır.
- Lisans seçimi bir mühendislik/idari işlemdir; kullanıcıya teknik soru yöneltilmesini gerektirmez.

## Sonuç

- `18-GUVENLIK-LISANS-PAKET.md` lisans bölümü bu kararla güncellendi.

## Geriye dönük uyumluluk

Lisans seçimi dağıtımdan önce yapılır; ilk görevde kod dağıtımı olmadığından bu karar ilk release paketlemesinde uygulanır.
