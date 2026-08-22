# ElBâri Biçim Spesifikasyonu

**Belge türü:** Arayüz Kontrol Dokümanı (ICD)
**Biçim sürümü:** 4
**Durum:** Dondurulmuş — bu sürümde geriye dönük uyumsuz değişiklik yapılmaz

> **Sürüm 4'te ne değişti — çerçeve başına sabit maliyet.** Ölçüm, 8 kanallı 25
> kayıtlık bir çerçevede 68 baytın **84 baytının** (yani neredeyse tamamının) sabit
> yük olduğunu gösterdi. Üç kalem kaldırıldı:
>
> 1. **Kanal uzunluk tablosu** (kanal × 4 bayt) — çözücü kanalları ardışık okuyor,
>    çekirdek kendi tükettiği bayt sayısını bildiriyor (§3.2, §3.6).
> 2. **Çerçeve başlığı 16 → 10 bayt** — ikinci sihirli sayı ve ayrılmış bayt
>    kaldırıldı, sıra no ve kayıt sayısı 4 → 2 bayta indi (§4.1).
> 3. **Mutlak referanslar tek blokta** — kanal başına 4 baytlık referans, akışın
>    ilk kaydı olarak birlikte sıkıştırılıyor (§3.7).
>
> Toplam: 8 kanalda çerçeve başına ~84 → ~20 bayt.
>
> **Sürüm 3'te ne değişmişti:** Çekirdeğe **blok-üstü sıfır koşusu** eklendi (§2.2b).

---

## 1. Kapsam

Bu belge, ElBâri sıkıştırma motorunun **bayt düzeyindeki veri biçimini** tanımlar.
Amaç, bağımsız bir tarafın bu belgeye bakarak uyumlu bir kodlayıcı/çözücü yazabilmesidir.

Biçim üç katmandan oluşur. Her katman bir öncekinin çıktısını taşır:

```
┌─ Çerçeve ────────────────────────────────────┐
│ başlık(16) │ ┌─ Kanal ────────────────────┐  │
│            │ │ başlık │ ┌─ Çekirdek ───┐  │  │
│            │ │        │ │ bit akışı    │  │  │
│            │ │        │ └──────────────┘  │  │
│            │ └────────────────────────────┘  │
└──────────────────────────────────────────────┘
```

### Genel kurallar

- **Bayt düzeni:** Tüm çok baytlı alanlar **little-endian**'dır.
- **Tamsayı gösterimi:** İşaretli değerler **ikiye tümleyen** (two's complement).
- **Bit sıralaması:** Bit akışı içinde alanlar **düşük bitten yükseğe** doldurulur.
- **Taşma:** Fark hesaplarında taşma **sarar** (modulo 2³²). Uygulama bunu işaretsiz
  aritmetikle sağlamalıdır; C'de işaretli taşma tanımsız davranıştır.

---

## 2. Katman 1 — Çekirdek

Tek bir `int32` dizisini fark kodlama + uyarlanabilir bit paketleme ile sıkıştırır.

### 2.1 Genel yapı

```
[0..3]  : referans   — dizinin ilk elemanı, int32 little-endian
[4..]   : bit akışı  — blok blok kodlanmış farklar
```

Dizinin ilk elemanı **mutlak** olarak saklanır. Sonraki tüm elemanlar, bir öncekinden
farkı (delta) olarak kodlanır.

### 2.2 Blok yapısı

Farklar **8'erli bloklar** hâlinde işlenir. Son blok daha kısa olabilir.

Her blok için bit akışına sırasıyla şunlar yazılır:

| Sıra | Alan | Bit | Açıklama |
| --- | --- | ---: | --- |
| 1 | etiket | 4 | `(mod << 1) \| aykırı_var` |
| 2 | aykırı maskesi | 8 | yalnızca `aykırı_var = 1` ise |
| 3 | normal farklar | n × w | aykırı olmayan farklar, `w` bit genişliğiyle |
| 4 | aykırı farklar | m × 32 | aykırı farklar, tam genişlikte |

**Etiket kodlaması:** `mod` alanı 3 bittir, sekiz genişlik tanımlar.

| `mod` | Bit genişliği `w` | Kapsadığı `M` |
| ---: | ---: | ---: |
| 0 | — | M ≤ 0 → **sıfır blok**, veri biti yazılmaz |
| 1 | 2 | M ≤ 1 |
| 2 | 3 | M ≤ 3 |
| 3 | 4 | M ≤ 7 |
| 4 | 5 | M ≤ 15 |
| 5 | 8 | M ≤ 127 |
| 6 | 10 | M ≤ 511 |
| 7 | 16 | M ≤ 32767 |

`aykırı_var` biti, blokta en az bir aykırı değer olup olmadığını gösterir.

**Aykırı maskesi:** Blok içindeki 8 pozisyon için birer bit. Bit *j* set ise, *j*.
fark aykırıdır ve adım 3'te atlanıp adım 4'te 32 bit olarak yazılır.

> **Ulaşılamaz birleşim.** `aykırı_var = 1` iken aykırı maskesi **asla 0x00 olamaz**:
> kodlayıcı `aykırı_var` bitini ancak maske sıfır değilken kurar. Sürüm 3 bu boşluğu
> kaçış kodu olarak kullanır (§2.2b).

### 2.2b Blok-üstü sıfır koşusu *(sürüm 3)*

**Sorun.** Sıfır blok (`mod = 0`, aykırı yok) veri biti yazmaz ama **etiketini yine
yazar**. Bu, biçime değer başına 0,5 bitlik bir taban maliyeti ve 32 bitlik değerler
için **64x'lik sert bir tavan** koyar — veri ne kadar sabit olursa olsun.

Ölçüldü: gerçek bir ArduPilot kumanda girişi (RCIN) kanalında blokların **%94,5'i**
sıfır blok ve bunların %93'ü uzun koşular hâlinde. Yani çıktının üçte ikisi
etiketten ibaretti.

**Çözüm.** Ardışık **tam** sıfır blokları tek bir kaçışla kodlanır:

| Sıra | Alan | Bit | Değer |
| --- | --- | ---: | --- |
| 1 | etiket | 4 | `0x1` — yani `mod = 0`, `aykırı_var = 1` |
| 2 | aykırı maskesi | 8 | `0x00` — **kaçış işareti** |
| 3 | koşu uzunluğu | 16 | koşudaki tam sıfır blok sayısı `R`, `1 ≤ R ≤ 65535` |

Çözücü bu kaçışı gördüğünde `R × 8` adet değeri, bir öncekiyle aynı olacak şekilde
üretir (tüm farklar sıfırdır) ve bir sonraki bloğa geçer.

**Neden bu birleşim.** Yukarıda belirtildiği gibi `aykırı_var = 1` + maske `0x00`
sürüm 2 kodlayıcısının **üretemeyeceği** bir birleşimdir. Bu sayede:

- yeni bir `mod` değeri harcanmaz (dördü de dolu),
- **sürüm 2 akışları sürüm 3 çözücüsünde aynen çalışır** — çünkü sürüm 2 bu daldan
  hiç geçmez.

**Kullanım kuralı (kodlayıcı).** Kaçış 4 + 8 + 16 = **28 bit**, düz kodlama koşu
başına 4 bit tutar. Başabaş nokta 7 bloktur; bu yüzden kodlayıcı kaçışı yalnızca
**R ≥ 8** iken kullanır. Daha kısa koşular düz kodlanır. Bu eşik biçimin parçasıdır:
uyumlu bir kodlayıcı aynı eşiği kullanmalıdır, aksi hâlde bit bit aynı çıktı üretmez.

**Kısıtlar.**

- Koşuya yalnızca **tam** bloklar (8 eleman) girer; kısa son blok koşuya dâhil edilmez.
- `R = 0` geçersizdir; çözücü bunu bozuk girdi olarak reddeder.
- `R` 16 bite sığmazsa koşu birden çok kaçışa bölünür.

**Ölçülen etki** (gerçek ArduPilot uçuş logu, kanal katmanı):

| Veri | Sürüm 2 | **Sürüm 3** |
| --- | ---: | ---: |
| Kumanda girişi (RCIN, 8 kanal) | 40,09x | **92,44x** |
| Servo çıkışı (RCOU, 8 kanal) | 25,64x | **37,54x** |
| Yönelim (ATT, 3 kanal) | 14,70x | **15,58x** |
| GPS / IMU / titreşim | — | **değişmedi** (sıfır koşusu yok) |

### 2.3 Bit genişliği seçimi

Blok içindeki **aykırı olmayan** farkların en büyük mutlak değeri `M` olsun; tablodaki
`M`'yi kapsayan **en küçük** genişlik seçilir (yukarıdaki etiket tablosu).

**Sıfır blok (`mod = 0`):** Bu değer yalnızca `M ≤ 0` iken seçilir; yani aykırı olmayan
farkların **tamamı sıfırdır**. Bu durumda aykırı olmayan pozisyonlar için **hiçbir veri
biti yazılmaz** — çözücü hepsini sıfır kabul eder. Aykırı değerler her zamanki gibi 32 bit
olarak yazılmaya devam eder.

> Bu bir en iyileme değil, **bilgi taşımayan bitlerin kaldırılmasıdır.** Önceki tasarımda
> bu bloklar için değer başına garantili sıfır olan 1 bit yazılıyordu. Ölçülen kazanç:
> gerçek GPS %5.4 (zaman kanalında blokların %89'u sıfır blok), İHA telemetrisi %3.5.

> **Genişlik kümesi nasıl seçildi:** 16 zorunludur (aykırı eşiği 32767 tam olarak 16 bit
> gerektirir). Kalan 7 yuva için 1..15 arasındaki **tüm kombinasyonlar** gerçek veri
> üzerinde denendi ve iki farklı veri setinde (gerçek GPS izleri ve kuantalanmış İHA
> telemetrisi) birden en iyi sonucu veren küme alındı. Ölçülen kazanç: GPS %25.6,
> İHA %24.0.
>
> Bu seçim önemlidir: yalnızca tek veri setine göre en iyilenen bir küme
> (`[1,2,7,8,9,10,11,16]`) diğer veri setinde **%60 kötüleşme** üretiyordu.

### 2.4 Aykırı değer tanımı

Mutlak değeri **32767**'den büyük olan fark **aykırı** sayılır. Aykırı farklar bit
genişliği hesabına dahil edilmez; 32 bit olarak ayrıca yazılır.

### 2.5 İşaret genişletme

Dar alanda saklanan farklar okunurken işaret genişletilir: `w` bitlik değerin en üst
biti (bit `w-1`) set ise, üst bitler 1 ile doldurulur.

### 2.6 Son bayt

Bit akışı bittiğinde tamponda 8'den az bit kalmışsa, bu bitler son bir bayta yazılır.
Kalan üst bitler tanımsızdır ve çözücü tarafından yok sayılır.

### 2.7 Reddetme durumları

Kodlayıcı şu durumlarda veriyi **reddeder** (hata değil; çağıran ham göndermelidir):

- **Hızlı tarama:** Örneklem üzerinde aykırı oranı %30'u aşarsa, ortalama fark
  `INT32_MAX/4`'ü aşarsa, ya da en büyük fark `INT32_MAX/2`'yi aşarken aykırı oranı
  %10'u geçerse.
- **Erken iptal:** İlk ~64 eleman işlendikten sonra sıkıştırma oranı 1.5x'in altındaysa.

### 2.8 Yapısal tüketim kontrolü (çözücü)

Geçerli bir akış girdinin **tamamını** tüketir. Çözücü, tüm elemanları ürettikten sonra
girdide tüketilmemiş bayt kalmışsa akışı **reddetmelidir**.

> Bu, çözücüye verilen `girdi_boyutu` değerinin sıkıştırılmış verinin **tam boyutu**
> olmasını gerektirir. Biçim, veri uzunluğunu kendi içinde taşımaz.

---

## 3. Katman 2 — Kanal

Çok kanallı (iç içe geçmiş) kayıt akışını kanallara ayırıp her kanalı bağımsız sıkıştırır.

### 3.1 Giriş düzeni

```
[k0, k1, ..., kN-1,  k0, k1, ..., kN-1,  ...]
 └── 1. kayıt ──┘    └── 2. kayıt ──┘
```

`c` numaralı kanalın elemanları, giriş dizisinde `c, c+N, c+2N, ...` indekslerindedir.

### 3.2 Başlık *(sürüm 4)*

```
[0]                 kanal sayısı K
[1 .. 1+B)          ikinci-derece bayrakları  (kanal başına 1 bit)
[1+B .. 1+2B)       ham-geçiş bayrakları      (kanal başına 1 bit)
[1+2B]              referans bloğu bayrağı    (1 = sıkıştırıldı, 0 = ham)
[1+2B+1 .. )        referans bloğu (§3.7), sonra kanal yükleri
```

`B = ceil(K/8)` **türetilir, taşınmaz.**

**Kanal yük boyutları taşınmaz.** Sürüm 3'e kadar her kanal için 4 baytlık bir
sıkıştırılmış uzunluk yazılıyordu. Bu gereksizdir: kanal başına eleman sayısı
formülle hesaplanır (§3.3) ve çekirdek çözücü kaç bayt tükettiğini bilir (§2.8).
Uyumlu bir çözücü kanalları **ardışık** okur. Rastgele erişim kaybolur; MTU'ya
sığan bir çerçevede bunun karşılığı yoktur.

#### Eski başlık (sürüm ≤ 3, referans)

`K` = kanal sayısı (1..255), `B = ceil(K/8)` = bayrak bayt sayısı.

| Bayt aralığı | İçerik |
| --- | --- |
| `[0]` | kanal sayısı `K` |
| `[1]` | bayrak bayt sayısı `B` |
| `[2 .. 2+B)` | ikinci-derece bayrakları (kanal başına 1 bit) |
| `[2+B .. 2+2B)` | ham-geçiş bayrakları (kanal başına 1 bit) |
| `[2+2B .. 2+2B+4K)` | kanal başına yük boyutu (int32, little-endian) |
| sonrası | kanal yükleri, kanal sırasına göre ardışık |

Bayrak bitleri: kanal `c` için `bayrak[c >> 3]` baytının `(c & 7)`. biti.

### 3.3 Kanal uzunluğu

Toplam eleman sayısı `T` iken, kanal `c`'nin eleman sayısı:

```
uzunluk(c) = (c >= T) ? 0 : ceil((T - c) / K)
```

Bu, eleman sayısının kanal sayısının tam katı olmadığı durumları da kapsar.

### 3.4 İkinci derece fark

Bir kanalda ardışık farkların toplam mutlak büyüklüğü, farkların farkının toplam mutlak
büyüklüğünden **büyükse** ikinci derece seçilir (ilk 512 eleman örneklenir).

İkinci derece seçilen kanalda yük şöyle düzenlenir:

```
[0..3] : ilk değer (mutlak, int32 little-endian)
[4..]  : fark akışının çekirdek katmanıyla sıkıştırılmış hâli
```

Fark akışı `[x1-x0, x2-x1, ..., x(m-1)-x(m-2)]` olup uzunluğu `m-1`'dir.

> **Neden ilk değer ayrı:** Akış `[x0, d1, d2, ...]` biçiminde olsaydı, `x0` mutlak
> (örn. 1.7 milyar) ve `d1` minik olduğu için 0→1 geçişinde yapay ve devasa bir sıçrama
> oluşurdu. Bu sıçrama bir aykırı değer harcar ve hızlı tarama istatistiklerini bozarak
> veriyi gereksiz yere reddettirebilir.

### 3.5 Ham geçiş

Bir kanal sıkıştırılamazsa (çekirdek reddederse veya kazanç yoksa) yük **ham** yazılır:
her eleman int32 little-endian olarak ardışık. Kanalın ham-geçiş bayrağı set edilir.

Ham geçiş, ikinci derece ile birlikte kullanılabilir; bu durumda ham yazılan şey fark
akışıdır ve yükün başında yine mutlak ilk değer bulunur.

> Bu mekanizma **kayıpsızlığı her koşulda** garanti eder: "reddedildi" durumunda veri
> düşmez.

### 3.6 Kanal sınırlarının bulunması *(sürüm 4)*

Sürüm 3'e kadar başlıkta kanal başına bir **yük boyutu** alanı vardı. Sürüm 4'te bu
alan yoktur; kanal sınırları **hesaplanır**:

| Kanal türü | Tükettiği bayt |
| --- | --- |
| Ham geçiş | `(m − 1) × 4` — kanal uzunluğundan bilinir (§3.3) |
| Sıkıştırılmış | çekirdek çözücünün **bildirdiği** tüketim (§2.8) |

Uyumlu bir çözücü kanalları **ardışık** okur ve her kanaldan sonra konumu bu
miktar kadar ilerletir. Tüm kanallar bittiğinde girdinin **tamamı** tüketilmiş
olmalıdır; aksi hâlde akış bu kodlayıcıdan çıkmamıştır (§2.8'deki yapısal tüketim
kontrolünün kanal katmanı karşılığı).

---

### 3.7 Referans bloğu *(sürüm 4)*

Her kanalın akışı bir **mutlak referansla** başlar. Bunlar kanal başına 4 bayt tutar
ve küçük bir çerçevede toplam boyutun neredeyse yarısını yiyordu — 8 kanallı 25
kayıtlık bir RCIN çerçevesinde 68 baytın 32'si.

Oysa bu K değer **akışın ilk kaydıdır** ve kanalların ilk değerleri genellikle
birbirine yakındır (8 RC kanalının hepsi ~1500). Bu yüzden hepsi tek blokta
birlikte kodlanır:

| Bayrak | İçerik |
| --- | --- |
| `0` | ham `K × 4` bayt (little-endian int32) |
| `1` | çekirdek katmanıyla kodlanmış K değer — **kendini sınırlar**, uzunluk taşınmaz |

Kodlayıcı bloğu yalnızca `K ≥ 3` iken sıkıştırmayı dener ve **ancak ham hâlden
küçükse** kullanır; aksi hâlde bayrak `0` olur. Bayrak baytı, korelasyonun olmadığı
durumda ödenen bedeldir (çerçeve başına 1 bayt).

Her kanalın kendi akışı bu referansı **dışarıdan** alır:

- **Birinci derece:** akış yalnızca `[1..m-1]` farklarını taşır; `values[0]` bloktan gelir.
- **İkinci derece:** mutlak ilk değer bloktan gelir; fark akışı kendi referansını
  (`d1`) taşımaya devam eder.
- **Ham geçiş:** birinci derecede `values[1..m-1]`, ikinci derecede fark dizisinin
  tamamı yazılır.

## 4. Katman 3 — Çerçeve

Akışı bağımsız çözülebilir, sıra numaralı, CRC korumalı parçalara böler.

### 4.1 Başlık (10 bayt, sürüm 4)

```
[0]        sihirli sayı 0xEB
[1]        sürüm (4)
[2..5]     CRC32
[6..7]     sıra no      (uint16, little-endian — SARAR)
[8..9]     kayıt sayısı (uint16, little-endian)
```

- **Tek sihirli sayı.** Asıl doğrulamayı CRC yapar; sihirli sayı yalnızca ucuz bir
  ön elemedir.
- **Ayrılmış bayt kaldırıldı.**
- **Sıra no 16 bittir ve sarar.** Kayıp ve sıralama tespiti için yeterlidir; RTP de
  16 bit kullanır. Karşılaştırma yapan taraf sarmayı hesaba katmalıdır.
- **Kayıt sayısı 16 bittir.** Sınır aşılırsa kodlayıcı sessizce kırpmaz,
  `ELBARI_HATA_PARAMETRE` döner.

#### Eski başlık (sürüm ≤ 3, referans)

| Bayt aralığı | İçerik |
| --- | --- |
| `[0..1]` | sihirli sayı: `0xEB 0x71` |
| `[2]` | sürüm: `2` ya da `3` |
| `[3]` | ayrılmış: `0` olmalıdır |
| `[4..7]` | CRC32 (uint32, little-endian) |
| `[8..11]` | çerçeve sıra numarası (uint32, little-endian) |
| `[12..15]` | bu çerçevedeki **kayıt** sayısı (int32, little-endian) |
| `[16..]` | kanal katmanı yükü |

### 4.2 CRC kapsamı

CRC32, **`[6]` konumundan çerçeve sonuna kadar** olan aralık üzerinden hesaplanır —
yani sıra numarası, kayıt sayısı ve yük dahildir; sihirli sayı, sürüm ve CRC alanının
kendisi hariçtir.

**Algoritma:** CRC-32 (IEEE 802.3), polinom `0xEDB88320` (ters çevrilmiş),
başlangıç `0xFFFFFFFF`, sonuç `0xFFFFFFFF` ile XOR'lanır.

> Sihirli sayı ve sürüm CRC kapsamı dışındadır; çözücü onları **birebir
> karşılaştırmayla** doğrular (§4.3).

### 4.3 Doğrulama sırası

Çözücü bir çerçeveyi kabul etmeden önce sırasıyla kontrol etmelidir:

1. Uzunluk ≥ 10
2. Sihirli sayı `0xEB`
3. Sürüm `4` (eski sürüm çerçeveleri **reddedilir** — sessizce yanlış çözmektense
   reddetmek yeğlenir)
4. CRC32 eşleşmesi
5. Kayıt sayısı makul aralıkta (çarpım taşması dahil)

Herhangi biri başarısızsa çerçeve **atılmalı**, kayıp olarak sayılmalıdır.

### 4.4 Çerçeveleme kuralları

- Çerçeveler **tam kayıt sınırında** bölünmelidir; eleman sayısı kanal sayısının tam
  katı olmalıdır.
- Sıra numarası her çerçevede birer artırılır; alıcı eksik numaralardan kaybı anlar.
- Çerçeveler **sırasız** gelebilir ve **bağımsız** çözülebilir.

---

## 4b. Float katmanları

Float desteği iki ayrı yoldan sağlanır. **Hiçbiri yukarıdaki üç katmanın biçimini
değiştirmez.**

### 4b.1 Kuantalama (kayıplı) — biçim dışı ön/son işleme

Ondalıklı değer, ölçekle çarpılıp tamsayıya yuvarlanır ve mevcut boru hattına verilir.

```
tamsayi = yuvarla(deger * olcek)
```

**Yuvarlama:** hesap **çift duyarlıkta** yapılır ve yuvarlama **sıfırdan uzağa**
(round half away from zero) uygulanır:

```
olcekli = (double)deger * (double)olcek
olcekli = (olcekli >= 0) ? olcekli + 0.5 : olcekli - 0.5
tamsayi = (int32)olcekli
```

> Bankacı yuvarlaması (round half to even) **kullanılmaz**. .NET'in `Math.Round`
> varsayılanı bankacı yuvarlamasıdır; kullanılsaydı C sürümüyle sınır değerlerde
> ayrışırdı. Doğrulama vektörü: `float_kuantala_yarim` (0.5→1, 1.5→2, 2.5→3).

NaN, sonsuz ve int32 aralığını aşan değerler **hata döndürür**; sessizce yanlış değer
üretilmez.

**Ölçekler biçim içinde taşınmaz.** Gönderici ve alıcı aynı ölçek dizisini kullanmak
zorundadır (telemetri şemasının parçası, bant dışı).

### 4b.2 Kayıpsız XOR (Gorilla ailesi)

Ardışık float'ların bit desenleri XOR'lanır ve yalnızca anlamlı bitler yazılır.

**Bit akışı** (düşük bitten yükseğe):

```
İlk değer : 32 bit ham (bit deseni olduğu gibi)

Sonraki her değer:
  XOR == 0  ->  1 bit: 0
  XOR != 0  ->  1 bit: 1, ardından:
      önceki pencere yeterliyse (BS >= öncekiBS ve SS >= öncekiSS):
          1 bit: 0
          (32 - öncekiBS - öncekiSS) bit: anlamlı bitler
      aksi hâlde:
          1 bit: 1
          5 bit: BS  (baştaki sıfır sayısı, 0..31)
          5 bit: anlamlı_uzunluk - 1  (0..31 => 1..32)
          anlamlı_uzunluk bit: anlamlı bitler
```

`BS` = baştaki sıfır sayısı, `SS` = sondaki sıfır sayısı,
`anlamlı_uzunluk = 32 - BS - SS`.

Başlangıçta `öncekiBS = öncekiSS = 32` kabul edilir ("geçerli pencere yok"); bu durumda
ilk sıfır-olmayan XOR daima yeni pencere yolunu kullanır. Çözücü, geçerli pencere yokken
pencere tekrarı istenirse girdiyi **bozuk** saymalıdır.

**Çok kanallı sarmalayıcı biçimi:**

| Bayt aralığı | İçerik |
| --- | --- |
| `[0]` | kanal sayısı `K` |
| `[1]` | bayrak bayt sayısı `B = ceil(K/8)` |
| `[2 .. 2+B)` | ham-geçiş bayrakları (kanal başına 1 bit) |
| `[2+B .. 2+B+4K)` | kanal başına yük boyutu (int32, little-endian) |
| sonrası | kanal yükleri, sırayla |

Kanal katmanıyla aynı yapıdadır, yalnızca ikinci-derece bayrakları yoktur (XOR yolunda
böyle bir seçim bulunmaz).

## 5. Sınırlar

| Sınır | Değer | Gerekçe |
| --- | ---: | --- |
| En fazla kanal sayısı | 255 | Başlıkta 1 bayt |
| En fazla eleman sayısı (tek çağrı) | 200.000.000 | Boyut hesaplarında 32 bit taşmasını önler |
| Blok boyutu | 8 | Sabit |
| Aykırı eşiği | 32767 | Sabit |

---

## 6. Uygunluk doğrulaması

Bu biçime uygunluk, [`testverisi/vektorler.txt`](../testverisi/vektorler.txt) dosyasındaki
**dondurulmuş referans vektörleriyle** doğrulanır. Dosya **29 vektör** içerir:

| Katman | Vektör | Kapsam |
| --- | ---: | --- |
| `cekirdek` | 12 | her bit genişliği, aykırı değer, sabit, negatif, kısmi blok, çok blok, **işaret biti (2³¹ farkı)**, **blok üstü sıfır koşusu** |
| `kanal` | 5 | çok kanal, ikinci derece, ham geçiş, K=1, eksik kayıt |
| `cerceve` | 3 | temel, sıra numarası, tek kayıt |
| `float_kuantala` | 3 | kanal başına ölçek, negatif değerler, **tam yarım (yuvarlama yönü)** |
| `float_xor` | 5 | tekrar eden değer, küçük değişim, **özel değerler (NaN/-0/sonsuz)**, işaret değişimi, tek eleman |
| `float_xor_kanal` | 1 | çok kanallı kayıpsız + ham geçiş |

> **Float vektörlerinde girdi, ondalık metin yerine BİT DESENİ olarak yazılır** (`GIRDIF`
> alanı, 8 haneli hex). Ondalık gösterim ayrıştırıcı farklılıkları yüzünden belirsizdir ve
> NaN / negatif sıfır gibi değerleri güvenilir taşımaz.

Bir implementasyon uyumlu sayılır ancak ve ancak:

1. Her vektörün girdisinden **birebir aynı bayt dizisini** üretiyorsa, **ve**
2. Her vektörün çıktısından **birebir aynı girdiyi** geri kurabiliyorsa.

### Mevcut doğrulama durumu

| Implementasyon | Durum |
| --- | --- |
| C# (.NET 10) | ✅ Referans implementasyon — vektörler bundan üretildi |
| C (C99/C17) | ✅ **29 vektör, 58 kontrol, 0 hata** |

Ek olarak iki implementasyon, 24.642 kayıtlık gerçek GPS verisinde **58.513 bayt birebir
aynı** çıktı üretmekte ve birbirinin çıktısını çözebilmektedir.

Çalıştırmak için:

```bash
c\derle.bat
uygunluk.exe ..\TestVectors\vektorler.txt
```

---

## 7. Sürüm geçmişi

### Sürüm 2 (güncel)

**İki değişiklik:**

1. **Bit genişliği tablosu 4 girdiden 8'e çıkarıldı.**
2. **Sıfır blok:** `mod = 0` artık veri biti yazmaz (bkz. bölüm 2.2/2.3).

Sürüm 1'de etiketin `mod` alanı 3 bit olmasına rağmen yalnızca 4 değer kullanılıyordu
(2/4/8/16); kalan 4 yuva boştu. Bu, 5 bit gereken bir farkın 8 bitle, 10 bit gerekenin
16 bitle yazılması demekti.

**Etiket alanı büyümedi** — yalnızca zaten var olan bitler değerlendirildi.

| Ölçüm | Sürüm 1 | Sürüm 2 |
| --- | ---: | ---: |
| Gerçek GPS (24.642 kayıt) | 3.56x | **5.05x** |
| İHA telemetrisi (kuantalanmış) | 8.01x | **10.51x** |
| Test paketi ortalaması | 5.45x | **8.83x** |

**Uyumluluk:** Sürüm 1 ile ikili uyumlu **değildir**. Çerçeve başlığındaki `[2]` sürüm
baytı `2` olur; sürüm 1 çözücüsü böyle bir çerçeveyi reddeder.

> ⚠️ **Çekirdek ve kanal katmanlarında sürüm baytı yoktur.** Bu katmanlar doğrudan
> kullanılıyorsa sürüm bilgisi **bant dışı** bilinmelidir. Sürüm işaretlemesi yalnızca
> çerçeve katmanında vardır.

### Sürüm 1

İlk dondurulmuş biçim. Bit genişlikleri: 2/4/8/16.

## 8. Sürüm politikası

- Biçim sürümü çerçeve başlığında `[2]` baytında taşınır.
- Sürüm 1 **dondurulmuştur**: geriye dönük uyumsuz değişiklik yapılmaz.
- Uyumsuz bir değişiklik gerekirse sürüm numarası artırılır ve çözücü eski sürümü
  desteklemeye devam edebilir.
- Test vektörleri sürümle birlikte dondurulur; mevcut vektörler değiştirilmez, yalnızca
  yenisi eklenebilir.

---

**© 2025 İmran Kağan. Tüm hakları saklıdır.**
Proprietary and Confidential. Commercial License Required.
