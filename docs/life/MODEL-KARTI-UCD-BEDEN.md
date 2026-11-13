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
   gerçek, deterministik ve kapılanabilir bir çözücüyle karşılamak. Yedinci dilim
   ile **dört tür de** (kızıl geyik, kızıl tilki, Avrupa tavşanı, yaban domuzu)
   aynı standartta kapılanır; kızıl geyik yüksek ayrıntılı referans tür olarak
   kalır.
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
   - **Açık-çevrim yürüyüş (sürücü):** `ws_quad3_gait_apply` her bacak eklemine
     fazdan türeyen **sabit** açı hedefi yazar. Hızı düzenleyemez: belli bir
     kadasta gövde pasif dinamiğin bıraktığı yerde oturur ve küçük türler komut
     hızlarının çok altında kalır (bkz. sınır 9).
   - **Kapalı-çevrim yürüyüş (Raibert ayak yerleştirme):**
     `ws_quad3_gait_closed_loop` her bacağı temas noktasından türetir. Kalça
     çapası gövde çerçevesinden dünya konumuna dönüştürülür; dikey IK referansı
     **nominal** ayakta durma yüksekliğidir (`ground + shoulder_h`), ölçülen
     (çökmüş olabilir) kalça yüksekliği değil — çöküşü geri beslemek bacağı tam
     düşerken kısaltır ve duruşu çökertir (pozitif geri besleme). Duruş fazında
     ayak dünya uzayında sabit kalır (latch edilmiş `td_x`), salınım fazında
     ileri taşınır. Nötr temas noktası Raibert kuralıdır:
     `neutral = 0,5·v_ff·T_stance + kv·(v_act − v_des)`; ileri besleme hızı
     `v_ff` bacağın erişebileceği en hızlı adımla sınırlanır
     (`v_stride = 2·0,8·reach/T_stance`, `reach = sqrt((l1+l2)² − shoulder_h²)`),
     çünkü ulaşılamaz bir komut nötr noktayı bacağın dışına taşır ve gövdeyi geri
     sürükler. `v_des` ölçülen hızdan büyükken nötr nokta kalçanın gerisine düşer
     ve gövdeyi ileri iter. Durum `ws_quad3_gait_state` içinde tutulur; bacak
     başına salınım tepesi ve yerleştirme kazancı `ws_quad3_gait_state_init` ile
     tür boyutundan ölçeklenir (referans geyik 0,08 m tepe / 0,30 s kazanç).
     Çağıran, açık-çevrim sürücüde olduğu gibi `ws_quad3_balance` ile gövdeyi
     dengeler.
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
   **Dört tür de** (geyik, tilki, tavşan, domuz) CI'da aynı standartta kapılanır;
   her tür kendi ölçeğinde değerlendirilir. Duruş kararlılığı, ileri yürüyüş,
   düz baş (yanal sürüklenme omuz yüksekliğiyle sınırlı, 20 s), **komutlu dönüş
   (izleme hatası < 0,25 rad, dik kalma > 0,9)**, **yokuş tırmanışı (%10 eğimde
   gövde yüksekliğinin >1,2 katı tırmanış, yürüyüş hızının >0,5 katı ileri
   ilerleme, dik kalma)**, **koşu (türün kendi yürüyüş hızının >1,10 katı, dik
   kalma > 0,9)** ve **engebeli zemin (10 cm genlikli dalgalı yüzeyde yürüyüş
   hızının >0,6 katı ileri ilerleme)** ile bağımsız sol/sağ ayak yükü ve dört
   ayak teması CI'da kapılanır (kanıt aracı toplam 20 sahne üretir; her aile
   dört türden geçer). **Kapalı-çevrim denetleyici** dört tür için de CI'da
   kapılanır: her tür kendi komut yürüyüş hızında ileri gider, dik kalır
   (`upright_min > 0,9`) ve yaklaşık düz bir çizgi tutar; birim testleri
   `closed-loop speed` ve `species run exceeds walk` (`tests/test_body3d.c`) ile
   kanıt aracı `closed_loop` satırları
   (`tools/ws_body3d_check.c`) bunu doğrular. D3 (ölçülmüş saha verisiyle kıyas)
   açıktır.
9. **Bilinen sınırlama:** Dört tür de aynı standartta kapılanır (yedinci dilim);
   kızıl geyik yüksek ayrıntılı referans türdür. Beşinci dilim küçük türlerin
   adım kuralını düzeltti (`ws_quad3_suggested_dt` artık `leg_upper` ile
   ölçekler) ve kanıt aracının tüm türlere sabit 1,4 Hz uygulamasını kaldırıp
   türün kendi `ws_quad3_gait_walk` frekansını kullanmasını sağladı. Altıncı
   dilim **kapalı-çevrim (Raibert ayak yerleştirmeli) denetleyiciyi** ekledi
   (`ws_quad3_gait_closed_loop`): hız geri beslemesi olmadığı için açık-çevrim
   tırıs küçük türlerde komut hızının çok altında kalıyordu; kapalı-çevrim
   denetleyici aynı koşullarda hızı komuta yaklaştırır. Ölçüm (20 s, düz zemin,
   her tür kendi komut hızında):

   | Tür | Komut (m/s) | Açık-çevrim (m/s) | Kapalı-çevrim (m/s) | Dik kalma |
   |---|---|---|---|---|
   | Kızıl geyik | 1,50 | 1,77 | 2,29 | 0,97 |
   | Kızıl tilki | 1,80 | 0,63 | 0,81 | 0,99 |
   | Avrupa tavşanı | 2,00 | 0,17 | 0,36 | 0,98 |
   | Yaban domuzu | 1,30 | 1,07 | 1,04 | 0,98 |

   Kapalı-çevrim tilki ve tavşanı belirgin biçimde hızlandırır ve dört tür de
   ayakta kalır. Yedinci dilimde **dönüş, yokuş, koşu ve engebeli zemin
   senaryoları dört türe yayıldı**; her kapı türün kendi ölçeğine göre kurulur
   (duruş/yürüyüş sınırları omuz yüksekliğiyle, koşu eşiği türün gerçekleşen
   yürüyüş hızının 1,10 katıyla, yokuş tırmanışı gövde yüksekliğiyle ölçeklenir).
   Koşu adımı da artık tür ölçeğine göre ayarlanır: `ws_quad3_gait_run` hip
   açısını `sqrt(0,60 m / leg_upper)` ile ölçekler; sabit açı kısa bacaklı
   türlerde doyuma giriyor ve tavşanın "koşusu" yürüyüşünden hızlı olmuyordu.
   Ölçülen yürüyüş→koşu oranı: geyik 1,20, tilki 1,38, tavşan 1,64, domuz 1,28
   (`species run exceeds walk` testi). Hâlâ eksik: **hiçbir tür komut hızının
   tamamına ulaşmaz** — küçük türlerin bacak erişimi ve kadansı komut hızlarını
   yapısal olarak sınırlar (tilki 1,8 / tavşan 2,0 m/s bu gövde geometrisiyle
   ulaşılamaz). Bu yüzden `ws_species_walk_speed` içindeki tilki/tavşan değerleri
   hâlâ Froude ölçeklemesiyle tutarsızdır (sabit-Froude yürüyüş `sqrt(shoulder_h)`
   ile ölçeklenir; tilki ≈0,90, tavşan ≈0,62 m/s olurdu). Bu değerlerin ya profile
   göre düşürülmesi ya da tür adım/kadans geometrisinin yeniden ayarlanması ayrı
   bir P13 dilimidir. Kapalı-çevrim denetleyici şu an yalnız düz zemin yürüyüşü
   için kapılanır; dönüş/yokuş/engebeli zeminde kapalı-çevrim kapılanması da ayrı
   bir dilimdir.
10. **İlişkili belgeler:** `docs/life/MODEL-KARTI-FRONTAL-DENGE.md` (P13 ilk
    dilim), `docs/life/MODEL-KARTI-REFERANS-TUR.md`, `docs/13-YOL-HARITASI.md`
    (P13), `docs/MILESTONE-BOARD.md`.
