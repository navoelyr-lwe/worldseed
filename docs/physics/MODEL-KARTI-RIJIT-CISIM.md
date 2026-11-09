# Model kartı — Rijit cisim dinamiği ve entegratör

1. **Kimlik / sürüm / modül:** `physics.rigid_body` v1; `src/physics/ws_physics.c`,
   `src/physics/ws_physics.h`; `WS_PHYSICS_VERSION = 1`.
2. **Temsil edilen süreç ve amaç:** Düşen ve yuvarlanan çevresel rijit cisimlerin
   (kaya, dal, düşen nesne) hareketi. Amaç, canlı bedenlerin (P5) ve ekoloji
   olaylarının üzerine kurulacağı kararlı, deterministik bir hareket çekirdeği.
3. **Ölçek:** Uzamsal 0,1 m–100 m (cisim ve bölge ölçeği); zamansal 1/200 s
   sabit adım (5 ms), tipik simülasyon penceresi saniyeler–saatler.
4. **Durum değişkenleri (SI):** konum `position` (m), hız `velocity` (m/s),
   kütle `mass` (kg), ters kütle `inv_mass` (1/kg), yarıçap `radius` (m).
5. **Denklemler / algoritma:** Newton dinamiği `F = m·a`; yarı-kapalı
   (symplectic/semi-implicit) Euler: `v ← v + a·dt`, `x ← x + v·dt`. Kuvvet
   toplayıcı her adımda sıfırlanır ve yerçekimi + ortam kuvvetleriyle doldurulur.
   Yöntemsel referans: standart sayısal analiz ve oyun-fiziği literatürü
   (semi-implicit Euler'in enerji davranışı); kod sıfırdan yazılmıştır, kaynak
   kopyalanmamıştır.
6. **Varsayımlar / geçerlilik:** Rijit cisim; dönme (açısal momentum) ilk
   sürümde modellenmez, bu yüzden cisimler kaymaz, yalnız ötelenir. Çok büyük
   hızlar ve şok dalgaları kapsam dışıdır. `dt` sabit ve render FPS'den bağımsız
   tutulur (`05-FIZIK-MOTORU.md`).
7. **Parametre kaynağı:** Yerçekimi `WS_GRAVITY = 9.80665 m/s²` (SI tanımı);
   varsayılan adım 1/200 s ve 4 solver iterasyonu mühendislik seçimidir, model
   kartında kayıtlıdır. Kalibrasyon gerektirmez.
8. **Sayısal yöntem / kararlılık:** Sabit adım + sabit iterasyon; kullanıcı
   dünyayı yalnız tam adımlarla ilerletir. Adım boyu yakınsaması test edilir;
   kararlılık için `dt` temas zaman ölçeğinin altında seçilir.
9. **Doğrulama planı:** Analitik serbest düşüş (hız ve konum), momentum korunumu,
   adım boyu yakınsaması, uzun koşu kararlılığı.
10. **Kabul eşikleri ve sınırlar:** Serbest düşüş hızı analitik çözümle bağıl
    hata ≤ 1e-2 (1/200 s'te); momentum iki cisimli esnek-olmayan çarpışmada
    korunur (≤ 1e-9 göreli); 20 s'lik dinlenme koşusunda konum kayması < 1e-3 m.
    Kanıt düzeyi **D2** (sayısal).
11. **Değişiklik kaydı:** Entegratör veya temas değişirse `WS_PHYSICS_VERSION`
    artırılır; kaydedilmiş dünya snapshot'ları bu sürümle etiketlenir ve eski
    snapshot'lar yeni sürümle sessizce yüklenmez.
