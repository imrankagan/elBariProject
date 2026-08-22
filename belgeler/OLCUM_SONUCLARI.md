# Ölçüm Sonuçları — Birincil Sayı Kaynağı

**Biçim sürümü:** 4
**Ortam:** Windows 11, x64, 24 çekirdek · .NET 10 (AVX2) · MSVC C17 `/O2`

> **Bu belgenin işi nedir?** Projedeki sayıların **tek kaynağı** olmak. Anlatı
> belgeleri (mimari, kıyas, test) buradan alıntı yapar; aynı sayı iki yerde
> tutulmaz.
>
> Her tablonun altında **hangi komutla üretildiği** yazılıdır. Üretilemeyen bir
> tablo varsa bu da açıkça belirtilir.

| Ne arıyorsanız | Nereye |
| --- | --- |
| Katmanların neden var olduğu | [MIMARI.md](MIMARI.md) |
| Kendi ailesiyle kıyas | [KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md) |
| Zstd / LZ4 / Brotli | [KIYAS_GENEL_AMACLI.md](KIYAS_GENEL_AMACLI.md) |
| Paket kaybı süpürmesi | [KAYIP_DAYANIKLILIK.md](KAYIP_DAYANIKLILIK.md) |
| Testlerin envanteri | [TEST_VE_DOGRULAMA.md](TEST_VE_DOGRULAMA.md) |
| MISRA, CI, gömülü | [C_SURUMU.md](C_SURUMU.md) |

---

## 1. Veri setleri

| Veri seti | K | Ham | Kaynak | Nerede |
| --- | ---: | ---: | --- | --- |
| **Gerçek GPS** | 3 | 295.704 B | OpenStreetMap halka açık GPS izleri, 24.642 kayıt | `testverisi/gercek_gps.bin` |
| **Yönelim (ATT)** | 3 | 820.188 B | ALFA uçuş logu, 68.349 kayıt | `c/veri/` ile üretilir |
| **IMU** | 6 | 3.280.752 B | ALFA uçuş logu, 136.698 kayıt | `c/veri/` ile üretilir |
| **Titreşim (VIBE)** | 3 | 820.056 B | ALFA uçuş logu | `c/veri/` ile üretilir |
| **Servo (RCOU)** | 8 | 2.186.848 B | ALFA uçuş logu | `c/veri/` ile üretilir |
| **Kumanda (RCIN)** | 8 | 2.186.848 B | ALFA uçuş logu | `c/veri/` ile üretilir |
| **GPS (ALFA)** | 3 | 164.064 B | ALFA uçuş logu | `c/veri/` ile üretilir |
| İHA telemetri (sentetik) | 6 | 39.984 B | .NET takımı içinde üretilir | `benchmark/` |

> **ALFA fikstürleri depoda değildir** — veri seti açılınca 12,5 GB'dır ve kendi lisansı
> vardır. `c/veri/donustur.exe` bir ArduPilot `.bin` logundan üretir; ayrıntı
> [`c/veri/BENIOKU.md`](../c/veri/BENIOKU.md).
>
> **Platform sabit kanattır** (Carbon Z T-28). Yönelimi bir çoklu rotordan belirgin
> biçimde daha düzgündür; ATT/IMU oranları çoklu rotor telemetrisine göre **iyimser**
> taraftadır.

---

## 2. Sıkıştırma oranı — katmana göre

**C# ve C birebir aynı çıktıyı ürettiği için oranlar özdeştir.**

| Veri seti | Çekirdek (tek akış) | Kanal | Çerçeve (100 kayıt) |
| --- | ---: | ---: | ---: |
| **Gerçek GPS** | ⊘ RED | **5.05x** | 4.63x |
| **İHA telemetri (6 kanal)** | ⊘ RED | **7.12x** | 6.78x |

```bash
dotnet run --configuration Debug     # senaryo 26-32
```

**Okunması gerekenler:**

- **Çok kanallı veride çekirdek tek başına REDDEDİYOR.** Kanallar iç içe olduğu için
  ardışık farklar zıplıyor. Kanal katmanı bu yüzden zorunlu, süs değil.
- **Çerçeveleme oranı düşürüyor** (5.05x → 4.63x, ≈%8). Bu, paket kaybı
  dayanıklılığının bedeli. Kayıpsız bir taşıma varsa çerçeve katmanı kullanılmamalı.
- Biçim sürümü 4 öncesinde bu bedel %14'tü; çerçeve başına sabit yükün düşmesi farkı
  kapattı.

### Gerçek uçuş fikstürleri (kanal katmanı, çerçevesiz)

| Veri seti | K | Bayt | Oran |
| --- | ---: | ---: | ---: |
| Kumanda (RCIN) | 8 | 23.657 | **92.44x** |
| Servo (RCOU) | 8 | 58.259 | **37.54x** |
| Yönelim (ATT) | 3 | 52.646 | **15.58x** |
| IMU | 6 | 476.928 | 6.88x |
| GPS (ALFA) | 3 | 29.306 | 5.60x |
| Titreşim (VIBE) | 3 | 152.546 | 5.38x |
| GPS (OSM) | 3 | 58.513 | 5.05x |

```bash
c\kiyas\kiyas.exe <fikstür.bin> 20     # SENARYO 1 satırı
```

Rakiplerle karşılaştırma: [KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md)

---

## 3. Hız

### C ile C# yan yana (kanal katmanı, MB/sn)

| Veri seti | C# encode | C encode | C# decode | C decode |
| --- | ---: | ---: | ---: | ---: |
| **Gerçek GPS** | 956 | **1.237** | 1.086 | **1.411** |
| **İHA telemetri** | **828** | 780 | 763 | **1.290** |
| **Float kuantalanmış** | 954 | **956** | 972 | **1.483** |
| Sıralı sayaç | 2.044 | **2.538** | **1.873** | 1.654 |
| Sabit değer | 2.653 | **2.749** | **2.728** | 2.117 |
| Sinüs sensör | 1.034 | **1.046** | 924 | **1.328** |
| Rastgele | **816** | 785 | 781 | **1.433** |

> ⚠️ **Bu tablo şu an tekrarlanamıyor.** `olcum.exe` bir referans dizini bekler
> (`girdi.bin` vb.) ve o dizini üreten kod depoda yoktur. Sayılar biçim sürümü 2
> döneminde alınmıştır; **hız sıralaması** geçerlidir (biçim değişikliği hızı
> belirgin biçimde etkilemedi) ama mutlak değerler tazelenmemiştir.
>
> Tekrarlanabilir hız ölçümü için `kiyas.exe` kullanın — o, depodaki fikstürlerle
> çalışır ve her kodeğin encode/decode hızını verir.

**Sezgiye aykırı sonuç: saf skaler C, SIMD'li C#'tan genelde hızlı.** Özellikle
çözmede fark belirgin. Gerekçesi ve "elle SIMD neden yazılmayacak" kararı:
[C_SURUMU.md §4](C_SURUMU.md)

### Tekrarlanabilir hız (kiyas takımı, gerçek GPS)

| Kodek | encode MB/sn | decode MB/sn |
| --- | ---: | ---: |
| **ElBâri (kanal)** | **1.176** | 1.673 |
| VByte | 2.892 | 1.747 |
| StreamVByte | 1.909 | 1.780 |
| Simple8b | 380 | 1.874 |
| Sprintz-Delta | 446 | 1.115 |
| OptPFD | 243 | 1.138 |
| BP128 | 570 | 1.210 |

```bash
c\kiyas\kiyas.exe testverisi\gercek_gps.bin 100
```

> Rakipler **yeniden yazılmış skaler** sürümlerdir; hız sütunu onlar için bir **alt
> sınırdır** ve buradan "ElBâri daha hızlı" iddiası **kurulmaz**.

---

## 4. Gecikme dağılımı

Çerçeve başına (100 kayıt × 3 kanal), 246 çerçeve × 200 tekrar, C sürümü:

| İşlem | en küçük | medyan | p95 | p99 | p99.9 | en büyük |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| encode | 0,90 µs | **1,60 µs** | 1,90 µs | 2,30 µs | 3,50 µs | 20,20 µs |
| decode | 0,70 µs | **1,30 µs** | 1,50 µs | 1,80 µs | 3,10 µs | 28,50 µs |

**Oynama oranı (p99 / medyan): encode 1,44x — decode 1,38x.** Dar bir bant.

**Veriye bağlı (algoritmik) değişkenlik: 2,55x.** Her çerçeve için 200 tekrarın
ortalaması alınarak işletim sistemi gürültüsü bastırıldığında kalan fark budur.

> **Dürüstlük notu:** En büyük değerler (20–28 µs) büyük ölçüde **işletim sistemi
> gürültüsüdür** — zamanlayıcı kesintileri, sayfa hataları, frekans ölçekleme.
> Algoritmanın kendisi değildir. Gerçek WCET analizi ancak bir RTOS üzerinde ve statik
> analizle yapılabilir; **yapılmadı** (§7).
>
> Bu tablo da §3 ile aynı referans dizinini kullanır ve **şu an tekrarlanamıyor.**

### İşlemci payı

Zorlayıcı bir telemetri hızı (saniyede 400 kayıt) için:

```
saniyede 4 çerçeve × 1,60 µs = 6,4 µs
bir saniye                    = 1.000.000 µs
────────────────────────────────────────────
işlemci kullanımı             ≈ %0,0006
```

Tıkanan yer işlemci değil, **telsizin bant genişliğidir.**

---

## 5. Tahsisat

```
100 encode+decode turu, gerçek GPS verisi:
  Tahsis edilen bayt: 0
```

**Sıfır heap tahsisatı doğrulandı** — GC duraklaması yok, gerçek-zaman uyumlu.

```bash
dotnet run --configuration Debug     # "Allocation" bölümü
```

---

## 6. Teorik alt sınır — ne kadar yer kaldı?

"Daha fazla sıkıştırabilir miyiz?" sorusu ölçüldü. Bu, başka hiçbir belgede olmayan
bir analizdir ve **entropi kodlaması eklenip eklenmemesi** kararını verir.

### Gerçek GPS

| Sınır | Oran | Bize göre |
| --- | ---: | --- |
| **Bizim çıktımız** | **5.05x** | — |
| Bit paketleme tabanı (etiket bedava) | 5.56x | %11 küçük |
| Shannon entropisi (model bedava) | 6.30x | %21 küçük |
| **Entropi + frekans tablosu** | **4.54x** | **%8 BÜYÜK** |

### İHA telemetrisi

| Sınır | Oran | Bize göre |
| --- | ---: | --- |
| **Bizim çıktımız** | **10.51x** | — |
| Bit paketleme tabanı | 12.06x | %13 küçük |
| Entropi + frekans tablosu | 14.18x | %25 küçük |

**Sonuç: entropi kodlaması gerçek GPS verisinde işe yaramaz.** Kanallarda 1.715–2.063
farklı sembol var; frekans tablosunun kendisi entropinin kazandırdığından fazlasını
götürüyor.

Tablo maliyetinden kaçmanın yolu **uyarlanabilir model** kullanmaktır — ama o da
çözücünün *önceki tüm veriyi görmüş olmasını* gerektirir, yani **bağımsız çerçeveleri
yok eder.**

> **Entropi kodlaması ile paket kaybı dayanıklılığı temelde uyuşmaz.** Bu, projenin
> en önemli mimari kısıtlarından biridir ve bir eksiklik değil, bilinçli bir seçimdir.

---

## 7. Bilinen eksikler ve gerekçeleri

Bu bölüm **bilinçli olarak** ayrı tutulmuştur. Bir alıcının *"neyi bilmiyorlar?"*
sorusunun cevabı gizlenmemelidir.

### ⏳ Gerçek ARM donanımında ölçüm

**Durum:** Yapılmadı — donanım mevcut değil.

Kod ARM'da **derlenir** (bağımlılıksız C99), ancak hız ve gecikme rakamları yalnızca
x64 üzerinde ölçülmüştür. ARM kartlar (Raspberry Pi, Jetson) tipik olarak daha
yavaştır; kabaca 5–10 kat varsayılabilir ama bu **ölçülmemiş bir tahmindir.**

**Ne gerekir:** Bir Raspberry Pi 4/5 ya da Jetson. `make` ile derleyip `kapsamli` ve
`olcum` çalıştırmak yeterli — kod değişikliği gerekmez.

**Önemi:** Orta. İşlemci payı x64'te %0,0006 çıktığı için 10 kat yavaşlama bile
%0,006 eder; tıkanma riski yoktur. Ölçüm, iddiayı doğrulamak içindir.

### ⏳ RTOS üzerinde en-kötü-durum (WCET) analizi

**Durum:** Yapılmadı — RTOS ortamı mevcut değil.

§4'teki gecikme yüzdelikleri genel amaçlı bir işletim sisteminde alınmıştır ve üst
değerler büyük ölçüde işletim sistemi gürültüsüdür. Gerçek WCET, kesintilerin denetim
altında olduğu bir RTOS'ta ve statik analiz araçlarıyla belirlenir.

**Kolaylaştıran tasarım:** Sabit blok yapısı, özyineleme olmaması, dinamik bellek
kullanılmaması ve tüm döngülerin sınırlı olması WCET analizini **mümkün** kılar. Bu
özellikler [MISRA_UYUM.md](MISRA_UYUM.md)'de ayrıca doğrulanmıştır.

### ⏳ `olcum.exe` referans veri seti depoda yok

**Durum:** §3 ve §4 tabloları tekrarlanamıyor.

`olcum.exe` bir dizinde `girdi.bin` ve `float_girdi.bin` bekler; bu dosyaları üreten
kod depoda değildir. Sayılar geçerli ama **tazelenemiyor.**

**Ne gerekir:** Küçük bir üretici (fikstür biçimi basit:
`[int32 kanal][int32 eleman][int32 × N]`) ya da bu tabloların `kiyas`/`kayip` gibi
depodaki fikstürlerle çalışan araçlara taşınması. İkincisi tercih edilmelidir —
"her tablo bir komutla üretilebilmeli" ilkesi bunu gerektirir.

### ❌ Elle yazılmış SIMD (C sürümü)

**Durum:** Yapılmayacak — **ölçüm gerekçesiyle.**

Bu bir eksiklik değil, ölçüme dayalı bir karardır:

1. **C zaten C#'tan hızlı.** C# elle AVX2 kullanıyor, C saf skaler; buna rağmen C
   çözmede %30–80 daha hızlı (§3).
2. **SIMD işin küçük bir kısmına dokunuyor.** Fark hesabı vektörleşebilir ama asıl yük
   olan **bit paketleme döngüsü** doğası gereği sıralıdır — her değer bir öncekinin
   bıraktığı bit konumundan devam eder.
3. **İşlemci zaten boşta.** Hedef senaryoda kullanım %0,0006; elle SIMD bunu %0,0004
   yapar.
4. **Maliyeti gerçek:** mimariye özel kod yolları (AVX2 / NEON / skaler), üç ayrı test
   yükü, MISRA denetiminin zorlaşması, taşınabilirliğin azalması.

> **Karar:** Tıkanan yer işlemci değil, telsizin bant genişliğidir. Optimizasyon çabası
> orana ve dayanıklılığa harcanmalıdır. Gerekçesi değişirse (çok daha yüksek veri hızı
> gerektiren bir kullanım çıkarsa) yeniden değerlendirilir.

### ⏳ Sertifikalı araçla MISRA doğrulaması

**Durum:** Elle inceleme yapıldı ve belgelendi ([MISRA_UYUM.md](MISRA_UYUM.md)); açık
kaynak Cppcheck CI'da temiz geçiyor. Nitelikli araç raporu yok. Ayrıntı:
[C_SURUMU.md §6](C_SURUMU.md)

---

## 8. Ölçümü tekrarlama

```bash
# .NET takımı — §2 ve §5
dotnet run --configuration Debug

# Kıyas takımı — §2 fikstür tablosu, §3 tekrarlanabilir hız
c\derle.bat
c\kiyas\derle.bat
c\kiyas\kiyas.exe testverisi\gercek_gps.bin 100

# Doğrulama — bkz. TEST_VE_DOGRULAMA.md
c\uygunluk.exe testverisi\vektorler.txt
c\deger_fuzz.exe 300000
c\fuzz.exe 300000

# Gerçek uçuş fikstürleri (ALFA logu gerekir)
c\veri\donustur.exe "<ArduPilot .bin>"
```

Linux/macOS için `cd c && make test`.

---

**© 2025-2026 İmran Kağan.** Akademik kullanım serbest → [LICENSE.txt](../LICENSE.txt)
