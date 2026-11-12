# Model kartı — Frontal düzlem (lateral) denge, dört ayak teması ve dönüş

1. **Kimlik / sürüm / modül:** `life.lateral.balance` v2
   (`WS_LATERAL_VERSION = 2`); `src/life/ws_lateral.c` (`ws_lateral_init`,
   `ws_lateral_step`, `ws_lateral_measure`, `ws_lateral_params_for`,
   `ws_lateral_capture_point`, `ws_lateral_support_center`). Kanıt aracı:
   `tools/ws_lateral_check.c`. Testler: `tests/test_lateral.c` (birim) ve CI
   adımı "Lateral balance and turning evidence (P13)".
2. **Temsil edilen süreç ve amaç:** Referans dört ayaklının (P5) sagittal
   zincirinde bulunmayan **üçüncü boyutu** — gövdenin yan (frontal) düzlemdeki
   salınımı, yalpalanması (roll) ve yön değiştirmesi (yaw) — ve **bağımsız dört
   ayak teması** azaltılmış mertebeden bir modelle temsil eder. Amaç, P13'ün
   "bağımsız sağ/sol duruş, temas ve denge" ölçütünü gerçek, deterministik ve
   kapılanabilir bir modelle karşılamak; tam 3B rijit-gövde çözücüsü ise ayrı ve
   sonraki bir iş olarak kalır (bkz. 10).
3. **Ölçek:** CoM yüksekliği ve duruş genişliği türün omuz yüksekliğinden
   ölçeklenir (geyik: `com_height ≈ 0,89 m`, `track ≈ 0,47 m`, `ω ≈ 3,32 1/s`).
   Adım periyodu yürüyüş frekansının yarısıdır (`0,5 / gait_freq`). Hız 0–1,5 m/s;
   dönüş hızı tür başına üst sınırla sınırlıdır (geyik `yaw_max = 2,0 rad/s`).
   Tüm durumlar SI'dır: konum m, hız m/s, açı rad, kuvvet N.
4. **Durum değişkenleri (SI):** `y` (m, orta hattan yanal CoM kayması; sol
   pozitif), `vy` (m/s), `phi` (rad, ileri eksen çevresinde gövde yalpalanması),
   `phi_dot` (rad/s), `yaw` (rad), `yaw_rate` (rad/s), `y_sup` (m, yük-ağırlıklı
   destek merkezi), `foot_y[4]` (sol-ön, sağ-ön, sol-arka, sağ-arka ayak yanal
   konumu), `foot_contact[4]` (1 duruş / 0 salınım), `foot_load[4]` (N, dikey
   yer tepkisi), `support` (+1 sol çift, −1 sağ çift), `step_timer` (s). Çıktı:
   `ws_lateral_metrics` (yanal + temas/yük metrikleri).
5. **Denklemler / algoritma:**
   - **Yanal LIPM + ayak-bilek stabilizatörü:**
     `a_y = ω²·(y − y_sup) + k_lat·(y_ref − y) − d_lat·vy`,
     `ω = sqrt(g / com_height)`, `k_lat = 1,3·ω²`, `d_lat = 0,6·ω`.
   - **Yakalama noktası:** `ξ = y + vy / ω`. Her adım periyodunda yüklü taraf
     değişir ve o tarafın yanal ayak konumu `clamp(ξ, ±foot_reach)` olur.
   - **Dört ayak teması:** Sol çift (sol-ön, sol-arka) ile sağ çift (sağ-ön,
     sağ-arka) dönüşümlü yüklenir (yanal-sıralı yürüyüş). Yüklü çiftteki iki ayak
     duruştadır ve ağırlığı eşit paylaşır; salınım çifti yük taşımaz. Böylece her
     adımda yer tepkisi toplamı tam olarak `m·g`'dir (korunum).
   - **Destek merkezi:** `y_sup = Σ(foot_load·foot_y) / Σ foot_load` — yükle
     birlikte sürekli hareket eder, adım başında sıçramaz.
   - **Dönüş:** `yaw_rate` birinci mertebeden gecikmeyle komuta yaklaşır
     (`τ = yaw_tau`) ve `±yaw_max` ile sınırlanır. Merkezcil ivme
     `a_lat = v·yaw_rate`; kalıcı yalpa hedefi `y_ref = a_lat / ω²` ve
     `phi_ref = a_lat / g`.
   - **Yalpa:** sönümlü osilatör
     `phi'' = roll_ω²·(phi_ref − phi) − 2·ζ·roll_ω·phi' + c·(y − y_sup)`.
6. **Varsayımlar / geçerlilik:** Bu **azaltılmış mertebeden** bir modeldir, tam
   3B rijit-gövde çözücüsü değildir. Dikey yön entegre edilmez; yük **yarı-statik**
   bir dağılımdır (duruş çiftinde eşit pay, salınım çiftinde sıfır) ve korunumludur.
   Ayak-bilek torku açıkça çözülmez; etkisi `k_lat/d_lat/roll` terimlerinde
   toplanır. Yanal ve sagittal hareket ayrıştırılmıştır (küçük yalpa açısı, sabit
   CoM yüksekliği). Model mevcut sagittal çözücüye **yazmaz**; bu yüzden sagittal
   altın sahneler bit-özdeş kalır. Geçerli aralık: düşük/orta hızda yürüyüş ve
   dönüş; sıçrama, ani yön kırma ve dik yamaç yanal itişi kapsam dışıdır.
7. **Parametre kaynağı:** Raibert ayak yerleştirme ilkesi ve LIPM yakalama-noktası
   literatürü. Tür ölçekleme tür kartlarındaki omuz yüksekliği, kütle ve yürüyüş
   frekansından türetilir. Kanıt seviyesi: **D2** (literatüre dayalı planlama
   değerleri; türe özgü ölçüm değil).
8. **Sayısal yöntem / kararlılık:** Sabit adım, açık (semi-implicit) Euler;
   `dt = 0,002 s`. Kazançlar `k_lat = 1,3·ω²`, `d_lat = 0,6·ω` seçilerek her tür
   için sönümlü ve kararlı salınım sağlanır; 60 s'lik düz yürüyüşte sınırlı kalır.
   Aynı girdiler bit-özdeş çıktı verir (deterministik).
9. **Doğrulama planı:** Her tür için düz yürüyüş (60 s) ve komutlu dönüş (30 s):
   `max|y|`, `max|phi|`, alınan adım sayısı, dönüş oranı izleme hatası, temas
   değişim sayısı, en az duruş ayak sayısı, yük korunum hatası ve determinizm.
   Birim testleri ayrıca tür ölçeklemesini, dört ayak temas/yük korunumunu ve
   geçersiz girdi reddini sınar. Araç çıktısı: `evidence/lateral/lateral.{txt,csv}`.
10. **Kabul eşikleri ve sınırlar:** Geçerli ölçüm (2026-11, Linux CI, tümü PASS):
    geyik `max_sway=0,168 m`, `max_roll=0,117 rad`, `contacts=674`; tilki
    `0,053 / 0,102 / 1250`; tavşan `0,034 / 0,137 / 1442`; yaban domuzu
    `0,109 / 0,100 / 866`. Kapılar: `max|y| < 0,6·track`, `max|phi| < 0,35 rad`,
    saniyede ≥ 1 adım, dönüş izleme hatası `< 0,05 rad/s`, her adımda en az iki
    ayak duruşta, temas değişimi > 0, yük korunum hatası `< 1e-6 N`, determinizm.
    **Sınır:** bu eşikler azaltılmış mertebeden modelin kendi tutarlılığını
    gösterir; tam 3B rijit-gövde/temas dinamiği kanıtı değildir. P13'ün kalan
    ölçütleri (bağımsız 3B uzuv eklem state'i, temas kuvvetinin dinamiği, enerji
    kıyası, tür-özel 3B gait doğrulaması) tam 3B çözücüyle ayrıca ele alınacaktır.
11. **Değişiklik kaydı:** Model yapısı, kazançlar veya ölçekleme değişirse bu kart
    ve `ws_lateral_check` kapıları birlikte güncellenir; `WS_LATERAL_VERSION`
    artırılır. v1: ilk frontal düzlem denge ve dönüş modeli. v2: dört bağımsız
    ayak (temas + yük korunumu) ve yük-ağırlıklı destek merkezi.
