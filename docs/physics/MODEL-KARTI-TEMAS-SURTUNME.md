# Model kartı — Zemin teması, sürtünme ve restitüsyon

1. **Kimlik / sürüm / modül:** `physics.contact` v1; `src/physics/ws_physics.c`
   (`resolve_terrain_contact`, `resolve_body_pair`); `WS_PHYSICS_VERSION = 1`.
2. **Temsil edilen süreç ve amaç:** Bir kürenin yükseklik-alanı zeminine ve
   başka bir küreye teması; temas kuvveti, Coulomb sürtünmesi ve restitüsyon
   (sıçrama) kaybı. Amaç, cisimlerin zeminde doğru yükseklikte ve kaymadan
   dinlenmesini sağlamak.
3. **Ölçek:** Uzamsal temas yarıçapı 0,01 m–10 m; zamansal adım başına temas
   çözümü, 1/200 s sabit adım.
4. **Durum değişkenleri (SI):** `restitution` (boyutsuz 0–1), `friction`
   (Coulomb katsayısı μ ≥ 0), `grounded` (0/1), konum/hız (m, m/s).
5. **Denklemler / algoritma:** Zemin yüksekliği `gy = h(x,z)`; penetrasyon
   `p = (gy + r) − y`. Penetrasyon doğrudan konumdan düşülür (dinlenme
   yüksekliği tam olur). Hız tepkisi impuls tabanlıdır: normal bileşen
   `v_n ← −e·v_n`, teğetsel bileşen Coulomb limitiyle azaltılır
   (`Δv_t ≤ μ·|Δv_n|`). Küre-küre teması ortak normal boyunca impuls paylaşır.
   Referans: standart impuls tabanlı temas çözümü ve Coulomb sürtünme yasası.
6. **Varsayımlar / geçerlilik:** Dönme yok; sürtünme kaymayı tam durdurur veya
   Coulomb limitiyle sınırlar. Eğim normali merkezî farklardan türetilir;
   çok dik süreksiz yüzeylerde (uçurum kenarı) normal yaklaşıktır. Tam akışkan
   sınır tabakası modellenmez.
7. **Parametre kaynağı:** μ ve e cisim başına verilir (örnek: kaya μ≈0,6,
   e≈0,05). Değerler mühendislik varsayımıdır ve kartta kayıtlıdır; D3 için
   yüzey türüne göre ölçülmüş sürtünme/restitüsyon tablosu eklenmelidir.
8. **Sayısal yöntem / kararlılık:** Her adımda `solver_iterations` kez temas
   çözülür; dinlenme bandı (−1e-6 m) zemin temasında titreşimi önler. Dik
   eğimde konum düzeltmesi dikey penetrasyonu doğrudan kaldırır, kayma sürüklenmesi
   oluşmaz.
9. **Doğrulama planı:** Eğik düzlemde statik denge ve kayma başlangıcı; 20 s
   dinlenme koşusunda penetrasyon/kayma kontrolü; restitüsyonlu sıçramada tepe
   yüksekliğinin azalması.
10. **Kabul eşikleri ve sınırlar:** Dinlenmede `|y − r| ≤ 1e-3 m`, yatay kayma
    < 1e-3 m, son hız < 1e-2 m/s (20 s sonunda); statik eğik düzlemde μ > tanθ
    ise cisim durur, μ < tanθ ise kayar. Kanıt düzeyi **D2**.
11. **Değişiklik kaydı:** Temas veya sürtünme modeli değişirse
    `WS_PHYSICS_VERSION` artırılır.
