# ElBâri Biçim Spesifikasyonu

**Belge türü:** Arayüz Kontrol Dokümanı (ICD)
**Biçim sürümü:** 1
**Durum:** Dondurulmuş — bu sürümde geriye dönük uyumsuz değişiklik yapılmaz

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

**Etiket kodlaması:**

| `mod` | Bit genişliği `w` |
| ---: | ---: |
| 0 | 2 |
| 1 | 4 |
| 2 | 8 |
| 3 | 16 |

`aykırı_var` biti, blokta en az bir aykırı değer olup olmadığını gösterir.

**Aykırı maskesi:** Blok içindeki 8 pozisyon için birer bit. Bit *j* set ise, *j*.
fark aykırıdır ve adım 3'te atlanıp adım 4'te 32 bit olarak yazılır.

### 2.3 Bit genişliği seçimi

Blok içindeki **aykırı olmayan** farkların en büyük mutlak değeri `M` olsun:

| Koşul | `w` |
| --- | ---: |
| `M ≤ 1` | 2 |
| `M ≤ 7` | 4 |
| `M ≤ 127` | 8 |
| aksi hâlde | 16 |

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

### 3.2 Başlık

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

### 3.6 Kaydedilen yük boyutu

Başlıktaki yük boyutu alanı, **ön ek dahil** toplam bayt sayısıdır:

```
kayıtlı_boyut = (ikinci_derece ? 4 : 0) + (sıkıştırılmış veya ham yük boyutu)
```

---

## 4. Katman 3 — Çerçeve

Akışı bağımsız çözülebilir, sıra numaralı, CRC korumalı parçalara böler.

### 4.1 Başlık (16 bayt)

| Bayt aralığı | İçerik |
| --- | --- |
| `[0..1]` | sihirli sayı: `0xEB 0x71` |
| `[2]` | sürüm: `1` |
| `[3]` | ayrılmış: `0` olmalıdır |
| `[4..7]` | CRC32 (uint32, little-endian) |
| `[8..11]` | çerçeve sıra numarası (uint32, little-endian) |
| `[12..15]` | bu çerçevedeki **kayıt** sayısı (int32, little-endian) |
| `[16..]` | kanal katmanı yükü |

### 4.2 CRC kapsamı

CRC32, **`[8]` konumundan çerçeve sonuna kadar** olan aralık üzerinden hesaplanır —
yani sıra numarası, kayıt sayısı ve yük dahildir; sihirli sayı, sürüm, ayrılmış bayt
ve CRC alanının kendisi hariçtir.

**Algoritma:** CRC-32 (IEEE 802.3), polinom `0xEDB88320` (ters çevrilmiş),
başlangıç `0xFFFFFFFF`, sonuç `0xFFFFFFFF` ile XOR'lanır.

> `[3]` ayrılmış baytı CRC kapsamı dışında olduğundan, çözücü bu baytın `0` olduğunu
> **ayrıca doğrulamalıdır**; aksi hâlde o bayta düşen bir bit bozulması fark edilmez.

### 4.3 Doğrulama sırası

Çözücü bir çerçeveyi kabul etmeden önce sırasıyla kontrol etmelidir:

1. Uzunluk ≥ 16
2. Sihirli sayı `0xEB 0x71`
3. Sürüm `1`
4. Ayrılmış bayt `0`
5. CRC32 eşleşmesi
6. Kayıt sayısı makul aralıkta (çarpım taşması dahil)

Herhangi biri başarısızsa çerçeve **atılmalı**, kayıp olarak sayılmalıdır.

### 4.4 Çerçeveleme kuralları

- Çerçeveler **tam kayıt sınırında** bölünmelidir; eleman sayısı kanal sayısının tam
  katı olmalıdır.
- Sıra numarası her çerçevede birer artırılır; alıcı eksik numaralardan kaybı anlar.
- Çerçeveler **sırasız** gelebilir ve **bağımsız** çözülebilir.

---

## 5. Sınırlar

| Sınır | Değer | Gerekçe |
| --- | ---: | --- |
| En fazla kanal sayısı | 255 | Başlıkta 1 bayt |
| En fazla eleman sayısı (tek çağrı) | 200.000.000 | Boyut hesaplarında 32 bit taşmasını önler |
| Blok boyutu | 8 | Sabit |
| Aykırı eşiği | 32767 | Sabit |

---

## 6. Uygunluk doğrulaması

Bu biçime uygunluk, [`TestVectors/vektorler.txt`](TestVectors/vektorler.txt) dosyasındaki
**dondurulmuş referans vektörleriyle** doğrulanır. Dosya 18 vektör içerir ve her bit
genişliğini, aykırı değerleri, ikinci derece farkı, ham geçişi, kısmi blokları ve çerçeve
başlığını kapsar.

Bir implementasyon uyumlu sayılır ancak ve ancak:

1. Her vektörün girdisinden **birebir aynı bayt dizisini** üretiyorsa, **ve**
2. Her vektörün çıktısından **birebir aynı girdiyi** geri kurabiliyorsa.

### Mevcut doğrulama durumu

| Implementasyon | Durum |
| --- | --- |
| C# (.NET 10) | ✅ Referans implementasyon — vektörler bundan üretildi |
| C (C99/C17) | ✅ 18 vektör, 36 kontrol, 0 hata |

Ek olarak iki implementasyon, 24.642 kayıtlık gerçek GPS verisinde **83.124 bayt birebir
aynı** çıktı üretmekte ve birbirinin çıktısını çözebilmektedir.

Çalıştırmak için:

```bash
c\derle.bat
uygunluk.exe ..\TestVectors\vektorler.txt
```

---

## 7. Sürüm politikası

- Biçim sürümü çerçeve başlığında `[2]` baytında taşınır.
- Sürüm 1 **dondurulmuştur**: geriye dönük uyumsuz değişiklik yapılmaz.
- Uyumsuz bir değişiklik gerekirse sürüm numarası artırılır ve çözücü eski sürümü
  desteklemeye devam edebilir.
- Test vektörleri sürümle birlikte dondurulur; mevcut vektörler değiştirilmez, yalnızca
  yenisi eklenebilir.

---

**© 2025 İmran Kağan. Tüm hakları saklıdır.**
Proprietary and Confidential. Commercial License Required.
