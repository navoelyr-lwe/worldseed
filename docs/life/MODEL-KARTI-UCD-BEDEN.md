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
   `ws_quad3_suggested_dt` = 3 ms · (`leg_upper` / 0,60 m), referans geyikte
   kalibre edilmiş 2 ms'de sınırlı (taban 0,2 ms). Çözücünün ölçülen kararlılık
   sınırı bacak boyuyla doğrusal ölçeklenir (≈0,0056 × `leg_upper`).
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
     `x`=ileri, `y`=yukarı, `z`=yanal), yaw sönümlemesi ve dünya baş açısı
     (heading) hedef izleme terimi (`ws_quad3_balance(..., heading_target)`).
     Hedef sıfırken düz yürüyüşü korur; doğrusal artan bir hedef komutlu dönüşü
     izler (dönüş izleme hatası < 0,25 rad). Kazançlar tür kütle × uzunluk
     ölçeğiyle ölçeklenir.
   - **Yürüyüş ve koşu:** tırıs (diagonal) faz; çapraz ayak çiftleri dönüşümlü
     salınır. Gait bir yapıdır (`ws_quad3_gait`): yürüyüş profil frekansı ve
     görev çarpanından, koşu ise daha yüksek kadans ve yaklaşık iki kat adım
     genliğinden türetilir (`ws_quad3_gait_walk`, `ws_quad3_gait_run`,
     `ws_quad3_gait_apply`). Koşu referans geyikte yürüyüşü ~1,8 m/s'den ~2,1
     m/s'ye çıkarır.
6. **Ayrıklaştırma ve çözücü:** Ardışık-impuls (sequential impulse),
   `solver_iterations = 30`, `position_bias = 0,2`, sabit adım. Deterministik:
   aynı girdi aynı çıktıyı verir (bit düzeyinde test edilir). **Adım duyarlılığı:**
   açık-çevrim yürüyüş sabit adıma duyarlıdır; baş açısı tutma terimi yanal
   sürüklenmeyi adımdan bağımsız biçimde sınırlar (2,0–2,2 ms aralığında
   sürüklenme < 0,3 m).
7. **Varsayımlar / sınırlar:** Katı gövdeler, düzlemsel olmayan zemin destekli;
   temas sürtünmesi tek nokta; kas yerine tork motorları; aerodinamik yok. Küçük
   türler daha küçük adım gerektirir (önerilen adım yardımcısı). Bu çözücü
   **sagittal golden sahneleri etkilemez**; golden CPU çıktısı bit-özdeş kalır.
8. **Kalibrasyon / doğrulama durumu:** D1 (kavramsal/geometrik doğrulama).
   Referans geyik için duruş kararlılığı, ileri yürüyüş (~1,7 m/s), düz baş
   (yanal sürüklenme < 0,3 m / 20 s), **komutlu dönüş (izleme hatası < 0,25 rad,
   dik kalma > 0,9)**, **yokuş tırmanışı (%10 eğimde > 0,5 m tırmanış, ileri
   ilerleme, dik kalma)**, **koşu (~2,1 m/s, yürüyüşten belirgin hızlı, dik
   kalma > 0,9)** ve **engebeli zemin (10 cm genlikli dalgalı yüzeyde ileri
   ilerleme)** ile bağımsız sol/sağ ayak yükü ve dört ayak teması CI'da
   kapılanır. D3 (ölçülmüş saha verisiyle kıyas) açıktır.
9. **Bilinen sınırlama:** Referans tür kızıl geyik tam kapılanır; diğer üç tür
   (tilki, tavşan, domuz) bilgi amaçlı raporlanır. Beşinci dilim küçük türlerin
   adım kuralını düzeltti (`ws_quad3_suggested_dt` artık `leg_upper` ile
   ölçekler) ve kanıt aracının tüm türlere sabit 1,4 Hz uygulamasını kaldırıp
   türün kendi `ws_quad3_gait_walk` frekansını kullanmasını sağladı; dört tür de
   artık kendi adımıyla ayakta kalır ve ileri yürür. Ölçüm (aynı kazançlar):
   tilki 0,15 → **0,63 m/s**, tavşan ~0,20 → **0,17 m/s**, domuz 1,36 → **1,07
   m/s** (`min_contacts=4`). Hâlâ eksik: açık-çevrim tırıs tür profil hızlarının
   altında kalır (tilki 1,8 / tavşan 2,0 / domuz 1,3 m/s); koşu/dönüş/yokuş
   senaryoları dört türe yayılmadı. Bunlar kapalı-çevrim (temas/hız geri
   beslemeli) denetleyici gerektirir ve ayrı bir P13 dilimidir. Ayrıca profil
   yürüyüş hızları Froude ölçeklemesiyle tutarsız görünüyor (tilki/tavşan için
   fiziksel olarak yüksek); bu da o dilimde gözden geçirilecek.
10. **İlişkili belgeler:** `docs/life/MODEL-KARTI-FRONTAL-DENGE.md` (P13 ilk
    dilim), `docs/life/MODEL-KARTI-REFERANS-TUR.md`, `docs/13-YOL-HARITASI.md`
    (P13), `docs/MILESTONE-BOARD.md`.
