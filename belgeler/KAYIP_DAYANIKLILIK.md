# Paket Kaybı Altında Kurtarma — Çerçeve Boyutu Süpürmesi

> Ölçüm kodu: [`c/test/kayip.c`](../c/test/kayip.c) ·
> Çalıştırma: `kayip <fikstür.bin> [mtu]` ·
> Sayılar **biçim sürümü 4** iledir.

## 1. Neden bu ölçüm

ElBâri'nin ailesindeki (Simple8b, OptPFD, Sprintz, BP128) **hiçbir üyede paket kaybı
dayanıklılığı yoktur**. Oran yarışında ElBâri yedi veri setinin beşinde önde, ikisinde
(IMU, titreşim) %2-7 geride ([kıyas raporu](KIYAS_TAMSAYI_KODEKLER.md)) — yani fark
ölçülü. Projenin asıl ayırt edici katkısı oran değil, **kayıplı linkte çalışabilmek**.

Ama o katkı bugüne kadar **tek bir işletim noktasında** ölçülmüştü: 100 kayıt/çerçeve.
Elde bir nokta vardı, eğri yoktu — dolayısıyla "çerçeve boyutu kaç olmalı?" sorusunun
ölçülmüş bir yanıtı da yoktu.

Bu belge eğriyi çıkarır.

## 2. Çelişki — optimumu bu belirler

| Çerçeve | Etkisi |
| --- | --- |
| **Büyük** | Başlık payı küçülür, **oran artar**. Ama çerçeve MTU'ya sığmayıp birden çok pakete bölünür ve ancak **tüm parçaları** ulaşırsa çözülür — hayatta kalma olasılığı paket sayısıyla **üstel** azalır. |
| **Küçük** | Her çerçeve tek pakete sığar, bağımsız hayatta kalır. Ama başlık payı büyür, **oran düşer**. |

Oran ve dayanıklılık ters yönde çalışır. Bu yüzden tek başına oran da, tek başına
kurtarma oranı da yanıt değildir. Yanıt ikisinin çarpımıdır:

> **etkin oran = (ham bayt × kurtarma oranı) / gönderilen bayt**
>
> Yani *gönderilen her bayta karşılık alıcıya ulaşan ham veri.*

Optimum çerçeve boyutu, bu sütunun tepesindedir.

## 3. Metodoloji

| | |
| --- | --- |
| Veri | ALFA uçuş logundan üretilen fikstürler + OSM GPS izleri |
| Telsiz MTU'su | 250 bayt (SiK / RFD900 sınıfı kullanılabilir yük) |
| Tur | 20 (istatistiksel ortalama) |
| Kayıp modeli | Gilbert-Elliott iki durumlu Markov zinciri |
| Kurtarma | **Tahmin edilmez.** Düşen paketler gerçekten atılır, hayatta kalan çerçeveler gerçekten çözülür, geri gelen kayıtlar orijinalle **bit bit** karşılaştırılır. Sayılan şey doğru çözülmüş kayıt sayısıdır. |

**Kayıp modeli.** Gerçek RF linkleri bağımsız değil, **patlamalı** kaybeder (girişim,
çok yollu sönme, anten yönelimi). İyi durumda kayıp yok, Kötü durumda her paket düşer.
Hedef kayıp oranı `L` ve ortalama patlama uzunluğu `B` için:

```
r = 1/B              (Kötü'den çıkma olasılığı)
p = L·r / (1−L)      (Kötü'ye girme olasılığı)
→ durgun durumda P(Kötü) = p/(p+r) = L
```

Patlama uzunluğu 1 verildiğinde model Bernoulli'ye (bağımsız kayıp) yaklaşır.

## 4. Taban çizgi — çerçevesiz klasik yaklaşım

Delta kodlamayı çerçevelemeden kullanmak, kayıplı linkte **çalışmaz**:

| Veri | Boyut | Oran | Paket | %1 kayıpta hayatta kalma |
| --- | ---: | ---: | ---: | ---: |
| Yönelim (ATT) | 52.646 B | 15,58x | 211 | **%12,0** |
| GPS | 58.513 B | 5,05x | 235 | **%9,5** |

Tek bir paket düşerse akışın tamamı çözülemez. **%1 kayıpta akışın onda dokuzu
tamamen gider.** Nominal 15,58x'lik oran, etkin olarak **1,9x**'e iner.

Tablodaki bütün çerçeveli değerler bunun üstünde. Karşılaştırma buradan başlamalı.

## 5. Sonuç — yönelim verisi (ATT, 3 kanal, 68.349 kayıt)

Bağımsız kayıp (patlama uzunluğu 1):

| kayıt/çerçeve | gönderilen | oran | pkt/çrç | %1 | %5 | %10 | %25 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 5 | 413.948 | 1,98x | 1,00 | 1,96x | 1,88x | 1,78x | 1,49x |
| 10 | 225.138 | 3,64x | 1,00 | 3,61x | 3,46x | 3,28x | 2,73x |
| 25 | 117.562 | 6,98x | 1,00 | 6,90x | 6,63x | 6,27x | 5,26x |
| 50 | 81.360 | 10,08x | 1,00 | 9,98x | 9,58x | 9,10x | 7,56x |
| 100 | 61.960 | 13,24x | 1,00 | 13,10x | 12,50x | 11,95x | 9,93x |
| 200 | 52.389 | 15,66x | 1,27 | 15,47x | 14,71x | 13,66x | **10,73x** |
| 500 | 47.476 | 17,28x | 1,82 | 16,92x | **15,89x** | **14,48x** | 10,23x |
| 1000 | 44.894 | 18,27x | 3,10 | **17,74x** | 15,52x | 12,98x | 7,49x |

> **Biçim sürümü 4'ün etkisi burada en net görülüyor.** Çerçeve başına sabit maliyet
> ~84 → ~20 bayta indiği için küçük çerçeveler artık cezalandırılmıyor: 25
> kayıt/çerçeve 4,69x → **6,98x**, 5 kayıt/çerçeve 1,17x → **1,98x**. Eğri düzleşti,
> yani yanlış çerçeve boyutu seçmenin bedeli de azaldı.

*Sayılar etkin orandır. Kalın = o kayıp oranındaki optimum.*

**Optimum çerçeve boyutu, kayıp oranı yükseldikçe küçülüyor:** 1000 → 500 → 500 → 200.

## 6. Patlamalı kayıp bağımsız kayıptan İYİDİR

Sezgiye aykırı ama ölçülmüş sonuç. Yönelim verisi, %25 kayıp:

| kayıt/çerçeve | patlama 1 | patlama 3 | patlama 10 |
| ---: | ---: | ---: | ---: |
| 200 | 10,73x | 11,39x | **11,47x** |
| 500 | 10,23x | 12,07x | **12,32x** |
| 1000 | 7,49x | 10,65x | **14,29x** |

Sebep: **patlamalar kayıpları yoğunlaştırır.** Aynı toplam kayıp oranı bağımsız
dağıldığında daha çok sayıda *ayrı* çerçeveye dokunur; patlamalı dağıldığında birkaç
çerçeveyi tamamen götürür, geri kalanı hiç etkilemez. Çerçeveler zaten bağımsız olduğu
için ikinci senaryo daha az hasar verir.

Bu, çerçeveli tasarımın *lehine* bir sonuçtur ve klasik "patlamalı kayıp daha kötüdür"
sezgisinin bu mimaride geçerli olmadığını gösterir. Zincirleme bağımlılığı olan bir
kodekte durum tersine dönerdi.

## 7. Asıl kural: çerçeveyi KAYITLA değil, BAYTLA ölç

Üç veri setinin optimumları kayıt cinsinden birbirini tutmuyor:

| Kayıp | ATT optimum | GPS optimum | IMU optimum |
| ---: | ---: | ---: | ---: |
| %1 | 1000 kayıt | 1000 kayıt | 500 kayıt |
| %5 | 500 | 200 | 200 |
| %10 | 500 | 100 | 100 |
| %25 | 200 | 50 | 50 |

Aynı optimumlara **çerçeve başına paket** cinsinden bakınca tablo toparlanıyor:

| Kayıp | ATT | GPS | IMU |
| ---: | ---: | ---: | ---: |
| %10 | 1,95 pkt | 1,83 pkt | 2,30 pkt |
| %25 | 1,28 pkt | 1,03 pkt | 1,32 pkt |

**Optimumu belirleyen şey kayıt sayısı değil, çerçevenin kaç pakete bölündüğüdür.**
Kayıt sayısı yalnızca bir vekildir ve verinin sıkışabilirliğine göre kayar: yönelim
verisi o kadar iyi sıkışır ki 1000 kayıt hâlâ 3,3 pakete sığar; GPS'te aynı 1000 kayıt
**10 pakete** bölünür ve çöker.

> ### Tasarım önerisi
>
> Çerçeveyi sabit kayıt sayısıyla değil, **hedef bayt boyutuyla** doldurun. Kodlayıcı
> çerçeveyi MTU'nun katına yaklaşana kadar beslesin, sonra kapatsın. Böylece aynı
> politika bütün veri tiplerinde optimuma yakın kalır; sıkışabilirlik değiştiğinde
> kendiliğinden uyarlanır.
>
> Kayıp oranına göre hedef: **yüksek kayıpta (≥%10) çerçeve ~1 pakete**, düşük kayıpta
> (≤%1) gecikme bütçesinin izin verdiği kadar büyük.

## 8. Düşük kayıpta eğri düzleşiyor

%1 kayıpta ATT için 500 → 16,92x, 1000 → 17,74x.
Yani düşük kayıpta çerçeve boyutu seçimi **neredeyse fark etmiyor** ve sınırlayan şey
kayıp değil, gecikme bütçesi ile MTU oluyor.

Pratik sonuç: çerçeve boyutunu kayıp oranına göre ayarlamak yalnızca **kötü linklerde**
anlamlıdır. İyi linkte kararı gecikme verir.

## 9. Sınırlar

1. **Tek uçuş, tek platform.** Fikstürler aynı ALFA uçuşundan (sabit kanat) geliyor.
2. **MTU sabit varsayıldı** (250 bayt). SiK radyoları bağlantı kalitesine göre veri
   hızını değiştirir; değişken MTU modellenmedi.
3. **Yeniden gönderim (ARQ) yok.** Ölçüm saf yayın (broadcast) varsayar. MAVLink
   telemetri linki pratikte böyledir, ama komut kanalı değildir.
4. **Gilbert-Elliott iki durumludur.** Gerçek sönme kanalları daha zengindir; model
   patlamayı yakalar ama sönme derinliğinin sürekli dağılımını yakalamaz.
5. **İleri hata düzeltme (FEC) yok.** FEC eklendiğinde optimum çerçeve boyutu değişir;
   bu ölçüm FEC'siz tabanı verir.

## 10. Bu ölçüm neyi değiştiriyor

| Önce | Sonra |
| --- | --- |
| "Paket kaybına dayanıklıyız" — tek nokta (100 kayıt/çerçeve) | Kayıp × çerçeve boyutu eğrisi çıkarıldı; optimum ölçülebilir hâle geldi |
| Çerçeve boyutu üç kısıtla seçiliyordu (oran, gecikme, paket) | Dördüncü kısıt (kurtarma) eklendi; **etkin oran** tek bir hedef fonksiyonu veriyor |
| "100 kayıt/çerçeve" varsayılanı gerekçesizdi | Gerekçe artık ölçülmüş: %10 kayıpta ATT için optimum 500, GPS için 100 |
| Çerçeve boyutu kayıtla ifade ediliyordu | **Baytla ifade edilmeli** — optimum, paket sayısına bağlı |

---

*Ölçüm: [`c/test/kayip.c`](../c/test/kayip.c) · Fikstür üretimi:
[`c/veri/`](../c/veri/) · Kıyas bağlamı: [KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md)*
