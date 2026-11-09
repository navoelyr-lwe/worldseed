# Model kartı — Hareket maliyeti (kas-eşdeğeri motor işi)

1. **Kimlik / sürüm / modül:** `life.metabolism.cost` v1;
   `src/life/ws_creature.c` (`ws_creature_step`; alanlar `motor_work`,
   `motor_power`).
2. **Temsil edilen süreç ve amaç:** Hareketin enerji maliyetinin ilk bağlantısı
   (`06-CANLI-BIYOMEKANIK.md`): kas-eşdeğeri motorların mekanik işi. Amaç,
   yürümenin durmaktan daha pahalı olmasını ölçülebilir kılmak ve gelecekteki
   fizyoloji (yorgunluk/açlık) bağlantısına temel sağlamak.
3. **Ölçek:** İş (J) ve güç (W); adım başına birikim, 1/500 s adım.
4. **Durum değişkenleri (SI):** `motor_work` (J, kümülatif), `motor_power` (W,
   son adım).
5. **Denklemler / algoritma:** Her motor için tork `t = kp·(hedef − q) − kd·q̇`
   (tork bütçesiyle sınırlı). Maliyet artışı `ΔW = |t·q̇|·dt`. Kas hem pozitif
   hem negatif işte enerji harcadığı için mutlak değer kullanılır (standart
   pozitif-iş maliyet vekili).
6. **Varsayımlar / geçerlilik:** Metabolik enerji, mekanik motor işiyle
   orantılı varsayılır; kas verimi, ısı, oksijen borcu ve anaerobik katkı
   modellenmez. Tendon elastik enerji depolaması yoktur.
7. **Parametre kaynağı:** Vekil maliyet yalnız motor torku ve eklem hızından
   türetilir; ek kalibrasyon parametresi yoktur. D3 için ölçülmüş metabolik
   hız (örn. oksijen tüketimi) ile kıyas gerekir.
8. **Sayısal yöntem / kararlılık:** Kümülatif toplam; negatif olmayan, sonlu.
   Uzun koşuda taşma olmaması test edilir.
9. **Doğrulama planı:** Duruşta maliyet küçük ama pozitif; yürüyüşte maliyet
   duruştan kesinlikle büyük; anlık güç sonlu ve makul üst sınırın altında.
10. **Kabul eşikleri ve sınırlar:** `walk_work > 0`; `0 ≤ motor_power < 5e5 W`;
    tüm durum sonlu. Kanıt düzeyi **D1** (mekanik iş vekili; metabolik D3
    kıyası yok).
11. **Değişiklik kaydı:** Maliyet tanımı değişirse kart ve `tests/test_life.c`
    güncellenir.
