# Model kartı — Rüzgâr profili, hava sürtünmesi ve kaldırma kuvveti

1. **Kimlik / sürüm / modül:** `physics.wind` v1 ve `physics.environment` v1;
   `src/physics/ws_wind.c`, `src/physics/ws_physics.c`
   (`apply_environmental_forces`); `WS_PHYSICS_VERSION = 1`.
2. **Temsil edilen süreç ve amaç:** Yüzeye yakın rüzgâr hızının yükseklikle
   değişimi (kesme), cisim üzerindeki aerodinamik sürükleme ve su içindeki
   kaldırma kuvveti. Amaç, rüzgârın ve suyun çevresel cisimlere etkisini
   deterministik ve bilimsel temelli biçimde hesaba katmak.
3. **Ölçek:** Uzamsal yükseklik 0 m–100 m; zamansal anlık (quasi-steady) profil.
4. **Durum değişkenleri (SI):** rüzgâr hızı (m/s), yükseklik `z` (m), hava
   yoğunluğu ρ (kg/m³), sürükleme alanı `0.5·C_d·A` (m²), hacim `V` (m³), su
   yoğunluğu (kg/m³).
5. **Denklemler / algoritma:** Rüzgâr profili güç yasası
   `u(z) = u_ref·(z/z_ref)^α`, `α = 0.14` (Hellmann üssü, açık arazi),
   `z_ref = 10 m` (standart anemometre yüksekliği). Yön ve hız çevre modelinden
   (`ws_environment`) gelir. Sürükleme `F = −ρ·(0.5·C_d·A)·|v_rel|·v_rel`.
   Kaldırma kuvveti tam batmada `F_b = ρ_su·V·g`, yukarı yönde.
6. **Varsayımlar / geçerlilik:** Nötr tabakalı yüzey tabakası; güç yasası
   mühendislik yaklaşımıdır (tam çözülmüş sınır tabakası değil). Kaldırma
   yalnız tam batma durumunda hesaplanır; kısmi batma ve dalga yok. Sürtünme
   hızı ve pürüzlülük uzunluğu modellenmez.
7. **Parametre kaynağı:** `α = 0.14` açık arazi için yaygın kabul gören Hellmann
   üssüdür; ρ_hava = 1,225 kg/m³ (15 °C, 101,325 kPa), ρ_su = 997 kg/m³
   (≈25 °C) standart-atmosfer/el kitabı değerleridir. Değerler `ws_units.h`
   içinde tek yerde tanımlıdır.
8. **Sayısal yöntem / kararlılık:** Profil ve kuvvetler saf fonksiyonlardır
   (durum taşımaz); her adımda yeniden değerlendirilir. Hız bağımlı sürükleme
   açık-Euler ile entegre edilir; `dt` küçük tutulduğundan kararlıdır.
9. **Doğrulama planı:** `u(10)=u_ref` ve `u(40)/u(10)=(4)^0.14` kıyası; rüzgâr
   yönünün yatay kalması (y bileşeni sıfır); hafif büyük hacimli cisimde
   kaldırma kuvvetinin yerçekimini yenmesi.
10. **Kabul eşikleri ve sınırlar:** Profil değerleri analitik ifadeyle bağıl
    hata ≤ 1e-9; yatay rüzgârda y bileşeni tam 0; kaldırma testinde yukarı hız
    > 0. Kanıt düzeyi **D2**. D3 için arazi pürüzlülüğü ve anemometre verisiyle
    kıyas gerekir.
11. **Değişiklik kaydı:** Profil üssü veya kuvvet formülü değişirse
    `WS_PHYSICS_VERSION` artırılır ve kart güncellenir.
