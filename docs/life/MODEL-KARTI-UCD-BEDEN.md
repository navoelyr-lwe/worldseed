# Model kartı — Üç boyutlu rijit-gövde çekirdeği ve referans dört ayaklı (P13)

1. **Kimlik / sürüm / modül:** `life.body3d.rigid` v1
   (`src/life/ws_body3d.c`: `ws_body3d_init`, `ws_body3d_add_body`,
   `ws_body3d_add_joint`, `ws_body3d_add_foot`, `ws_body3d_step`,
   `ws_body3d_advance`; `src/life/ws_quad3d.c`: `ws_quad3_suggested_dt`,
   `ws_quad3_build`, `ws_quad3_hold_rest`, `ws_quad3_gait_targets`,
   `ws_quad3_balance`). Kanıt aracı: `tools/ws_body3d_check.c`. Testler:
   `tests/test_body3d.c` (birim) ve CI adımı "Three-dimensional body evidence
   (P13)".
2. **Temsil edilen süreç ve amaç:** P5 sagittal (tek düzlem) zincirinin ve P13
   frontal (lateral) modelinin yerine geçmeyen, **bağımsız sağ/sol uzuvları olan
   tam üç boyutlu** rijit-gövde dinamiği. Gövde yönelimi kuaterniyonla, dönme
   ataleti gövde-çerçevesi tensörüyle temsil edilir; eklemler üç boyutlu nokta
   (ball-socket) bağı, gerçek menteşe kilidi ve tek taraflı açısal limit ile
   modellenir; temas üç boyutlu normal + Coulomb sürtünmedir. Amaç, P13'ün
   "yürüme/koşu/dönüş/yokuş senaryolarında bağımsız uzuvlu 3B beden" ölçütünü
   gerçek, deterministik ve kapılanabilir bir çözücüyle karşılamak. İlk dilim
   referans tür **kızıl geyik**tir; diğer üç tür bilgi amaçlı raporlanır.
3. **Ölçek:** SI. Konum m, hız m/s, açı rad, açısal hız rad/s, kütle kg, kuvvet
   N, tork N·m. Referans geyik: omuz 1,05 m, gövde uzunluğu 1,6 m, toplam kütle
   110 kg, bacak üst/alt 0,60 m, duruş 0,80 m. Önerilen adım
   `ws_quad3_suggested_dt` = 2 ms · (omuz / 1,05 m)^0,75 (taban 0,2 ms).
4. **Durum değişkenleri (SI):** Gövde başına konum (`pos`), kuaterniyon
   oryantasyon (`orient`), doğrusal hız (`vel`), açısal hız (`omega`); gövde
   kütlesi, gövde-çerçevesi atalet tensörü (`inertia_body`) ve tersi. Eklem:
   iki gövde indeksi, yerel bağlantı noktaları, menteşe ekseni ve kilidi, açısal
   limit, motor hedefi. Ayak: gövde + yerel nokta + sürtünme. Temas: normal,
   nüfuz, birikmiş normal/teğet impuls. Çıktı sorguları: toplam kütle, kütle
   merkezi, kinetik enerji, doğrusal ve açısal momentum.
5. **Denklemler / algoritma:**
   - **Yönelim:** birim kuaterniyon `q`; `ws_quat_integrate` ile
     `q ← normalize(q + 0,5·ω⊗q·dt)`. Dünya ataleti
     `I_world = R·I_body·Rᵀ`.
   - **Tümleme:** yarı-örtük Euler; kuvvet/tork önce hıza, sonra konuma işlenir.
   - **Eklem (ball-socket):** nokta bağı, etkin kütle
     `K = Σ(1/m + 1/I·r²)` ile çözülür; Baumgarte terimi konum hatasını
     `position_bias` oranıyla hıza çevirir.
   - **Menteşe kilidi:** menteşe eksenine dik iki açısal serbestlik kilitlenir;
     kilitleme bir hız düzeyi kısıtıdır, konum hatası `bias/dt` gibi
     uygulanmaz (aksi hâlde enerji enjekte eder).
   - **Temas + sürtünme:** normal kısıt tek taraflıdır (impuls ≥ 0); teğet
     kısıt Coulomb konisiyle sınırlanır (`|λ_t| ≤ μ·λ_n`).
   - **Denge refleksi:** gövde ekseninde pitch/roll PD torku (gövde
     `x`=ileri, `y`=yukarı, `z`=yanal) ve yaw hız servosu
     (`ws_quad3_balance(..., yaw_rate_target)`). Kazançlar tür kütle × uzunluk
     ölçeğiyle ölçeklenir.
   - **Yürüyüş:** tırıs (diagonal) faz; çapraz ayak çiftleri dönüşümlü salınır
     (`ws_quad3_gait_targets`).
6. **Ayrıklaştırma ve çözücü:** Ardışık-impuls (sequential impulse),
   `solver_iterations = 30`, `position_bias = 0,2`, sabit adım. Deterministik:
   aynı girdi aynı çıktıyı verir (bit düzeyinde test edilir).
7. **Varsayımlar / sınırlar:** Katı gövdeler, düzlemsel olmayan zemin destekli;
   temas sürtünmesi tek nokta; kas yerine tork motorları; aerodinamik yok. Küçük
   türler daha küçük adım gerektirir (önerilen adım yardımcısı). Bu çözücü
   **sagittal golden sahneleri etkilemez**; golden CPU çıktısı bit-özdeş kalır.
8. **Kalibrasyon / doğrulama durumu:** D1 (kavramsal/geometrik doğrulama).
   Referans geyik için duruş kararlılığı, ileri yürüyüş, düz baş, bağımsız
   sol/sağ ayak yükü ve dört ayak teması CI'da kapılanır. D3 (ölçülmüş saha
   verisiyle kıyas) açıktır.
9. **Bilinen sınırlama:** Diğer üç tür (tilki, tavşan, domuz) henüz aynı
   standartta doğrulanmadı; tür-özel gait fazı/frekansı ve denge kazançları
   ayrıca ele alınacak (P13 sonraki dilim). Koşu ve yokuş senaryoları bu dilimde
   yok.
10. **İlişkili belgeler:** `docs/life/MODEL-KARTI-FRONTAL-DENGE.md` (P13 ilk
    dilim), `docs/life/MODEL-KARTI-REFERANS-TUR.md`, `docs/13-YOL-HARITASI.md`
    (P13), `docs/MILESTONE-BOARD.md`.
