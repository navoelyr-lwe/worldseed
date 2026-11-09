# Plan denetim raporu (ilk görev)

Bu rapor ilk görevin plan denetimini, bulunan çelişkileri ve yapılan düzeltmeleri kanıta bağlar. Referans: kök `AGENTS.md`, `docs/plan-bundle/RESTORE.md`.

## 1. Arşiv bütünlüğü

- 16 parça (`part-01.txt` … `part-16.txt`) birleştirildi, Base64 çözüldü.
- Aktarım dosyası 34.198 bayt; SHA-256: `69c49e5d7868fe1d83c2559f5da703a95eb443d18e4a5bd8b29eaa6c5a8f2f5a` — **beklenen değerle eşleşti**.
- Çift gzip açıldı, GNU tar çıkarıldı; **22 dosya** `docs/` altına yerleştirildi.
- `PLAN-MANIFEST.json` içindeki 22 dosyalık liste ile gerçek dosyalar eşleşiyor.
- Bağımsız çapraz kontrol: projenin kendi C17 SHA-256 uygulaması aynı özeti üretti.

## 2. Bulunan çelişkiler ve düzeltmeler

### C1 — Release onayı çelişkisi (giderildi)

- **Sorun:** `18-GUVENLIK-LISANS-PAKET.md` "Tag/release, proje sahibi onayı … geçmeden oluşturulmaz" diyordu; kök `AGENTS.md` ve `14` ise yayımlama yetkisinin OpenHands'e devredildiğini söylüyordu.
- **Düzeltme:** `18` release politikası bölümü yeniden yazıldı; ADR-007 eklendi.
- **Kanıt:** `docs/18-GUVENLIK-LISANS-PAKET.md`, `docs/adr/ADR-007-yetki-devri-ve-release.md`.

### C2 — Harici hazır varlık çelişkisi (giderildi)

- **Sorun:** `08`, `14`, `15` belgeleri "kamu malı/CC0 veya ticari kullanıma açık hazır veri varlığı kullanılabilir" diyordu; kök `AGENTS.md` ilk release içeriğinin dışarıdan hazır paket olarak alınmayacağını söylüyordu.
- **Düzeltme:** Üç belge, ilk release içeriğinin kendi C17 hattımızda özgün üretilmesini zorunlu kılacak biçimde güncellendi; yalnız bilimsel parametre/ölçüm verisinin model kartlarında referans alınabileceği belirtildi. ADR-008 eklendi.
- **Kanıt:** `docs/08`, `docs/14`, `docs/15`, `docs/adr/ADR-008-varlik-ve-icerik-uretimi.md`.

### C3 — Kaynak kodu lisansı çelişkisi (giderildi)

- **Sorun:** `18` "C kaynak kodu için lisans kararı proje sahibi tarafından verilir" diyordu; bu, teknik soru sorma yasağı ve yetki devriyle çelişiyordu.
- **Düzeltme:** Lisans seçimi OpenHands tarafından uygulanacak bir işlem olarak tanımlandı; ADR-010 eklendi.
- **Kanıt:** `docs/18`, `docs/adr/ADR-010-kaynak-kodu-lisansi.md`.

## 3. Eksik kabul eşiği ve performans kriteri tamamlamaları

- Performans release eşiği netleştirildi: referans sinematik profilde **p95 frame-time ≤ 33.3 ms** ve en az 10 dk ölçüm (`11-PERFORMANS-DONANIM.md`).
- İçerik üretim kapasitesi yeni mühendislik riski olarak `15-RISKLER-KARARLAR.md` tablosuna eklendi.

## 4. Uygulanabilirlik doğrulamaları (spike)

- **CPU renderer:** tiled raster + derinlik + PPM + SHA-256 prototipi çalışıyor. Bkz. `docs/feasibility/FEASIBILITY-CPU-RENDERER.md`.
- **OpenCL 1.2 / CPU fallback:** sistem loader üzerinden yetenek keşfi; GPU yoksa fallback. Bkz. `docs/feasibility/FEASIBILITY-OPENCL.md`.
- **Varlık hattı ve golden scene:** planlar hazır. Bkz. `docs/feasibility/FEASIBILITY-ASSET-PIPELINE.md`, `docs/feasibility/FEASIBILITY-GOLDEN-SCENE.md`.

## 5. Erişim sınırı (dürüst rapor)

- GitHub entegrasyon token'ı ile **push ve PR açma çalışıyor**.
- `main` branch protection, repository ruleset ve Actions izin ayarları API'leri **403** döndürüyor (yalnız içerik yazma yetkisi). Bu nedenle koruma ayarları bu token ile otomatik kurulamadı.
- Uygulanan güvenli alternatif: değişiklikler yalnız PR üzerinden; zorunlu kapılar workflow tanımlarıyla. Bkz. `docs/CI-VE-DAL-KORUMASI.md`, ADR-009.

## 6. Kalan riskler

- Windows GUI self-hosted runner henüz mevcut değil; `gui-windows.yml` runner kayıtlı olduğunda gerçek pencere testiyle genişletilecek. Runner yokken bu iş "çalıştı" sayılmaz.
- Branch protection yönetici yetkili erişim gerektirir; sağlanınca kurulacak.
- GPU kernel derleme/çalıştırma P10'a bırakıldı; bu spike yalnız keşif yapar.
