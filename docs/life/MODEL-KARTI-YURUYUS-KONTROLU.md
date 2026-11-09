# Model kartı — Yürüyüş kontrolü (Raibert tarzı ayak yerleştirme)

1. **Kimlik / sürüm / modül:** `life.locomotion.walk` v1;
   `src/life/ws_quadruped.c` (`ws_quadruped_walk`, `ws_quadruped_walk_steer`,
   `ws_gait_state_init`, `ws_quadruped_balance`).
2. **Temsil edilen süreç ve amaç:** Referans dört ayaklının düz/eğimli zeminde
   kararlı ileri yürüyüşü. Amaç, ayak yerleştirmesini hız geri beslemesiyle
   kapatarak düşmeden, taşmadan ve kaymadan ilerlemek.
3. **Ölçek:** Yürüyüş hızı 0–1,5 m/s; adım frekansı ~1,4 Hz; görev faktörü
   ~0,70; ayak kaldırma ~0,08 m.
4. **Durum değişkenleri (SI):** `phase` [0,1); ön/arka ayak `td_x`, `lo_x` (m);
   `stance[2]`; `foot_lift` (m); `kv` (s); `heading` (rad); `yaw_rate` (rad/s).
5. **Denklemler / algoritma:**
   - Nötr nokta (feed-forward): `x_td = hip_x + 0,5·v_des·T_st`.
   - Hız hatası düzeltmesi: `+ kv·(v_des − v_actual)` (negatif geri besleme).
   - Stance ayak geri çekilmesi: duruş fazında ayak gövde-göreli olarak sürüş
     hızında geriye çekilir; ayak yalnız "çok hızlı" durumda öne dikilir.
   - Gövde yüksekliği referansı: `ground_hip + p->shoulder_h`.
   - Yönlendirme (`walk_steer`): `heading += yaw_rate·dt` (kinematik).
   - Bacak IK: kalça/diz açıları (2-segment kapalı-form IK).
6. **Varsayımlar / geçerlilik:** Düzlemsel (sagittal) yürüyüş; lateral denge ve
   yaw torku yok. Zemin profili yürüyüş hattı boyunca tek boyutlu örneklenir.
   Kontrolcü, fizik çözücünün motor hedeflerini sürer; doğrudan poz komutu vermez.
7. **Parametre kaynağı:** Raibert ayak yerleştirme ilkesi ve standart dört ayaklı
   yürüyüş gözlemleri; `kv=0,30`, `foot_lift=0,08` yerel olarak ayarlanmıştır ve
   testlerle doğrulanmıştır. D3 için yayımlanmış yürüyüş kinematiğiyle (adım
   uzunluğu/frekans, görev faktörü) nicel kıyas eklenmelidir.
8. **Sayısal yöntem / kararlılık:** Denetim, fizik adımıyla (1/500 s) senkron
   çalışır; kazançlar testlerle sınırlandırılmıştır. Determinizm birebirdir.
9. **Doğrulama planı:** Düz zeminde ileri yürüyüş; eğimde (≈%3) yürüyüş;
   engebeli zeminde yürüyüş; dönüş komutu; duruşta denge; eklem sınırları;
   determinizm; uzun koşu.
10. **Kabul eşikleri ve sınırlar:** Düz zeminde ileri hız > 0,5 m/s (hedef ≈
    0,8 m/s); gövde dik (|açı| < ~0,35 rad); engebeli zeminde de ileri hız >
    1,0 m/s (15 s koşu) ve |açı| < 0,40 rad; determinizm birebir. Kanıt düzeyi
    **D2** (sayısal kararlılık, determinizm ve senaryo testleri geçti; D3
    referans kıyası yok).
11. **Değişiklik kaydı:** Kontrolcü yapısı veya kazançları değişirse kart ve
    `tests/test_life.c` güncellenir.
