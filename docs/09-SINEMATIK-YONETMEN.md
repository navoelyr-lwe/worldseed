# 09 — Cinematic Observer: sinematik yönetmen

## Ana kural

Yönetmen dünya simülasyonunun otoritesi değildir. Yalnızca olay akışını ve salt okunur dünya snapshot'ını tüketir. Dünya durumu, zaman akışı, canlı kararları veya fizik üzerine yazma yetkisi yoktur.

## Girdiler

- Sürümlü dünya olayları.
- Canlı/nesne kimlikleri ve konumları.
- Olay geçmişi ve önem/şiddet bilgisi.
- Kamera görünürlük ve erişilebilirlik bilgisi.
- Ses kaynakları ve çevresel durum.
- Gözlemci kullanıcının mod tercihi (serbest/otomatik/takip).

## Olay sistemi

Örnek olay kategorileri: beslenme, su içme, dinlenme, hareket, keşif, karşılaşma, tehlike, kaçış, çatışma, üreme, doğum, yavru bakımı, yaralanma, ölüm ve çevresel değişim. Olaylar yalnız simülasyon gerçekten ürettiğinde kaydedilir. Olay kimliği, dünya zamanı, katılımcılar, konum, neden/sonuç ilişkisi ve kaynak model sürümü bulunur.

## Yönetmen katmanları

1. **Olay keşfi:** önemli/izlenebilir olayları bulur.
2. **Önceliklendirme:** olayların önemini ve görsel anlatım değerini hesaplar.
3. **Konu seçimi:** canlı, grup veya çevresel olayı seçer.
4. **Çekim planlama:** geniş plan, orta plan, yakın plan, takip, çevre planı.
5. **Kamera kontrolü:** konum, yön, odak, hız, yumuşak geçiş ve engel farkındalığı.
6. **Süreklilik:** önceki planı, olayın durumunu ve izleyici yönelimini korur.
7. **Ses eşleme:** gerçek dünya ses kaynağı ve mekânsal sunum.
8. **Olay arşivi:** çekimlerin hangi gerçek olaylara karşılık geldiğini saklar.

## Sinematik kurallar

- Kamera hızı, ivmesi ve dönüşü sınırlanır; ani sıçrama ve mide bulandırıcı hareket kabul edilmez.
- Kamera fiziksel olarak ulaşamayacağı yere kesmesiz uçamaz; kesme veya farklı kamera modu açıkça kullanılır.
- Canlı örtülüyse kamera gerçek görüş çizgisini dikkate alır; canlıyı göstermek için fiziksel duvar/bitki kaldırılmaz.
- Olay birden fazla aktör içeriyorsa kadraj ilişkiyi anlatır.
- Çekim tercihi olayın önemine, mesafeye ve görünürlüğe göre yapılır.
- Kullanıcı serbest kameraya geçtiğinde yönetmen kontrolü bırakır; dünya etkilenmez.

## Olayın tekrar izlenmesi

Tekrar izleme otoritatif dünya durumunu değiştirmez. Kayıtlı checkpoint/olay günlüğü üzerinden replay view oluşturulur. Aynı olay farklı sanal kamera açılarından incelenebilir. Replay sırasında canlılar yeni karar alıyormuş gibi yanlış gösterilmez; geçmişteki simülasyon durumu yeniden gözlemlenir.

## Kabul testleri

- Yönetmen etkin/kapalı iken aynı seed ve dünya komutları aynı simülasyon sonucunu üretir.
- Yönetmen API'si yazma çağrısı içermez; mimari/derleme testiyle doğrulanır.
- Kamera olayı var etmez ve aktörlere komut göndermez.
- Olay katılımcıları ve zaman damgaları replay ile eşleşir.
- Kamera takip hatası, hız/ivme ve kesme geçişleri ölçülür.
- Görüş engeli/occlusion senaryoları çalışır.
- Olay olmadığı durumda sahte dramatik olay uydurulmaz.
- Görsel referans sahnelerde çekim kriterleri otomatik ölçülür.

## Sınır

Yönetmen “en iyi hikâyeyi” yazmaz. Dünya ne üretiyorsa onu gözlemler. Belirli bir türün hiç gerçekleşmeyen davranışını sırf sinematik sonuç için uyduramaz.
