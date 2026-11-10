# Model kartı — Algı (duyusal model)

1. **Kimlik / sürüm / modül:** `life.perception` v1; `src/life/ws_perception.c`
   (`ws_perception_update`, `ws_perception_config_default`).
2. **Temsil edilen süreç ve amaç:** Referans organizmanın bir kontrol adımında
   hangi besin/su/tehdit noktasını algıladığını ve nerede olduğunu belirlemek.
   Davranış katmanına dünya durumuna dayalı, kamera bağımsız bir algı çıktısı
   vermek.
3. **Ölçek:** Uzamsal menzil 0–60 m; görüş alanı ~240°; zaman kontrol adımı
   (~0,02 s). Konum metre, açı radyan.
4. **Durum değişkenleri (SI):** Her kanal için `seen` (0/1), `dist` (m),
   `bearing` (rad, yönelime göre), `wx`,`wy` (m), `value` (0..1). İç durum
   kopyası: enerji, hidrasyon, vücut sıcaklığı (°C), yorgunluk (0..1).
5. **Denklem / algoritma:** Kanal başına, menzil içindeki ve görüş alanı
   (|bearing| ≤ fov/2) içindeki en yakın nokta seçilir; `bearing = wrap(atan2(dy,
   dx) − heading)`. Değeri sıfır (tükenmiş) noktalar atlanır. Eşitlikte düşük
   dizi indeksi seçilir (deterministik).
6. **Varsayımlar / geçerlilik:** Oklüzyon (görüş engeli) yok; koku ve işitme
   yok; çözünürlük tek seviyeli. Yalnız görünen menzil/alan içinde seçim.
   Geçerlilik: açık arazi ve orta ölçekli mesafeler.
7. **Parametre kaynağı:** Menzil ve görüş alanı, toynaklı memeli görüş alanı
   literatür aralığına (geniş, yandan görüş) dayandırılmış yerel seçimlerdir;
   nicel D3 kıyası yapılmamıştır.
8. **Sayısal yöntem / kararlılık:** Saf fonksiyon; kontrol adımıyla senkron.
   Determinizm birebir.
9. **Doğrulama planı:** Görüş alanı ve menzil sınırları; tükenmiş kaynağı atlama;
   determinizm. `ws_test_agent` içinde uygulanır.
10. **Kabul eşikleri ve sınırlar:** Menzil dışı ve görüş alanı dışı hedef
    seçilmez; en yakın hedef seçilir. Kanıt düzeyi **D1** (matematiksel/birim
    kontrolü ve sınır davranışı; ölçülmüş algı verisiyle D3 kıyası yok).
11. **Değişiklik kaydı:** Menzil/görüş alanı/seçim kuralı değişirse kart ve
    `tests/test_agent.c` güncellenir.
