# Milestone panosu

Bu pano `13-YOL-HARITASI.md` aşamalarının durumunu izler. Durum yalnız kanıtla ilerler: kod + otomatik test + gerçek çalışma kanıtı + (ilgiliyse) benchmark + doküman + bilinen sınırlama.

| Aşama | Kapsam | Durum | Kanıt |
|---|---|---|---|
| P0 | Ürün ve bilimsel temel | Tamamlanan denetim | `DENETIM-RAPORU.md`, ADR-007…010 |
| P1 | C17 motor çekirdeği | Tamamlandı (altyapı) | `docs/P1-MOTOR-CEKIRDEGI.md`, `src/core`, `src/world`, `tests/`, `ci.yml` |
| P2 | Render ve içerik temeli | Başlangıç (spike) | `src/render`, `FEASIBILITY-CPU-RENDERER.md` |
| P3 | İlk doğal çevre | Planlandı | — |
| P4 | Çevresel fizik | Planlandı | — |
| P5 | Referans canlı biyomekaniği | Planlandı | — |
| P6 | Organizma ve davranış | Planlandı | — |
| P7 | Ekosistem ve temsilî türler | Planlandı | — |
| P8 | Kalıcı ve çok ölçekli dünya | Planlandı | — |
| P9 | Sinematik yönetmen ve ses | Planlandı | — |
| P10 | GPU ve performans ölçekleme | Başlangıç (keşif) | `src/platform`, `FEASIBILITY-OPENCL.md` |
| P11 | Ürünleştirme ve ilk release | Planlandı | — |

## Açık blokajlar

- Windows GUI self-hosted runner henüz kayıtlı değil; bu nedenle `gui-windows.yml` yalnız manuel (`workflow_dispatch`) tetiklenir. Otomatik tetikleme, runner kaydedilince açılır. Runner yokken hiçbir GUI testi "çalıştı" olarak raporlanmaz.
- Branch protection yönetici erişimi gerektirir (bkz. `CI-VE-DAL-KORUMASI.md`).

## Notlar

- İlk görev yalnız doküman/altyapı PR'ıdır; ürün release'i değildir.
- Kullanıcıya görünür ilk Windows paketi P2'den itibaren prerelease olarak yayımlanır.
