# Model kartı — Frontal düzlem (lateral) denge ve dönüş

1. **Kimlik / sürüm / modül:** `life.lateral.balance` v1
   (`WS_LATERAL_VERSION = 1`); `src/life/ws_lateral.c` (`ws_lateral_init`,
   `ws_lateral_step`, `ws_lateral_measure`, `ws_lateral_params_for`,
   `ws_lateral_capture_point`). Kanıt aracı: `tools/ws_lateral_check.c`. Testler:
   `tests/test_lateral.c` (birim) ve CI adımı "Lateral balance and turning
   evidence (P13)".
2. **Temsil edilen süreç ve amaç:** Referans dört ayaklının (P5) sagittal
   zincirinde bulunmayan **üçüncü boyutu** — gövdenin yan (frontal) düzlemdeki
   salınımı, yalpalanması (roll) ve yön değiştirmesi (yaw) — azaltılmış
   mertebeden bir modelle temsil eder. Amaç, P13'ün "bağımsız sağ/sol duruş,
   denge ve dönüş dinamiği" ölçütünü gerçek, deterministik ve kapılanabilir bir
   modelle karşılamak; tam 3B rijit-gövde çözücüsü ise ayrı ve sonraki bir iş
   olarak kalır (bkz. 10).
3. **Ölçek:** CoM yüksekliği ve duruş genişliği türün omuz yüksekliğinden
   ölçeklenir (geyik: `com_height ≈ 0,89 m`, `track ≈ 0,47 m`, `ω ≈ 3,32 1/s`).
   Adım periyodu yürüyüş frekansının yarısıdır (`0,5 / gait_freq`). Hız 0–1,5 m/s;
   dönüş hızı tür başına üst sınırla sınırlıdır (geyik `yaw_max = 2,0 rad/s`).
   Tüm durumlar SI'dır: konum m, hız m/s, açı rad.
4. **Durum değişkenleri (SI):** `y` (m, orta hattan yanal CoM kayması; sol
   pozitif), `vy` (m/s), `phi` (rad, ileri eksen çevresinde gövde yalpalanması),
   `phi_dot` (rad/s), `yaw` (rad, yer düzleminde yön), `yaw_rate` (rad/s),
   `y_sup` (m, yüklü desteğin yanal konumu), `foot_y[2]` (sol/sağ ayak),
   `support` (+1 sol, −1 sağ), `step_timer` (s). Çıktı: `ws_lateral_metrics`.
5. **Denklemler / algoritma:**
   - **Yanal LIPM + ayak-bilek stabilizatörü:**
     `a_y = ω²·(y − y_sup) + k_lat·(y_ref − y) − d_lat·vy`,
     `ω = sqrt(g / com_height)`, `k_lat = 1,3·ω²`, `d_lat = 0,6·ω`.
     İlk terim pasif (destekten uzağa düşen) sarkaçtır; `k_lat/d_lat` terimleri
     standart "ayak-bilek stratejisi"dir.
   - **Yakalama noktası (capture point):** `ξ = y + vy / ω`. Her adım periyodunda
     destek tarafı değişir ve salınan ayak `clamp(ξ, ±foot_reach)` konumuna
     yerleştirilir (Raibert ilkesinin yanal karşılığı).
   - **Dönüş:** `yaw_rate` birinci mertebeden gecikmeyle komuta yaklaşır
     (`τ = yaw_tau`) ve `±yaw_max` ile sınırlanır. Merkezcil ivme `a_lat = v·yaw_rate`;
     kalıcı yalpa hedefi `y_ref = a_lat / ω²` ve `phi_ref = a_lat / g`.
   - **Yalpa:** sönümlü osilatör
     `phi'' = roll_ω²·(phi_ref − phi) − 2·ζ·roll_ω·phi' + c·(y − y_sup)`.
6. **Varsayımlar / geçerlilik:** Bu **azaltılmış mertebeden** bir modeldir, tam
   3B rijit-gövde çözücüsü değildir. Ayaklar kütlesizdir; ayak-bilek torku ve
   temas kuvveti açıkça çözülmez, bunların etkisi `k_lat/d_lat/roll` terimlerinde
   toplanır. Yanal ve sagittal hareket ayrıştırılmıştır (küçük yalpa açısı, sabit
   CoM yüksekliği varsayımı). Model mevcut sagittal çözücüye **yazmaz**; bu yüzden
   sagittal altın sahneler bit-özdeş kalır. Geçerli aralık: düşük/orta hızda
   yürüyüş ve dönüş; sıçrama, ani yön kırma ve dik yamaç yanal itişi kapsam dışıdır.
7. **Parametre kaynağı:** Raibert ayak yerleştirme ilkesi ve LIPM yakalama-noktası
   literatürü (standart yürüyüş denetimi sonuçları). Tür ölçekleme, tür kartlarındaki
   omuz yüksekliği ve yürüyüş frekansından türetilir. Kanıt seviyesi: **D2**
   (literatüre dayalı planlama değerleri; türe özgü ölçüm değil).
8. **Sayısal yöntem / kararlılık:** Sabit adım, açık (semi-implicit) Euler;
   `dt = 0,002 s`. Kazançlar `k_lat = 1,3·ω²`, `d_lat = 0,6·ω` seçilerek her tür
   için sönümlü ve kararlı salınım sağlanır; 60 s'lik düz yürüyüşte sınırlı kalır
   (bkz. 9). Aynı girdiler bit-özdeş çıktı verir (deterministik).
9. **Doğrulama planı:** Her tür için düz yürüyüş (60 s) ve komutlu dönüş (30 s):
   `max|y|`, `max|phi|`, alınan adım sayısı, dönüş oranı izleme hatası ve
   determinizm. Birim testleri ayrıca tür ölçeklemesini ve geçersiz girdi reddini
   sınar. Araç çıktısı: `evidence/lateral/lateral.{txt,csv}`.
10. **Kabul eşikleri ve sınırlar:** Geçerli ölçüm (2026-11, Linux CI, tümü PASS):
    geyik `max_sway=0,168 m`, `max_roll=0,117 rad`; tilki `0,053 / 0,102`; tavşan
    `0,034 / 0,137`; yaban domuzu `0,109 / 0,100`. Kapılar: `max|y| < 0,6·track`,
    `max|phi| < 0,35 rad`, saniyede ≥ 1 adım, dönüş izleme hatası `< 0,05 rad/s`,
    determinizm. **Sınır:** bu eşikler azaltılmış mertebeden modelin kendi
    tutarlılığını gösterir; tam 3B temas/denge kanıtı değildir. P13'ün kalan
    ölçütleri (bağımsız 3B uzuv state'i, temas kuvveti, enerji kıyası, tür-özel
    gait doğrulaması) tam 3B çözücüyle ayrıca ele alınacaktır.
11. **Değişiklik kaydı:** Model yapısı, kazançlar veya ölçekleme değişirse bu kart
    ve `ws_lateral_check` kapıları birlikte güncellenir; `WS_LATERAL_VERSION`
    artırılır. v1: ilk frontal düzlem denge ve dönüş modeli (P13 ilk dilimi).
