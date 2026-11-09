# 05 — Fizik motoru

## Hedef

Kendi C17 fizik sistemimiz; çevre, zemin, nesneler ve canlı bedenleri için tutarlı kuvvet, temas, kısıt ve hareket hesapları sağlar. Tam genel amaçlı bilimsel fizik çözücüsü olduğu iddia edilmez; her alt modelin kapsamı belgelenir.

## İlk release fizik kapsamı

- Yerçekimi ve temel kuvvetler.
- Rijit cisim kinematiği/dinamiği.
- Çarpışma tespiti ve temas çözümü.
- Sürtünme ve restitüsyon parametreleri.
- Kısıtlar ve eklem bağlantıları.
- Zemin temas modeli.
- Canlı bedenleri için çok segmentli rijit cisim yapısı.
- Rüzgâr gibi çevresel kuvvetler.
- Sınırlı yumuşak doku/deformasyon (yalnız gerekli referans türde).

## Sayısal yöntem seçimi

Solver veya entegratör adı önceden dogma yapılmaz. Her problem için uygun yöntem karşılaştırılır: ör. semi-implicit/symplectic Euler, Verlet, Runge–Kutta ailesi, constraint solver, impulse-based veya penalty contact. Seçim kararlılık, enerji davranışı, doğruluk, hız ve referans testine göre kayıt altına alınır.

Fizik dt'si, beden modeli ve temas çözümünün kararlılığını bozacak biçimde render FPS'ye bağlanamaz. Gerekirse sabit alt adımlar ve adaptif substep kullanılır; testlerde minimum/maximum dt ve solver iterasyon bütçeleri ölçülür.

## Fiziksel birimler

Tüm model parametreleri SI birimleriyle tutulur veya açık dönüşümle tanımlanır. Konum metre, kütle kilogram, zaman saniye, kuvvet Newton, enerji Joule ve basınç Pascal gibi birimlerle belgelenir. Birim dönüşümleri merkezi ve testli olur.

## Kabul testleri

- Serbest düşüş analitik çözümle hata karşılaştırması.
- Eğik düzlem/sürtünme referans deneyi.
- Momentum ve enerji değişiminin beklenen sınırda olması.
- Çarpışma penetrasyonu ve dinlenme kararlılığı.
- Farklı dt ve solver iterasyonlarında yakınsama davranışı.
- Büyük koordinat ve çok küçük kütle/ölçek sınırları.
- NaN/Inf, aşırı hız ve bozuk parametre reddi.
- Beden eklem zinciri ve zemin teması referans testi.
- Uzun süreli simülasyonda enerji/konum patlaması olmaması.

## Fizik görselleştirmeden bağımsızdır

Fizik modülü headless olarak çalışır. Render görüntüsü fizik durumundan üretilir ama fizik doğru sayılmak için render testine dayanmaz. Görsel hareket testleri fizik testlerinin yerine geçmez.

## İlk sürüm dışında

Tam CFD, çok fazlı akışkanlar, kırılma/yangın kimyası, atomistik moleküler dinamik, her malzemenin sonlu eleman modeli ve gezegen tektoniğinin yüksek çözünürlüklü çözümü ilk release kapsamı değildir. Bunlar seçilmiş alt problemlerde araştırılabilir.
