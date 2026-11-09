# Model kartı — Davranış ve sınırlı öğrenme

1. **Kimlik / sürüm / modül:** `life.agent.behavior` v1; `src/life/ws_agent.c`
   (`ws_agent_step`, `ws_agent_init`, `ws_agent_memory_*`).
2. **Temsil edilen süreç ve amaç:** Algı ve iç durumdan açık, determinist ve
   açıklanabilir bir davranış seçmek; kısa olay hafızası ve uzamsal hafızayla
   sınırlı öğrenme göstermek.
3. **Ölçek:** Yol hızı 0–1,5 m/s; dönüş oranı 2 rad/s; zaman kontrol adımı
   (~0,02 s); deney süresi ~90 s; olay hafızası 32 kayıt.
4. **Durum değişkenleri (SI):** `x`,`y` (m), `heading` (rad), `speed` (m/s),
   `behavior`, `target_x/y` (m), uzamsal hafıza konumları (m), olay halkası
   (tick, tür, x, y, değer), `distance_travelled` (m), `food_found`,
   `water_found`, `alarm_s` (s).
5. **Denklem / algoritma (karar):** Sıra ile: tehdit görüş mesafesinde ise kaç;
   değilse en acil ihtiyaç eşiği aşıyorsa o ihtiyaca yönel; dürtü düşük ama
   hafıza varsa hafızadaki noktaya dön; aksi hâlde gezin. Hedefe sınırlı dönüş
   oranıyla yönel, son yaklaşmada yavaşla; erişim menzilinde kaynağı tüket.
   Kaçış yönü kilitlenir ve alarm süresi boyunca sürer.
6. **Varsayımlar / geçerlilik:** Fayda tabanlı, durum-makinesi benzeri karar;
   yol (path) kinematiktir (gövde dinamiği ayrı). Görülmeyen kaynağa yalnız
   hafıza üzerinden gidilir. Öğrenme küçük ve ölçülebilirdir; bilinç/zekâ
   iddiası yoktur.
7. **Parametre kaynağı:** Raibert/klasik davranış literatürü ve otlama gözlemleri
   bilgilendiricidir; eşikler ve kazançlar yerel ayarlıdır. D3 kıyası yoktur.
8. **Sayısal yöntem / kararlılık:** Saf determinist adım; rastgelelik yalnız
   ajan tohumundan (SplitMix64) gelir, aynı tohum aynı yolu verir.
9. **Doğrulama planı:** Tehditten kaçış, açlıkta besin arayışı, susuzlukta su
   arayışı, dinlenme, davranış determinizmi (kamera bağımsızlığı), çok tohumlu
   öğrenme-kontrol kıyası, geçersiz girdi. `ws_test_agent` içinde uygulanır.
10. **Kabul eşikleri ve sınırlar:** Tehditten uzaklaşma; dürtü eşiğinde ilgili
    kaynağa yönelme; öğrenen ajan, aynı tohum kümesinde toplam beslenmede
    kontrolü geçmeli ve tohumların çoğunda kazanmalı (tek tohum kabul değildir).
    Kanıt düzeyi **D2** (senaryo ve çok tohum testleri geçti; D3 davranış ölçüm
    kıyası yok).
11. **Değişiklik kaydı:** Karar sırası, eşikler veya hafıza kuralları değişirse
    kart ve `tests/test_agent.c` güncellenir.
