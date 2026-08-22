# Mimari — Neden Üç Katman?

> Bayt düzeyindeki tanım: [BICIM_SPESIFIKASYONU.md](BICIM_SPESIFIKASYONU.md) ·
> Görsel akış: [AKIS_SEMASI.md](AKIS_SEMASI.md)

Bu belge **hangi katmanın neden var olduğunu** anlatır. Her katman bir problemi çözmek
için eklenmiştir; hiçbiri süs değildir ve her birinin bir bedeli vardır.

---

## Kısa cevap

| Katman | Çözdüğü problem | Bedeli |
| --- | --- | --- |
| **Çekirdek** | Ardışık tamsayılar arasındaki fark küçüktür; bunu az bitle yaz | — |
| **Kanal** | Telemetri tek sayı akışı değil, **kayıt** akışıdır | Kanal başına küçük bir başlık |
| **Çerçeve** | Telsiz paketi düşer; zincir kopunca her şey gider | Çerçeve başına ~20 bayt + oran kaybı |

Üçünü **birlikte** kullanmak zorunda değilsiniz. Güvenilir bir taşıma (TCP, dosya)
üzerinde çalışıyorsanız çerçeve katmanına gerek yoktur ve oranınız daha iyi olur.

---

## 1. Çekirdek katmanı — farkları paketle

**Problem.** Telemetri değerleri komşularına yakındır. Enlem `400000135`'ten
`400000270`'e gider; ikisi de 32 bit yer kaplar ama aralarındaki fark 135'tir ve
8 bite sığar.

**Çözüm.** Akışın başına bir **mutlak referans** yazılır, sonrası ardışık farklarla
kodlanır. Farklar 8'erli **bloklara** ayrılır; her blok için o bloğun en büyük farkını
taşıyacak en dar bit genişliği seçilir ve 4 bitlik bir **etiketle** bildirilir.

```
[mutlak referans 32 bit][blok][blok][blok]...

blok = [etiket 4 bit][(aykırı maskesi 8 bit)][farklar n×w bit][aykırılar m×32 bit]
```

**Neden blok başına genişlik?** Tüm akış için tek bir genişlik seçmek, tek bir büyük
sıçramanın bütün akışı geniş bit alanına mahkûm etmesi demekti. Blok başına seçim,
sakin bölgelerin dar kalmasını sağlar.

**Peki tek bir aykırı değer bloğu bozarsa?** Blok içindeki büyük farklar "aykırı" olarak
işaretlenip ayrıca 32 bitle yazılır; kalan farklar dar genişlikte kalır. Literatürde
bu **PFOR-patching** olarak bilinir.

### Bit genişliği tablosu

| `mod` | Genişlik | Kapsadığı en büyük fark |
| ---: | ---: | ---: |
| 0 | — | 0 — **sıfır blok**, veri biti yazılmaz |
| 1 | 2 bit | 1 |
| 2 | 3 bit | 3 |
| 3 | 4 bit | 7 |
| 4 | 5 bit | 15 |
| 5 | 8 bit | 127 |
| 6 | 10 bit | 511 |
| 7 | 16 bit | 32.767 |

Bundan büyük farklar aykırı sayılır ve 32 bitle yazılır.

### Blok üstü sıfır koşusu *(biçim sürümü 3)*

Sıfır blok veri biti yazmaz ama **etiketini yine yazar**. Bu, biçime değer başına
0,5 bitlik bir taban maliyeti ve 32 bitlik değerler için **64x'lik sert bir tavan**
koyuyordu.

Ölçüldü: gerçek bir kumanda girişi (RCIN) kanalında blokların **%94,5'i** sıfır bloktu
ve çıktının üçte ikisi etiketten ibaretti. Sprintz-Delta aynı veride 74x alıyordu —
yani ElBâri'nin *teorik tavanının üstünde*.

Sürüm 3'te ardışık sıfır blokları tek bir kaçışla kodlanıyor. **RCIN 40x → 92x.**

Kaçış kodunun seçimi özellikle önemliydi: `mod 0 + aykırı_var 1` ardından maske `0x00`,
sürüm 2 kodlayıcısının **üretemeyeceği** bir birleşimdir (aykırı bayrağı ancak maske
sıfır değilken kurulur). Bu sayede yeni bir `mod` değeri harcanmadı ve sürüm 2 akışları
sürüm 3 çözücüsünde aynen çalışmaya devam etti.

---

## 2. Kanal katmanı — kayıt akışını ayır

**Problem.** Gerçek telemetri tek bir sayı dizisi değildir:

```
[enlem, boylam, irtifa,  enlem, boylam, irtifa,  enlem, boylam, irtifa, ...]
```

Bu akış olduğu gibi çekirdeğe verilirse ardışık farklar **kanallar arasında zıplar**.
`enlem → boylam` farkı milyonlarca birim olur, aykırı oranı %100'e çıkar ve veri
*"sıkıştırılamaz"* diye reddedilir.

**Çözüm.** Akış önce kanallara ayrılır (deinterleave); her kanal kendi içinde
sıkıştırılır.

> **Ölçülen etki:** kanal ayrımı olmadan **reddediliyor** → kanal ayrımıyla **5.05x**.
> Yani birincil hedef veri tipi ancak bu katmanla çalışıyor.

**Bu ElBâri'ye özgü bir zayıflık mı?** Hayır. Kanal ayrımı olmadan tamsayı kodek
ailesinin tamamı çöküyor: BP128 1.00x, Sprintz 0.97x, Simple8b **0.50x** — Simple8b
veriyi ikiye katlıyor. Kanal katmanı bir savunma değil, bu problem sınıfının zorunlu
ön koşuludur.

### Kanal başına adaptif fark derecesi

Sabit hızla ilerleyen bir kanalda (düzgün uçuşta GPS enlemi) ardışık farklar neredeyse
sabittir. Böyle bir kanalı önce kendi fark akışına çevirmek, çekirdeğin içeride bir kez
daha fark alması sayesinde **ikinci derece fark** üretir ve değerler çok küçülür.

Gürültülü kanallarda ise ikinci derece fark varyansı **büyütür**. Bu yüzden karar kanal
başına verilir ve bir bayrakla bildirilir.

### Ham geçiş güvenliği

Bir kanal sıkışmıyorsa (gürültü, rastgele veri) **ham yazılır** ve bayrağı kurulur.
Sonuç: kanal katmanı hiçbir koşulda veriyi büyütmez ve "reddedildi" durumunda veri
kaybı olmaz.

### Referans bloğu *(biçim sürümü 4)*

Her kanalın akışı 4 baytlık bir mutlak referansla başlıyordu. Küçük bir çerçevede bu
toplam boyutun neredeyse yarısıydı — 8 kanallı 25 kayıtlık bir RCIN çerçevesinde
68 baytın 32'si.

Oysa bu K değer **akışın ilk kaydıdır** ve kanalların ilk değerleri genellikle birbirine
yakındır (8 RC kanalının hepsi ~1500). Sürüm 4'te hepsi tek blokta birlikte kodlanıyor.

Korelasyon yoksa (enlem/boylam/zaman gibi) blok kazanmaz ve ham hâle düşülür; bedeli
çerçeve başına 1 baytlık bayraktır. Adaptifliğin fiyatı budur.

---

## 3. Çerçeve katmanı — paket kaybına dayan

**Problem.** Delta kodlamanın ölümcül zayıflığı **zincirleme bağımlılıktır**: her değer
bir öncekine dayanır. Tek bir paket düşerse ondan sonraki **tüm** veri çözülemez.

İHA telemetrisi kayıplı bir RF linkinden gider; paket düşmesi normaldir.

> **Ölçülen taban çizgi:** çerçevesiz akış %1 paket kaybında yalnızca **%12** hayatta
> kalıyor. Nominal 15,58x'lik oran, etkin olarak **1,9x**'e iniyor.

**Çözüm.** Akış, her biri **kendi mutlak referansını taşıyan**, sıra numaralı ve CRC32
korumalı bağımsız çerçevelere bölünür. Hata yayılımı tek çerçeveyle sınırlıdır.

```
[başlık 10 bayt][kanal katmanı yükü]

başlık = [sihirli sayı][sürüm][CRC32][sıra no u16][kayıt sayısı u16]
```

### Bedeli — ve nerede optimum

Bağımsızlık bedavaya gelmez. Her çerçeve kendi referansını ve başlığını taşır, ve
sıkıştırma daha kısa bir pencere üzerinde çalışır.

| Çerçeve boyutu | Oran (yönelim verisi) |
| ---: | ---: |
| çerçevesiz | 15,58x |
| 1000 kayıt | 18,27x* |
| 100 kayıt | 13,24x |
| 25 kayıt | 6,98x |

\* *Çerçeveli değerler sürüm 4 ile çerçevesizi geçebiliyor; çünkü küçük pencerelerde
blok başına genişlik seçimi daha iyi uyum sağlıyor.*

**Peki çerçeve kaç kayıt olmalı?** Bu soru üç kısıtla (oran, gecikme, paket boyutu)
ölçülüyordu; dördüncüsü — kayıp altında ne kadarının kurtarıldığı — ayrı bir raporda
süpürüldü: **[KAYIP_DAYANIKLILIK.md](KAYIP_DAYANIKLILIK.md)**.

Kısa cevap: optimum **kayıt sayısıyla değil, çerçevenin kaç pakete bölündüğüyle**
belirlenir. Yüksek kayıpta hedef ~1 pakettir; düşük kayıpta gecikme bütçesinin izin
verdiği kadar büyük.

---

## 4. Float katmanları — ondalıklı telemetri

Çekirdek tamsayı üzerinde çalışır. Yönelim açıları, hız, ivme ve batarya gerilimi ise
ondalıklıdır. İki yol var ve **birini seçmek zorundasınız**:

### 4a. Kuantalama *(kayıplı — çoğu telemetride doğru seçim)*

Değerler istenen **hassasiyete** göre ölçeklenip tamsayıya çevrilir, sonra normal boru
hattına verilir. **Biçim değişmez.**

Bir yönelim açısını 0,001 radyan (0,06°) hassasiyetle taşımak fazlasıyla yeterlidir;
tam float taşımak bant genişliği israfıdır.

| Yöntem | Boyut | Oran |
| --- | ---: | ---: |
| Ham float32 | 288.000 B | — |
| **Kuantalama + kanal katmanı** | **27.403 B** | **10.51x** |
| Float bit desenini doğrudan vermek | 195.039 B | 1.48x |

*(12.000 kayıt × 6 kanal: roll, pitch, yaw, hız, batarya, irtifa)*

Kuantalama, bit desenini doğrudan sıkıştırmaktan **6,9 kat** iyi. Sebebi basit: float
bit desenlerinin ardışık farkları büyük ve düzensizdir; kuantalanmış tamsayılar düzgün
delta üretir.

**Ölçülen hata**, her kanalda hassasiyetin tam yarısını aşmıyor — kuantalamanın
matematiksel olarak ulaşabileceği en iyi sonuç.

> ⚠️ Tam değerin korunması gereken veriler (ham sensör kaydı, adli inceleme, uçuş
> sonrası tam saklama) bu katmandan **geçirilmemelidir**.

> **Ölçekler biçim içinde taşınmaz.** Gönderici ve alıcı aynı ölçek dizisini kullanmak
> zorundadır — telemetri şemasının parçası olarak, bant dışı. MAVLink de böyle çalışır.

### 4b. XOR *(kayıpsız — "mecbur kalınca")*

Ardışık float'ların bit desenleri XOR'lanır; yakın değerlerde işaret, üstel kısım ve
mantisin üst bitleri aynı olduğu için sonucun başında ve sonunda çok sayıda sıfır bulunur
ve yalnızca ortadaki anlamlı bitler yazılır. Literatürde Gorilla (Facebook, 2015) olarak
bilinir.

| Veri tipi | Kayıpsız (XOR) | Kayıplı (kuantalama) |
| --- | ---: | ---: |
| Gürültülü uçuş verisi | **1.21x** | **10.51x** |
| Durağan veri (çok tekrar eden) | **15.08x** | 10.86x |
| Düzgün sinyal (gürültüsüz) | **1.00x** | 15.83x |

Bu tablo dürüst bir beklenti yönetimi sunar:

- **Gürültülü sensör verisinde XOR neredeyse hiç kazandırmaz.** Gürültü mantisin alt
  bitlerini her örneklemde değiştirir; o bitler tanımı gereği sıkıştırılamaz.
- **Düzgün sinyalde 1.00x** — hiç sıkışmaz, ham geçişe düşer.
- **XOR yalnızca değerler AYNEN tekrar ettiğinde parlar.** Gorilla'nın tasarlandığı
  senaryo tam olarak budur: izleme verisinde değerler çoğu zaman hiç değişmez.

> **Kural:** Tam değer gerekmiyorsa **kuantalama kullanın.**

---

## 5. Algoritma ailesi — ne icat edildi, ne edilmedi

ElBâri **yeni bir algoritma iddiasında değildir.** Kullanılan teknikler:

1. **Delta encoding** — ardışık değerler arasındaki fark
2. **Frame of Reference (FOR)** — blok başına adaptif bit genişliği
3. **PFOR / patching** — büyük sapmaları ayrı 32-bit liste ile işleme
4. **SIMD hızlandırma** — AVX2 / NEON / skaler

Bu kombinasyon literatürde **PFOR-Delta** olarak bilinir (Zukowski ve ark., 2006;
Lemire & Boytsov'un SIMD çalışmaları).

**Katkı algoritma değil, paketlemedir:** bağımlılıksız, tahsisatsız, iki bağımsız
implementasyonda bit-bit aynı, ve **kayıplı linkte çalışabilen** bir telemetri kodeki.

Bu katkı ölçüldü ve sınırları biliniyor:
**[KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md)**.

---

## 6. Biçim sürümleri

Her sürüm artışında eski çözücü yeni çerçeveyi **sessizce yanlış çözmek yerine
reddeder** — çerçeve başlığındaki sürüm baytı bunu garanti eder.

| Sürüm | Ne değişti | Ölçülen etki |
| ---: | --- | --- |
| 1 | İlk biçim; 4 bit genişliği (2/4/8/16) | — |
| 2 | Bit genişliği tablosu 4 → 8 mod; sıfır blok | GPS +%25,6 |
| 3 | Blok üstü sıfır koşusu | RCIN 40x → 92x |
| 4 | Çerçeve sabit maliyeti: uzunluk tablosu kaldırıldı, başlık 16 → 10 bayt, referans bloğu | 8 kanalda 84 → ~20 bayt/çerçeve; RCIN 25 kayıt/çerçevede +%94 |

Ayrıntı: [BICIM_SPESIFIKASYONU.md](BICIM_SPESIFIKASYONU.md)
