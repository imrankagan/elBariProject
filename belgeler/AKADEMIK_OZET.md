# ElBâri — Akademik Özet

**Kayıplı telsiz linkleri için bağımsız çerçeveli tamsayı telemetri sıkıştırması**

İmran Kağan · 2025–2026

> Bu belge tez metnine uyarlanmak üzere yazılmıştır. Her sayı ölçülmüştür ve üretim
> komutu ilgili raporda kayıtlıdır. Ölçülmemiş hiçbir iddia içermez; ölçülmemiş olanlar
> §8'de açıkça listelenmiştir.

---

## 1. Özet

İnsansız hava aracı (İHA) telemetrisi, dar bantlı ve **kayıplı** bir radyo bağlantısı
üzerinden akar. Bu alanda yaygın olarak kullanılan fark (delta) tabanlı tamsayı
sıkıştırma yöntemleri yüksek oran verir, ancak **zincirleme bağımlılık** taşırlar: her
değer bir öncekine dayandığı için tek bir paketin düşmesi, o noktadan sonraki tüm verinin
çözülememesine yol açar. Ölçüm bunu doğrulamaktadır — 211 pakete bölünen çerçevesiz bir
akışta %1 paket kaybında akışın tamamının sağ çıkma olasılığı %12'dir; nominal 15,58x'lik
sıkıştırma oranı etkin olarak 1,9x'e iner.

Bu çalışma, PFOR-Delta ailesinden bir tamsayı kodeğini **bağımsız çözülebilir çerçeveler**
üzerine kuran bir telemetri sıkıştırma motoru sunar. Akış, her biri kendi mutlak
referansını taşıyan, sıra numaralı ve CRC32 korumalı çerçevelere bölünür; hata yayılımı
tek çerçeveyle sınırlıdır. Motor iki bağımsız dilde (C ve C#) uygulanmış ve iki
uygulamanın **bit düzeyinde özdeş** çıktı ürettiği dondurulmuş uygunluk vektörleriyle
doğrulanmıştır.

Değerlendirme, sentetik veri yerine **gerçek uçuş logları** üzerinde yapılmıştır: ALFA
veri setinden bir ArduPilot DataFlash logu (yönelim, IMU, GPS, servo, kumanda, titreşim)
ve OpenStreetMap GPS iz arşivi. Kodek, kendi ailesindeki beş referans uygulamayla
(Simple8b, OptPFD, Sprintz-Delta, BP128, StreamVByte) yedi veri setinde karşılaştırılmıştır.

Temel bulgu, oran üstünlüğünün **dar ve veri setine bağlı** olduğudur: yedi setin beşinde
önde, ikisinde %2–7 geride. Buna karşılık, aynı paket kaybı dayanıklılığı bütün kodeklere
verildiğinde sıralama ElBâri lehine belirginleşir — 100 kayıt/çerçevede beş veri setinin
beşinde de lider konuma geçer. Çalışmanın savunulabilir katkısı "en yüksek sıkıştırma
oranı" değil, **oran ile kayıp dayanıklılığı arasındaki ödünleşimin ölçülmesi ve
optimize edilmesidir.**

**Anahtar kelimeler:** telemetri sıkıştırma, PFOR-Delta, paket kaybı dayanıklılığı,
İHA, MAVLink, MISRA C, gömülü sistemler

### Abstract (EN)

Unmanned aerial vehicle (UAV) telemetry travels over narrow-band, **lossy** radio links.
Delta-based integer compression, the standard approach in this domain, achieves high
ratios but carries a **chained dependency**: each value depends on its predecessor, so a
single lost packet renders all subsequent data undecodable. Measurement confirms this —
for an unframed stream split across 211 packets, the probability that the entire stream
survives 1% packet loss is 12%, reducing a nominal 15.58× ratio to an effective 1.9×.

This work presents a telemetry compression engine that builds a PFOR-Delta integer codec
on **independently decodable frames**, each carrying its own absolute reference, sequence
number and CRC32, so that error propagation is bounded to a single frame. The engine is
implemented twice, in C and C#, and the two implementations are verified to produce
**bit-identical** output against frozen conformance vectors.

Evaluation uses **real flight logs** rather than synthetic data: an ArduPilot DataFlash
log from the ALFA dataset and the OpenStreetMap GPS trace archive. The codec is compared
against five reference implementations from its own family across seven datasets.

The principal finding is that the compression-ratio advantage is **narrow and
dataset-dependent** — leading on five of seven datasets, trailing by 2–7% on two. When
the same packet-loss resilience is granted to all codecs, however, the ordering shifts
decisively: at 100 records per frame the proposed engine leads on all five measured
datasets. The defensible contribution is therefore not peak ratio but the **measurement
and optimisation of the trade-off between ratio and loss resilience.**

---

## 2. Problem tanımı

### 2.1 Bağlam

İHA telemetrisinin taşıma ortamı üç kısıtla tanımlanır:

1. **Dar bant.** Tipik SiK/RFD900 sınıfı bir telsizin kullanılabilir yükü paket başına
   ~250 bayttır ve veri hızı onlarca kbit/s mertebesindedir.
2. **Paket kaybı normaldir.** Girişim, çok yollu sönme ve anten yönelimi nedeniyle kayıp
   sürekli ve genellikle **patlamalıdır**.
3. **Gerçek-zaman kısıtı.** Kritik telemetri (kalp atışı, sistem durumu) geciktirilemez;
   sıkıştırma için tamponlama yapılacaksa bunun bir bütçesi vardır.

### 2.2 Mevcut yaklaşımların yetersizliği

**Genel amaçlı sıkıştırıcılar** (zstd, LZ4, Brotli) telemetride zayıf kalır. Ölçülen:
gerçek GPS verisinde zstd-1 1,61x, LZ4 1,29x. Sebep yapısaldır — bu yöntemler veriyi
anlamsız bir bayt dizisi olarak görür, oysa telemetride kanallar iç içe geçmiştir
(`enlem, boylam, irtifa, enlem, …`) ve bu, örüntü aramayı köreltir.

**Tamsayı kodek ailesi** (Simple8b, OptPFD, Sprintz, BP128) bu problemi çözer ve çok daha
iyi oran verir. Ancak **hiçbirinde paket kaybı dayanıklılığı yoktur.** Hepsi tek bir
sürekli akış üretir; tek paket kaybı akışı bitirir.

Literatürdeki boşluk buradadır: *tamsayı kodek ailesinin oran performansını, kayıplı bir
linkte çalışabilirlikle birleştiren bir tasarım.*

---

## 3. Yöntem

### 3.1 Üç katmanlı mimari

Her katman tek bir problemi çözer ve bedeli açıkça ölçülmüştür.

| Katman | Problem | Çözüm | Bedel |
| --- | --- | --- | --- |
| **Çekirdek** | Ardışık farklar küçüktür | Delta + 8'erli bloklarda adaptif bit paketleme + PFOR yamalama | — |
| **Kanal** | Telemetri kayıt akışıdır, sayı akışı değil | Kanallara ayırma, kanal başına adaptif fark derecesi | Kanal başına küçük başlık |
| **Çerçeve** | Paket kaybında zincir kopar | Bağımsız çerçeve + mutlak referans + sıra no + CRC32 | Çerçeve başına ~20 bayt ve oran kaybı |

**Kanal katmanının zorunluluğu ölçülmüştür.** Kayıt akışı doğrudan çekirdeğe verildiğinde
ardışık farklar kanallar arasında zıplar, aykırı değer oranı %100'e çıkar ve veri
*"sıkıştırılamaz"* olarak reddedilir. Bu, öneriye özgü bir zayıflık değildir: kanal ayrımı
olmadan ailenin tamamı çöker — BP128 1,00x, Sprintz 0,97x, Simple8b **0,50x** (veriyi
ikiye katlar). Kanal katmanı bir tasarım tercihi değil, problem sınıfının ön koşuludur.

### 3.2 İki bağımsız uygulama

Motor C# (SIMD hızlandırmalı, sunucu/yer istasyonu hedefli) ve C (bağımlılıksız, RTOS ve
bare-metal hedefli) olarak iki kez uygulanmıştır. Bu bir tekrar değil, **metodolojik bir
araçtır**: iki bağımsız uygulamanın bit düzeyinde aynı çıktıyı üretmesi, biçim
spesifikasyonunun kod ile tutarlı olduğunun doğrudan kanıtıdır. Arayüz kontrol dokümanı
(ICD) yazımında bu, beyan yerine kanıt sağlar.

İkili özdeşliğin iki teknik ön koşulu vardır ve ikisi de tasarıma yazılmıştır:

- **İşaretli taşma kullanılmaz.** C'de işaretli taşma tanımsız davranıştır; tüm fark ve
  toplama işlemleri işaretsiz aritmetik üzerinden yapılır. Bu hem C'de tanımlıdır hem de
  .NET'in `unchecked` davranışıyla birebir aynı sonucu verir.
- **Bayt düzeni açıktır.** Little-endian elle yazılır ve okunur; big-endian işlemcide de
  aynı biçim üretilir.

### 3.3 Ölçüm metodolojisi

Çalışmanın metodolojik duruşu şudur: **her iddia bir komutla yeniden üretilebilmelidir.**

**Gerçek veri.** Sentetik veri sıkıştırma ölçümlerini kolayca yanıltır; fazla düzgün bir
sinyal gerçekçi olmayan yüksek oranlar verir. Bu nedenle değerlendirme, ALFA veri
setinden ([Keipour ve ark., 2021](#kaynaklar)) alınan bir ArduPilot DataFlash logundan
türetilmiştir. Log, MAVROS gibi bir ara katmandan geçmemiş **ham otopilot kaydıdır** —
koordinat çerçevesi çevrilmemiş, birim dönüştürülmemiştir; yani telde giden sayılara en
yakın hâldir.

**Adil kıyas.** Rakip kodeklere kanal ayrımı, fark alma ve zigzag kodlaması **bedava
verilmiş** ve bu ön işlemenin süresi onların encode süresine dâhil edilmiştir. Her
kodeğin kendi çerçeveleme maliyeti boyuta dâhildir.

**Hız iddiasının kurulmaması.** Rakip kodekler, yayımlanmış biçim tanımlarından yeniden
yazılmış skaler C uygulamalarıdır; yazarlarının SIMD'li kütüphaneleri değildir. **Oran**
biçimden gelir ve taşınabilirdir; **hız** taşınabilir değildir. Bu nedenle bu çalışmada
"daha hızlı" iddiası **kurulmamıştır** — hız sütunu rakipler için bir alt sınırdır.

**Kuantalama ölçeklerinin keyfî seçilmemesi.** Ondalıklı log değerleri tamsayıya
çevrilirken MAVLink protokolünün kendi gösterimiyle aynı hassasiyet kullanılmıştır
(yönelim milirad, jiroskop mrad/sn, ivme mg, enlem/boylam 1e-7 derece). Kendi kafamıza
göre bir ölçek seçmek sonucu yanıltırdı: kaba ölçek oranı şişirir, ince ölçek düşürür.

---

## 4. Bulgular

### 4.1 Sıkıştırma oranı — kendi ailesiyle, çerçevesiz

Yedi veri seti, kanal ayrımı senaryosu:

| Veri seti | K | **ElBâri** | Ailenin en iyisi | Fark |
| --- | ---: | ---: | --- | ---: |
| Kumanda girişi (RCIN) | 8 | **92,44x** | Sprintz 74,39x | **+%24,3** |
| Servo çıkışı (RCOU) | 8 | **37,54x** | Sprintz 34,18x | **+%9,8** |
| Yönelim (ATT) | 3 | **15,58x** | Sprintz 15,55x | +%0,2 |
| GPS (ALFA uçuş) | 3 | **5,60x** | Simple8b 5,43x | +%3,1 |
| GPS (OSM referans) | 3 | **5,05x** | Sprintz 4,67x | +%8,1 |
| IMU (jiroskop+ivme) | 6 | 6,88x | Simple8b 7,43x | −%7,4 |
| Titreşim (VIBE) | 3 | 5,38x | Simple8b 5,51x | −%2,4 |

**Yorum.** Fark tek haneli yüzdelerden %24'e kadar değişmekte ve iki veri setinde işaret
değiştirmektedir. Tek bir veri setinden okunan bir yüzde genellenemez. Kaybedilen iki set
(IMU, titreşim) gürültülü çok kanallı veridir; Simple8b'nin 64 bitlik kelime paketlemesi
bu profilde 8'erli blok yapısından daha iyi uyum sağlamaktadır.

### 4.2 Sıkıştırma oranı — çerçeveleme herkese verildiğinde

Rakiplerde çerçeveleme yoktur; dolayısıyla onların çerçevesiz oranıyla bu çalışmanın
çerçeveli oranını karşılaştırmak yöntemsel olarak hatalıdır. Adil karşılaştırma için aynı
yük herkese verilmiştir: akış bağımsız parçalara bölünmüş, her parça bağımsız kodlanmış ve
her parçaya aynı çerçeve başlığı eklenmiştir.

**100 kayıt/çerçeve:**

| Veri seti | **ElBâri** | Sprintz | Simple8b |
| --- | ---: | ---: | ---: |
| Kumanda (RCIN) | **33,44x** | 17,67x | 12,14x |
| Yönelim | **13,24x** | 9,04x | 8,28x |
| IMU | **6,35x** | 5,58x | 6,07x |
| Titreşim | **4,92x** | 4,47x | 4,61x |
| GPS | **4,63x** | 3,94x | 3,83x |

**Bulgu.** Çerçeveleme bütün kodekleri vurur, ancak bu çalışmayı en az vurur. Çerçevesiz
kıyasta kaybedilen iki veri setinde (IMU, titreşim) sıralama tersine dönmektedir. Sebep,
tasarımın küçük bağımsız bloklar üzerine kurulu olmasıdır: 8'erli blok yapısı ve kanal
başına uyarlanan parametreler kısa pencerelerde bilgi kaybetmezken, uzun koşulara dayanan
yöntemler pencere kısaldıkça daha çok kaybeder.

25 kayıt/çerçevede ise bu çalışma %2–4 geridedir; kalan fark çerçeve başına sabit
maliyetten gelmektedir (§4.4).

### 4.3 Paket kaybı dayanıklılığı

Bu, çalışmanın ayırt edici katkısının ölçüldüğü bölümdür.

**Model.** Gerçek RF linkleri bağımsız değil, patlamalı kaybeder. İki durumlu
Gilbert-Elliott Markov zinciri kullanılmıştır: İyi durumda kayıp yok, Kötü durumda her
paket düşer. Hedef kayıp oranı `L` ve ortalama patlama uzunluğu `B` için
`r = 1/B`, `p = L·r/(1−L)`; durgun durumda `P(Kötü) = L` olur.

**Kurtarma tahmin edilmez.** Düşen paketler gerçekten atılır, hayatta kalan çerçeveler
gerçekten çözülür ve geri gelen kayıtlar orijinalle bit düzeyinde karşılaştırılır.

**Taban çizgi.** Çerçevesiz akış (yönelim verisi, 52.646 bayt, MTU 250 → 211 paket):
%1 kayıpta akışın tamamının sağ çıkma olasılığı **%12**. Nominal 15,58x, etkin **1,9x**.

**Hedef fonksiyonu.** Ne tek başına oran ne de tek başına kurtarma yanıttır; yanıt
ikisinin çarpımıdır:

> **etkin oran = (ham bayt × kurtarma oranı) / gönderilen bayt**

yani gönderilen her bayta karşılık alıcıya ulaşan ham veri. Optimum çerçeve boyutu bu
büyüklüğün tepesindedir.

**Üç bulgu:**

**(a) Optimum çerçeve boyutu, kayıp oranı yükseldikçe küçülür.** Yönelim verisinde
%1 → 1000 kayıt, %5 → 500, %10 → 500, %25 → 200. Büyük çerçeve MTU'da parçalanır ve ancak
tüm parçaları ulaşırsa çözülür; hayatta kalma olasılığı paket sayısıyla üstel azalır.

**(b) Patlamalı kayıp, bağımsız kayıptan daha az zarar verir.** Yönelim verisi, %25 kayıp,
500 kayıt/çerçeve: bağımsız kayıpta etkin oran 10,23x, ortalama 10 paketlik patlamalarda
**12,32x**. Sebep, patlamaların hasarı az sayıda çerçevede yoğunlaştırmasıdır; çerçeveler
zaten bağımsız olduğu için toplam hasar azalır. Bu sonuç, zincirleme bağımlılığı olan bir
kodekte tersine dönerdi ve dolayısıyla **çerçeveli tasarımın lehine bir kanıttır.**

**(c) Optimumu belirleyen şey kayıt sayısı değil, çerçevenin kaç pakete bölündüğüdür.**
Üç veri setinde optimum kayıt sayısı 50–1000 arasında savrulurken, optimum paket/çerçeve
oranı %10 kayıpta 1,83–2,30; %25 kayıpta 1,03–1,32 bandında sabit kalmaktadır. Buradan somut
bir tasarım kuralı çıkar: **çerçeve sabit kayıt sayısıyla değil, hedef bayt boyutuyla
doldurulmalıdır.** Böylece politika, verinin sıkışabilirliği değiştiğinde kendiliğinden
uyarlanır.

### 4.4 Biçim evrimi — ölçümün tasarımı yönlendirmesi

Biçim, ölçüm sonuçlarına göre üç kez revize edilmiştir. Her revizyon **önce ölçülmüş,
sonra tasarlanmıştır.**

| Sürüm | Bulgu | Değişiklik | Ölçülen etki |
| ---: | --- | --- | --- |
| 2 | Etiket alanının 8 yuvasının yalnızca 4'ü kullanılıyordu | Bit genişliği tablosu 4 → 8 mod | Kanal ayrılmış GPS'te sürüm 1 3,56x ile Brotli-q11'in (3,88x) gerisindeydi; genişletme bu farkı kapattı |
| 3 | RCIN'de blokların %94,5'i sıfır bloktu ve çıktının **%63'ü etiketti** | Blok üstü sıfır koşusu | RCIN 40,09x → **92,44x**, RCOU 25,64x → 37,54x, ATT 14,70x → 15,58x |
| 4 | 8 kanallı 25 kayıtlık bir çerçevenin ~84 baytı sabit yüktü | Uzunluk tablosu kaldırıldı, başlık 16 → 10 bayt, mutlak referanslar tek blokta | 25 kayıt/çerçevede RCIN **+%94**, yönelim +%49, GPS +%21 |

**Sürüm 3 örneği metodolojik olarak öğreticidir.** Ölçüm, sıfır bloğun veri biti yazmasa
da etiketini yazdığını ve bunun biçime değer başına 0,5 bitlik bir taban, dolayısıyla
32 bitlik değerler için **sert bir tavan** koyduğunu göstermiştir; RCIN'in veri şekli için
bu tavan 63,94x'tir. Sprintz aynı veride 74,39x alıyordu — yani bu çalışmanın *teorik
tavanının üstünde*. Tavan bir ayar
meselesi değil, biçimsel bir kısıttı ve ancak biçim değişikliğiyle kalkabilirdi.

Kaçış kodunun seçimi de kayda değerdir: `mod 0 + aykırı_var 1` ardından maske `0x00`,
önceki sürümün **üretemeyeceği** bir birleşimdir. Bu sayede yeni bir etiket değeri
harcanmamış ve eski akışlar yeni çözücüde aynen çalışmaya devam etmiştir — 29 uygunluk
vektörünün 26'sı bit düzeyinde değişmeden geçmiştir.

### 4.5 Canlı telemetri — iki kademeli MAVLink vekili

Kodek, telsizin iki ucuna yerleştirilen şeffaf bir vekil içinde değerlendirilmiştir.
Kritik mesajlar (kalp atışı, sistem durumu) sıfır gecikmeyle ham geçer; yüksek hızlı
telemetri biriktirilip sıkıştırılır.

Yayın hızları uydurulmamış, ALFA uçuşunun **kendi parametre dökümünden** (`SR0_*`,
`SR1_*`) okunmuştur:

| Hız profili | 2 sn bütçe | 5 sn bütçe |
| --- | ---: | ---: |
| SR1 — telemetri telsizi (dar bant) | 1,16x | **1,68x** |
| SR0 — USB / companion (geniş bant) | **1,71x** | 1,71x |

**Bulgu:** vekil, akış hızı yükseldikçe kazanır. Dar bantta 2 saniyelik bütçeye yalnızca
4–8 kayıt sığar ve çerçeve başlığı bu kadar az kaydın üzerine dağıtılamaz. **Tek bir
kazanç rakamı vermek yanıltıcıdır**; profil belirtilmelidir.

Bu bölüm ayrıca bir yöntem uyarısı içerir: sentetik üreteç gürültüyü bilerek yüksek
tuttuğu için kötümser taraftaydı. Gerçek uçuş verisi bağlandığında SCALED_IMU mesajının
kazancı 1,77x → **3,24x**, ATTITUDE'un 4,06x → **6,44x** çıkmıştır.

---

## 5. Doğrulama

| Yöntem | Kapsam | Sonuç |
| --- | --- | --- |
| Dondurulmuş uygunluk vektörleri | 29 vektör, 58 kontrol | 0 hata |
| İkili özdeşlik (C ↔ C#) | Üç katman, gerçek veri | Bit düzeyinde aynı |
| Kodlayıcı değer fuzz'ı | 12 düşmanca dağılım × 300.000 tur | 0 kayıp |
| Çözücü sağlamlık fuzz'ı | 300.000 tur, kanarya korumalı | 0 taşma, bozuk çerçeve reddi %100 |
| ASan + UBSan | Her push, CI | Temiz |
| MISRA C:2012 | Elle inceleme + iki Cppcheck sürümü | Kayıtlı 2 sapma dışında 0 bulgu |
| .NET test paketi | 32 senaryo | 0 hata |

### 5.1 Metodolojik bulgu: gerçek verinin bulduğu hata

Çalışmanın en öğretici sonucu bir doğrulama boşluğudur.

Gerçek IMU verisi vekile bağlandığında, kayıpsız modda tam tur doğrulaması düşmüştür.
İzlendiğinde çekirdek kodekte gerçek bir **kayıpsızlık hatası** bulunmuştur: ardışık iki
değerin farkı tam olarak `INT32_MIN` olduğunda, 32 bitlik mutlak değer negatif kalıyor;
bu yüzden fark ne "aykırı" işaretleniyor ne de blok bit genişliğini yükseltiyor, sonra
dar maskeyle paketlenip **üst bitini kaybediyordu.**

Pratikte bu, bir kayan nokta değerinin işareti değişip büyüklüğü aynı kaldığında oluşur
(`+0.001f` → `-0.001f`).

**Neden daha önce bulunamadı:** o tarihteki 27 uygunluk vektörünün hiçbirinde 2³¹'lik
fark yoktu; mevcut fuzz ise bir **çözücü sağlamlık** testiydi — bozuk girdiyi çözücüye
verir, rastgele *değerleri* kodlayıp geri okumazdı. Sentetik yönelim verisi sıfırı
geçiyordu, ancak işaret değişimi hep büyüklük değişimiyle birlikte oluyordu; tam olarak
2³¹ hiç çıkmamıştı. **Bu deseni ancak gerçek jiroskop verisi üretmiştir.**

Hata C ve C# sürümlerinde düzeltilmiş, regresyon vektörü eklenmiş ve boşluğu kapatmak
için **kodlayıcı değer fuzz'ı** yazılmıştır. Yeni araç, düzeltme geçici olarak geri
alındığında hatayı 20.000 turda yakalamaktadır.

Bu, sentetik veriyle doğrulamanın sınırı hakkında somut bir kanıttır ve tez metninde
metodolojik bir bulgu olarak kullanılabilir.

### 5.2 İkinci metodolojik düzeltme: adil olmayan karşılaştırma

Çalışmanın erken bir aşamasında *"çerçeveleme açıldığında oran liderliği kaybediliyor"*
sonucuna varılmıştı. Bu sonuç **yöntemsel olarak hatalıydı**: bu çalışmanın çerçeveli
oranını, rakiplerin çerçevesiz oranıyla karşılaştırıyordu. Rakiplerde çerçeveleme
olmadığı için o bedeli hiç ödemiyorlardı.

Aynı yük herkese verildiğinde (§4.2) sonuç tersine döndü. Bu düzeltme, karşılaştırmalı
değerlendirmede **kısıtların simetrik uygulanması** ilkesinin somut bir örneğidir.

---

## 6. Katkının konumlandırılması

Bu çalışma **yeni bir algoritma önermez.** Kullanılan teknikler — delta kodlama, Frame of
Reference, PFOR yamalama, bit paketleme — literatürde yerleşiktir ve prior art
oluşturmaktadır.

Katkı üç başlıkta toplanabilir:

**(1) Mimari katkı.** PFOR-Delta ailesinin oran performansını bağımsız çerçevelerle
birleştirmek ve bu birleşimin **bedelini ölçmek**. Ailenin hiçbir üyesinde paket kaybı
dayanıklılığı yoktur; bu çalışma o boşluğu doldurur ve maliyetini gizlemez.

**(2) Ölçüm katkısı.** Kayıp × çerçeve boyutu süpürmesi ve "etkin oran" hedef fonksiyonu.
Bu, çerçeve boyutu seçimini sezgiden çıkarıp ölçülebilir bir optimizasyon problemine
dönüştürür. Buradan çıkan tasarım kuralı — *çerçeveyi baytla ölç, kayıtla değil* —
literatürde bu netlikte formüle edilmemiştir.

**(3) Metodolojik katkı.** İki bağımsız uygulamanın bit düzeyinde özdeşliğinin biçim
spesifikasyonu için kanıt olarak kullanılması; ve gerçek uçuş verisinin, sentetik veriyle
ve mevcut test takımıyla bulunamayan bir kayıpsızlık hatasını ortaya çıkarması.

**Katkı olmayan:** en yüksek sıkıştırma oranı. Bu açıkça belirtilmelidir.

---

## 7. Mimari kısıt: entropi kodlaması neden eklenemez

Teorik alt sınır analizi yapılmıştır ve önemli bir sonuç vermektedir.

Gerçek GPS verisi için:

| Sınır | Oran |
| --- | ---: |
| Bu çalışmanın çıktısı | 5,05x |
| Bit paketleme tabanı (etiket bedava) | 5,56x |
| Shannon entropisi (model bedava) | 6,30x |
| **Entropi + frekans tablosu** | **4,54x** |

Entropi kodlaması, frekans tablosunun maliyeti hesaba katıldığında bu veri setinde
**daha kötü** sonuç vermektedir; kanallarda 1.715–2.063 farklı sembol bulunmaktadır.

Tablo maliyetinden kaçmanın yolu uyarlanabilir model kullanmaktır — ancak bu, çözücünün
*önceki tüm veriyi görmüş olmasını* gerektirir ve **bağımsız çerçeveleri yok eder.**

> **Entropi kodlaması ile paket kaybı dayanıklılığı temelde uyuşmaz.** Bu, bir eksiklik
> değil, tasarımın kabul ettiği bir kısıttır ve tez metninde böyle konumlandırılmalıdır.

---

## 8. Sınırlar

Bu bölüm bilinçli olarak ayrı tutulmuştur.

| Sınır | Etkisi |
| --- | --- |
| **Hız iddiası kurulmamıştır** | Rakipler yeniden yazılmış skaler uygulamalardır; SIMD kütüphaneleri bağlanmamıştır. Hız sütunu rakipler için bir alt sınırdır |
| **Tek uçuş, tek platform** | Uçuş fikstürlerinin tamamı aynı ALFA uçuşundandır. Platform **sabit kanattır**; yönelimi bir çoklu rotordan düzgündür, dolayısıyla ATT/IMU oranları çoklu rotor telemetrisine göre **iyimserdir** |
| **ARM ve big-endian doğrulanmadı** | Kod derlenmektedir; hız ve davranış gerçek donanımda ölçülmemiştir |
| **RTOS üzerinde WCET analizi yok** | Gecikme dağılımı genel amaçlı bir işletim sisteminde ölçülmüştür; üst değerler büyük ölçüde işletim sistemi gürültüsüdür |
| **Sertifikalı MISRA aracı kullanılmadı** | Açık kaynak Cppcheck temiz geçmektedir; nitelikli araç raporu yoktur. (Not: *"MISRA sertifikası"* diye bir belge yoktur; uyum beyandır ve kanıtla desteklenir) |
| **Gerçek telsizle uçtan uca test yok** | Kayıp altında kurtarma simülasyonla ölçülmüştür |
| **İleri hata düzeltme (FEC) modellenmedi** | FEC eklendiğinde optimum çerçeve boyutu değişir; bu ölçüm FEC'siz tabanı verir |

---

## 9. Gelecek çalışma

Öncelik sırasıyla:

1. **Kanal başına profil seçimi.** Ölçüm, iki israf kaynağının birbirine zıt çalıştığını
   göstermektedir: düzgün veride etiket alanı israf eder (yönelimde entropi 1,98 bit,
   harcanan 4 bit), gürültülü veride bit genişliği tablosu israf eder (%3–8). Kanal
   başlığına eklenecek iki bitlik bir profil seçici her kanalın kendi profilini seçmesini
   sağlayabilir. Projeksiyon: IMU ve titreşimdeki kalan açığın kapanması.
2. **Rakiplerin SIMD kütüphanelerinin bağlanması.** Hız iddiası ancak bundan sonra
   kurulabilir.
3. **Çoklu rotor uçuş verisi.** Mevcut sonuçlar sabit kanat için iyimserdir.
4. **Gömülü ARM üzerinde ölçüm** ve RTOS'ta WCET analizi.
5. **FEC ile birlikte optimizasyon.** Kayıp süpürmesine beşinci eksen olarak eklenmesi.

---

## 10. Yeniden üretilebilirlik

Bütün ölçümler depoda bulunan araçlarla yeniden üretilebilir. Her tablonun üretim komutu
[OLCUM_SONUCLARI.md](OLCUM_SONUCLARI.md) içinde kayıtlıdır; üretilemeyen iki tablo da
orada açıkça işaretlenmiştir.

| Rapor | İçerik |
| --- | --- |
| [BICIM_SPESIFIKASYONU.md](BICIM_SPESIFIKASYONU.md) | Bayt düzeyinde tam tanım (ICD) |
| [KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md) | Kendi ailesiyle kıyas, yedi veri seti |
| [KAYIP_DAYANIKLILIK.md](KAYIP_DAYANIKLILIK.md) | Kayıp × çerçeve boyutu süpürmesi |
| [MAVLINK_VEKIL.md](MAVLINK_VEKIL.md) | Canlı telemetri senaryosu |
| [TEST_VE_DOGRULAMA.md](TEST_VE_DOGRULAMA.md) | Doğrulama envanteri ve bilinen boşluklar |
| [MISRA_UYUM.md](MISRA_UYUM.md) | Kural kural uyum matrisi ve sapma kaydı |
| [OLCUM_SONUCLARI.md](OLCUM_SONUCLARI.md) | Birincil sayı kaynağı |

**Veri seti erişimi.** ALFA veri seti depoya dâhil değildir (açılınca 12,5 GB) ve kendi
lisansına tabidir. Türetilmiş fikstürler `c/veri/donustur.exe` ile bir ArduPilot `.bin`
logundan üretilir.

---

## Kaynaklar

- Anh, V. N., & Moffat, A. (2010). *Index compression using 64-bit words.*
  Software: Practice and Experience, 40(2), 131–147. **[Simple8b]**
- Blalock, D., Madden, S., & Guttag, J. (2018). *Sprintz: Time series compression for
  the Internet of Things.* Proceedings of the ACM on Interactive, Mobile, Wearable and
  Ubiquitous Technologies, 2(3). **[Sprintz-Delta]**
- Gilbert, E. N. (1960). *Capacity of a burst-noise channel.* Bell System Technical
  Journal, 39(5), 1253–1265. **[patlamalı kayıp modeli]**
- Keipour, A., Mousaei, M., & Scherer, S. (2021). *ALFA: A dataset for UAV fault and
  anomaly detection.* The International Journal of Robotics Research.
  DOI: 10.1177/0278364920966642 **[uçuş verisi]**
- Lemire, D., & Boytsov, L. (2015). *Decoding billions of integers per second through
  vectorization.* Software: Practice and Experience, 45(1), 1–29. **[BP128]**
- Lemire, D., Kurz, N., & Rupp, C. (2018). *Stream VByte: Faster byte-oriented integer
  compression.* Information Processing Letters, 130, 1–6. **[StreamVByte]**
- MISRA Consortium (2012). *MISRA C:2012 — Guidelines for the use of the C language in
  critical systems.*
- Pelkonen, T., ve ark. (2015). *Gorilla: A fast, scalable, in-memory time series
  database.* Proceedings of the VLDB Endowment, 8(12), 1816–1827. **[XOR katmanı]**
- Yan, H., Ding, S., & Suel, T. (2009). *Inverted index compression and query processing
  with optimized document ordering.* WWW '09. **[OptPFD]**
- Zukowski, M., Heman, S., Nes, N., & Boncz, P. (2006). *Super-scalar RAM-CPU cache
  compression.* ICDE '06. **[PFOR / patching]**

---

**© 2025-2026 İmran Kağan.** Akademik ve eğitim amaçlı kullanım, inceleme ve atıf
serbesttir — tez çalışmaları dâhil. Ayrıntı: [LICENSE.txt](../LICENSE.txt)
