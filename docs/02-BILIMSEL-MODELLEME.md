# 02 — Bilimsel modelleme ve doğrulama standardı

## Amaç

Worldseed Engine bilimsel olarak temellendirilmiş modeller kullanır; hiçbir model yalnızca inandırıcı görüntü verdiği için “gerçekçi/bilimsel” sayılmaz.

## Zorunlu model kartı

Her fiziksel, biyolojik, ekolojik veya çevresel alt model için aşağıdaki kart tutulur:

1. Model kimliği, sürümü ve sorumlu modül.
2. Temsil edilen gerçek süreç ve kullanım amacı.
3. Uzamsal ölçek (mm, cm, m, km vb.) ve zamansal ölçek (ms, s, dk, saat, gün vb.).
4. Durum değişkenleri, SI birimleri ve veri türleri.
5. Kullanılan denklemler/algoritmalar ve kaynaklar.
6. Varsayımlar, ihmal edilen süreçler ve geçerlilik aralığı.
7. Parametrelerin kaynağı, kalibrasyon yöntemi ve belirsizliği.
8. Sayısal yöntem, zaman adımı, sınır koşulları ve kararlılık kısıtları.
9. Referans veri/analitik çözüm/deney ile doğrulama planı.
10. Kabul eşikleri, hata metrikleri ve bilinen sınırlamalar.
11. Model değişikliğinde kayıt biçimi ve dünya sürümüne etkisi.

Bu kart olmadan model “bilimsel doğrulandı” diye sunulamaz.

## Kanıt hiyerarşisi

- **D0 — Kavramsal:** fikir veya görsel; bilimsel doğruluk iddiası yok.
- **D1 — Matematiksel:** denklemler, birimler, boyut analizi ve sınır davranışları kontrol edildi.
- **D2 — Sayısal:** analitik çözüm, korunum, yakınsama ve kararlılık testleri geçti.
- **D3 — Referans:** yayınlanmış deneysel/ölçülmüş veri veya güvenilir referans modelle karşılaştırıldı.
- **D4 — Sistem:** diğer dünya modelleriyle entegre edildi; uzun süre ve çoklu senaryo testleri geçti.

Her modelin seviyesi açıkça kaydedilir. D1 düzeyindeki bir model D3 diye tanıtılmaz.

## Bilimsel doğrulama türleri

- Birim ve boyut kontrolü.
- Kütle, momentum, enerji, su ve besin dengesi.
- Sınır koşulu testleri.
- Sayısal kararlılık ve zaman adımı hassasiyeti.
- Uzamsal çözünürlük yakınsaması.
- Referans deney/ölçümle hata karşılaştırması.
- Parametre duyarlılığı ve belirsizlik analizi.
- Çoklu tohum ve farklı başlangıç koşulları.
- Uzun süreli sistem davranışı.

## Bilimsel kaynak politikası

- Öncelik hakemli makale, bilimsel veri seti, resmi teknik doküman ve üniversite/araştırma kurumu kaynaklarıdır.
- Her model kartı kaynak listesini ve erişim tarihini içerir.
- Kod veya veri seti kopyalanmayacak; lisans ve atıf koşulları araştırılmadan içerik alınmayacak.
- Başka bir simülatörün varlığı, kodunun veya kütüphanesinin kullanılmasına izin vermez. Bu projede yöntemsel referans alınır; uygulama C ile sıfırdan yazılır.
- Çelişen bilimsel sonuçlar varsa tek gerçek gibi sunulmaz; belirsizlik ve seçilen varsayım belgelenir.

## Ölçek ve ayrıntı seviyesi

Her fiziksel süreç aynı ayrıntıda çözülmek zorunda değildir. Örneğin arazi akışı, organizma fizyolojisi, kas-iskelet sistemi ve ekolojik popülasyon farklı zaman/uzay ölçekleri kullanır. Katmanlar arasında aktarım sözleşmesi açık olmalıdır.

Yakınlaştırılmış fiziksel alt modelin toplulaştırılmış üst modele geçişi durum/enerji/kütle kaybını kontrol etmelidir. Bir model ayrıntı seviyesi değiştirirken ani konum, hız, enerji veya popülasyon sıçraması oluşturamaz.

## Bilimsel olmayan görsel hileler

- Canlı animasyonunu gerçek fizik sonucu gibi göstermek yasaktır.
- Simülasyonda gerçekleşmeyen davranış kamera için üretilmez.
- Çevresel efektler altta tanımlı süreç olmadan bilimsel neden-sonuç iddiasıyla sunulmaz.
- Görsel güzellik doğruluk kanıtı değildir.

## Araştırma görevlerinin çıktısı

Her araştırma işi şu dosyaları üretir: `MODEL-KARTI.md`, kaynak listesi, parametre tablosu, birim/ölçek tanımı, test planı, bilinen sınırlamalar ve uygulama önerisi. Sonra C uygulaması ayrı PR'da yapılır.
