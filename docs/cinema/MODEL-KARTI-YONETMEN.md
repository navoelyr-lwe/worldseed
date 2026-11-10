# Model kartı — Sinematik yönetmen (olay-tabanlı gözlemci)

1. **Kimlik / sürüm / modül:** `cinema.director` v1; `src/cinema/ws_director.c`,
   `src/cinema/ws_director.h`. Olay taşıyıcı: `src/life/ws_life_event.*`,
   `src/world/ws_event.*` (`WS_EVENT_LIFE`). Sahne köprüsü:
   `src/render/ws_env_scene.c` (`director_shot`, scene_kind 8).
2. **Temsil edilen süreç ve amaç:** `09-SINEMATIK-YONETMEN.md` uyarınca dünyayı
   izleyen, değiştirmeyen sinematik gözlemci. Yönetmen simülasyonun gerçekten
   ürettiği olay akışını tüketir, her olayı önem sırasına koyar, bir konu seçer,
   çekim tipini planlar ve kamerayı hareket sınırları içinde sürer. Amaç, olayları
   değiştirmeden gerçek zamanlı, sinematik bir görüntü sunmaktır.
3. **Ölçek:** Uzamsal: dünya yer düzlemi; yönetmen yer-göreli çerçevede (y=0)
   planlar, çizici kamerayı araziye kaldırır. SI birimleri (m, s, rad). Zamansal:
   kare adımı `dt` (s), çağıran tarafından verilir; ekolojide tipik 0.05 s.
4. **Durum değişkenleri:** mevcut çekim (`ws_director_shot`: tip, olay sırası/tick,
   tür, aktör türü, konu konumu, önem, planlanan kamera); çekim tutma süresi;
   kamera durumu (göz, hedef, önceki göz/hız/yaw); ölçümler (`ws_director_metrics`:
   hız/ivme/dönüş üst sınırları, takip hatası, kesme sayısı, gözlenen olay, kare);
   `next_sequence` (okunacak sonraki olay).
5. **Denklemler / algoritma (deterministik):**
   - **Önem:** `imp(kind)`: av 1.0, ölüm 0.8, doğum 0.6, otlama 0.35.
   - **Çekim tipi:** türden sabit eşleme (av → takip, ölüm → yakın, doğum → orta,
     otlama → geniş).
   - **Kesme kuralı:** ilk olay çekimi alır; mevcut çekim `min_shot_s` dolmadan
     kesilmez; `min_shot_s` sonrası yeni olay önemi `mevcut + cut_cost` üstündeyse
     kesilir; `max_shot_s` sonrası bayat çekim yenilenir.
   - **Kamera planı:** çekim tipine göre mesafe/yükseklik/sabit azimut;
     `(ex,ez) = (sx + cos(az)·dist, sz + sin(az)·dist)`, `ey = sy + height`.
   - **Occlusion:** görüş çizgisi 16 noktada örneklenir; `gy = ground(x,z)+1.5`;
     en büyük `gy − line_y > 0` kadar `ey` yükseltilir (yalnız kamera hareket eder).
   - **Kamera kontrolü:** hız `max_speed` ile, ivme `max_accel` ile, yaw dönüşü
     `max_turn_rate` ile sınırlanır; ivme `prev_vel`'den integre edilir.
   - **Kesme (cut):** göz sıçrar, hareket geçmişi sıfırlanır, kesme sayılır; sıçrama
     hareket olarak ölçülmez.
6. **Varsayımlar / ihmal edilen süreçler:** Tek konu izlenir; çok aktörlü ilişki
   kadrajı, yumuşak geçiş (dissolve) ve nesne/bitki occlusion'ı yoktur. Olay türü
   sayısı ekolojiyle sınırlıdır (su olayı yoktur). Rastgelelik yoktur.
7. **Parametre kaynağı, kalibrasyon ve belirsizlik:** `ws_director_config_default`
   tüm sabitleri tasarım seçimidir (`min_shot_s` 2 s, `max_shot_s` 8 s,
   `cut_cost` 0.25, `max_speed` 12 m/s, `max_accel` 8 m/s², `max_turn_rate`
   1.2 rad/s ≈ 69°/s, hareket hastalığı eşiğinin altında). Ölçülmüş sinematografi
   verisine kalibre edilmemiştir; belirsizlik orta-yüksektir.
8. **Sayısal yöntem, kararlılık:** Ölçümler kesmeler arası (sınırlı) karelerde
   alınır; hız/ivme/dönüş üst sınırları küçük sayısal marjla tutulur. `dt > 0` ve
   sonlu değilse `WS_ERR_INVALID_ARGUMENT`. Tüm değerler sonludur.
9. **Doğrulama planı:** `tests/test_cinema.c`: yönetmenin dünya otoritesi olmadığı
   (yönetmenli/yönetmensiz koşu bit-özdeş olay özeti); çekimin gerçek olay kimliğine
   bağlandığı ve sahte olay üretilmediği; çekim önceliği ve histerezis; kamera
   hız/ivme/dönüş sınırları (hareketli konu ile); kesmeler arası süreklilik;
   mod kontrolü ve serbest kamera; occlusion'ın yalnız kamerayı yükselttiği.
   `tests/test_world_env.c`: `director_shot` sahnesinin aynı tohumla aynı çekimi ve
   pozu ürettiği.
10. **Kabul eşikleri ve sınırlar:** Ölçülen hız/ivme/dönüş üst sınırları yapılandırma
    üst sınırının `%5` marjını aşmaz; kesmeler arası göz adımı `max_speed·dt·1.05`'i
    aşmaz; yönetmensiz koşuya göre olay özeti bit-özdeş. Bilinen sınırlar: tek konu,
    sert kesme, bitki occlusion'ı yok. Kanıt düzeyi **D2** (determinizm, sınır ve
    davranış testleri geçti; nicel sinematografi D3 kıyası yok).
11. **Değişiklik kaydı:** Önem tablosu, çekim parametreleri, kesme kuralı veya
    occlusion algoritması değişirse bu kart, `ws_director.*`, ilgili testler ve
    `docs/P9-SINEMATIK-YONETMEN-VE-SES.md` birlikte güncellenir.
