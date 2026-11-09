# Model kartı — Homeostaz (iç durum ve ihtiyaçlar)

1. **Kimlik / sürüm / modül:** `life.organism.homeostasis` v1;
   `src/life/ws_organism.c` (`ws_organism_step`, `ws_organism_eat`,
   `ws_organism_drink`, `ws_organism_most_urgent`, `ws_organism_drive`).
2. **Temsil edilen süreç ve amaç:** Referans organizmanın enerji, hidrasyon,
   vücut sıcaklığı, yorgunluk ve sağlık durumunu ve bunlardan türeyen ihtiyaç
   dürtülerini tutmak; davranış katmanına aciliyet sıralaması sağlamak.
3. **Ölçek:** Rezervler tür maksimumunun [0,1] oranı; sıcaklık °C; hızlar 1/s ve
   °C/s; zaman kontrol adımı (~0,02 s).
4. **Durum değişkenleri (SI):** `energy`, `hydration`, `fatigue`, `health`
   (0..1); `body_temp_c` (°C); `alive` (0/1); türetilmiş `hunger`, `thirst`,
   `warmth` (0..1).
5. **Denklem / algoritma:**
   - Enerji: `E -= (basal + move·v)·dt`; su: `H -= (basal_w + move_w·v)·dt`.
   - Yorgunluk: `F += fatigue_rate·v·dt` (hareket) veya `F -= recovery·dt`
     (dinlenme).
   - Sıcaklık: `T += (T_hedef − T)·thermal_rate·dt`, `T_hedef = T_konfor +
     activity_heat·v + 0.15·(T_ortam − T_konfor)`.
   - Dürtüler: `hunger = 1−E`, `thirst = 1−H`, `warmth = clamp(|T−T_konfor|/
     scale)`.
   - Sağlık: yalnız `E=0` veya `H=0` iken `health -= starve_rate·dt`.
6. **Varsayımlar / geçerlilik:** Doğrusal tüketim ve birinci mertebe sıcaklık
   gevşetmesi; sindirim, oksijen borcu ve ısı üretimi ayrıntısı yok. Vücut
   sıcaklığı ±4·scale bandıyla sınırlı. Rezervler [0,1] dışına çıkmaz.
7. **Parametre kaynağı:** Konfor sıcaklığı geyik çekirdek sıcaklığı (~38,5 °C)
   literatür bilgisidir; tüketim/geri kazanım hızları yerel ayarlıdır ve makul
   bir otlama döngüsü verecek biçimde seçilmiştir. D3 nicel kıyası yoktur.
8. **Sayısal yöntem / kararlılık:** Açık (explicit) Euler; kontrol adımıyla
   senkron. Rezerv korunumu ve sınırları testlerle doğrulanır. Determinizm
   birebir.
9. **Doğrulama planı:** Rezerv korunumu (bedava enerji yok), yeme/içme doldurma,
   rezerv tükendiğinde ölüm, ihtiyaç önceliklendirme, uzun koşu kararlılığı.
   `ws_test_agent` içinde uygulanır.
10. **Kabul eşikleri ve sınırlar:** `0 ≤ rezerv ≤ 1`; sağlık yalnız boş rezervde
    azalır; sağlık=0 → ölüm. Kanıt düzeyi **D2** (korunum ve kararlılık testleri
    geçti; D3 metabolik ölçüm kıyası yok).
11. **Değişiklik kaydı:** Hızlar veya denklemler değişirse kart ve
    `tests/test_agent.c` güncellenir.
