# 10 — Ses, kayıt ve tekrar oynatma

## Ses

- C ile PCM üretimi, karıştırma ve ses kaynak modeli.
- Çevresel ses: rüzgâr, yağmur, su, ortam katmanı.
- Canlı sesleri: tür, davranış ve olayla ilişkili.
- Mesafe, yön, engel ve ortam akustiğine bağlı mekânsal sunum.
- Platform aygıt çıkışı ince adaptör arkasında.
- Ses aygıtı kaybı/taşması simülasyon adımını bloke etmez.
- Ses örnekleri/asset'leri kullanılırsa kaynağı ve lisansı kaydedilir.

## Kalıcılık

Dünya kaydı en az şunları tutar:

- Dünya UID ve seed.
- Dünya şema sürümü ve üretici sürümleri.
- Bölge kimlikleri, prosedürel üretici sürümleri ve yaşanmış değişiklikler.
- Canlı/nesne kalıcı kimlikleri ve durumları.
- RNG stream durumu/counter bilgisi.
- Simülasyon zamanı ve çevresel durum.
- Ekolojik/bitki/kaynak durumu.
- Model sürümleri ve gerekli parametre seti.
- Checkpoint ve olay günlüğü konumu.

Ham C struct diske yazılmaz. Sürümlü, boyut sınırlı ve açık endian/alan biçimi kullanılır. Kayıt atomik yazılır; bozuk/kısmi kayıt test edilir. Geri yükleme doğrulama ve migration uygular.

## Replay

- Checkpoint + deterministik olay/komut günlüğü.
- Aynı build/seed/parametre koşusunda tekrarlanabilirlik.
- Model ve içerik sürüm değişikliklerinde uyum/migration raporu.
- Kamera ve yönetmen replay'i simülasyon state'inden ayrı.
- Replay çıktısı gerçek geçmiş durumunu gösterir; olay yaratmaz.

## Uzun süre ve kapanma

- Dünya host'u görüntüleyiciden ayrılabilir hale tasarlanır.
- Kullanıcı görüntüleyiciyi kapatsa bile host açık kalması sonraki ürün aşamasında desteklenebilir.
- Bilgisayar uykuya veya kapalı duruma geçtiğinde gerçek simülasyon ilerlediği söylenmez.
- Açılışta son checkpoint yüklenir; çevrimdışı geçen süre için yaklaşık zaman telafisi ayrı ve açık bir model kararıdır.
