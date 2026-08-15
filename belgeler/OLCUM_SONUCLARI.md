# ElBâri — Kapsamlı Ölçüm Sonuçları

**Biçim sürümü:** 2
**Ortam:** Windows 11, x64, 24 çekirdek · .NET 10 (AVX2) · MSVC C17 `/O2`
**Yöntem:** 15 tur ısınma, 120 tur ölçüm, tek iş parçacığı

> Bu belgedeki her sayı **ölçülmüştür**. Tahmin, hedef ya da literatürden alınan
> değer içermez. Ölçümü kendiniz tekrarlamak için son bölüme bakınız.

---

## 1. Veri setleri

| Veri seti | K | Ham | Kaynak |
| --- | ---: | ---: | --- |
| **Gerçek GPS** | 3 | 295.704 B | OpenStreetMap halka açık GPS izleri, 24.642 gerçek kayıt |
| **İHA telemetri** | 6 | 288.000 B | Gerçekçi uçuş (sabit hız + gürültü), tamsayı |
| **Float kuantalanmış** | 6 | 288.000 B | Yönelim/hız/batarya, 0.001 ve 0.01 hassasiyet |
| Sıralı sayaç | 1 | 240.000 B | En iyi durum (sentetik) |
| Sabit değer | 1 | 240.000 B | Tüm farklar sıfır (sentetik) |
| Sinüs sensör | 1 | 240.000 B | Periyodik sinyal (sentetik) |
| Rastgele | 1 | 240.000 B | Zorlu senaryo (sentetik) |

> Gerçek karar için **ilk üç satıra** bakınız. Sentetik setler yalnızca uç davranışı
> göstermek içindir; gerçek dünyayı temsil etmezler.

---

## 2. Sıkıştırma oranı — katmana göre

**C# ve C birebir aynı çıktıyı ürettiği için oranlar özdeştir.**

| Veri seti | Çekirdek | Kanal | Çerçeve (100) |
| --- | ---: | ---: | ---: |
| **Gerçek GPS** | ⊘ RED | **4.95x** | 4.30x |
| **İHA telemetri** | ⊘ RED | **7.13x** | 5.82x |
| **Float kuantalanmış** | 1.94x | **10.51x** | 7.94x |
| Sıralı sayaç | 12.80x | 63.73x | 10.26x |
| Sabit değer | 63.93x | 63.80x | 11.43x |
| Sinüs sensör | 6.72x | 13.24x | 6.46x |
| Rastgele | 1.94x | 1.94x | 1.72x |

**Okunması gerekenler:**

- **Çok kanallı veride çekirdek katmanı tek başına REDDEDİYOR.** Kanallar iç içe
  olduğu için ardışık farklar zıplıyor ve veri "sıkıştırılamaz" görünüyor. Kanal
  katmanı bu yüzden zorunlu, süs değil.
- **Çerçeveleme oranı düşürüyor** (4.95x → 4.30x, yaklaşık %13). Bu, paket kaybı
  dayanıklılığının bedeli. Kayıpsız bir taşıma varsa çerçeve katmanı kullanılmamalı.
- **Rastgele veride 1.94x** — sıkıştırılamayan veride bile bit paketleme sayesinde
  bir miktar kazanç var, ve veri **kaybolmuyor** (ham geçiş).

---

## 3. Hız — C# ve C yan yana

Kanal katmanı, MB/sn (ham veri üzerinden).

| Veri seti | C# encode | C encode | C# decode | C decode |
| --- | ---: | ---: | ---: | ---: |
| **Gerçek GPS** | 956 | **1.237** | 1.086 | **1.411** |
| **İHA telemetri** | **828** | 780 | 763 | **1.290** |
| **Float kuantalanmış** | 954 | **956** | 972 | **1.483** |
| Sıralı sayaç | 2.044 | **2.538** | **1.873** | 1.654 |
| Sabit değer | 2.653 | **2.749** | **2.728** | 2.117 |
| Sinüs sensör | 1.034 | **1.046** | 924 | **1.328** |
| Rastgele | **816** | 785 | 781 | **1.433** |

**Sezgiye aykırı sonuç: saf skaler C, SIMD'li C#'tan genelde hızlı.**

Özellikle **çözmede fark belirgin** — C, gerçek veride %30-80 daha hızlı. Sebebi:

1. C#'taki AVX2 yalnızca fark hesabını hızlandırıyor; işin asıl yükü olan **bit
   paketleme döngüsü** her iki sürümde de skaler.
2. C tarafında dizi sınır kontrolü yok.
3. Derleyici bazı döngüleri kendiliğinden vektörleştiriyor.

> Pratik sonuç: gömülü ve savunma hedefleri, aynı zamanda **daha hızlı** olan
> sürümü alıyor. Elle SIMD eklemek öncelik değil.

### Kayıt başına verim

| Veri seti | C# | C |
| --- | ---: | ---: |
| Gerçek GPS | 83,6 M kayıt/sn | **108,1 M kayıt/sn** |
| İHA telemetri | **36,2 M** | 34,1 M |
| Float kuantalanmış | 41,7 M | **41,8 M** |

---

## 4. Gecikme dağılımı — "deterministik" iddiasının sınavı

Gerçek zamanlı sistemde ortalama gecikme anlamsızdır; önemli olan **en kötü ihtimal**.
Çerçeve başına (100 kayıt × 3 kanal), 246 çerçeve × 200 tekrar, C sürümü:

| İşlem | en küçük | medyan | p95 | p99 | p99.9 | en büyük |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| encode | 0,90 µs | **1,60 µs** | 1,90 µs | 2,30 µs | 3,50 µs | 20,20 µs |
| decode | 0,70 µs | **1,30 µs** | 1,50 µs | 1,80 µs | 3,10 µs | 28,50 µs |

**Oynama oranı (p99 / medyan): encode 1,44x — decode 1,38x.** Dar bir bant.

**Veriye bağlı (algoritmik) değişkenlik: 2,55x.** Her çerçeve için 200 tekrarın
ortalaması alınarak işletim sistemi gürültüsü bastırıldığında kalan fark budur.

> **Dürüstlük notu:** En büyük değerler (20-28 µs) büyük ölçüde **işletim sistemi
> gürültüsüdür** — zamanlayıcı kesintileri, sayfa hataları, frekans ölçekleme.
> Algoritmanın kendisi değildir. Gerçek en-kötü-durum (WCET) analizi ancak bir RTOS
> üzerinde ve statik analizle yapılabilir; **bu henüz yapılmamıştır.**

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

---

## 6. Paket kaybı dayanıklılığı

Gerçek GPS verisi, 100 kayıt/çerçeve, 247 çerçeve, rastgele paket kaybı:

| Paket kaybı | Çerçeveli (kurtarılan) | Çerçevesiz |
| ---: | ---: | ---: |
| %1 | **24.442 kayıt (%99,2)** | 0 (%0) |
| %5 | **23.042 kayıt (%93,5)** | 0 (%0) |
| %10 | **21.842 kayıt (%88,6)** | 0 (%0) |
| %25 | **17.642 kayıt (%71,6)** | 0 (%0) |
| %50 | **11.242 kayıt (%45,6)** | 0 (%0) |

Sağdaki sütun meselenin özü: klasik fark kodlamada **tek bir paket düşerse her şey
gider**. Çerçeveli yaklaşımda kayıp lineerdir.

---

## 7. Float: kayıplı mı, kayıpsız mı?

Aynı veri (12.000 kayıt × 6 kanal, gerçekçi uçuş):

| Yöntem | Boyut | Oran | Doğruluk |
| --- | ---: | ---: | --- |
| Ham float32 | 288.000 B | — | — |
| **Kuantalama (kayıplı)** | **27.403 B** | **10.51x** | Hata ≤ yarım adım |
| Kayıpsız XOR | 237.335 B | 1.21x | Bit bit aynı |

**Kuantalama 8,7 kat daha küçük.**

Gürültülü sensör verisinde kayıpsız XOR neredeyse hiç kazandırmıyor: gürültü mantisin
alt bitlerini her örneklemde değiştiriyor ve bu bitler tanımı gereği sıkıştırılamaz.

> XOR yalnızca değerler **aynen tekrar ettiğinde** parlar (durağan veride 15,08x
> ölçüldü). Tam değer gerekmiyorsa kuantalama kullanın.

---

## 8. Rakiplerle karşılaştırma

Aynı makine, aynı gerçek GPS verisi, aynı tur sayısı.

| Yöntem | Boyut | Oran | Encode | Decode |
| --- | ---: | ---: | ---: | ---: |
| **ElBâri — kanal ayrımı** | **59.695 B** | **4.95x** | **863 MB/sn** | 846 MB/sn |
| **ElBâri — çerçeveli** | 68.844 B | 4.30x | 224 MB/sn | 245 MB/sn |
| Zstd (seviye 1) | 184.181 B | 1.61x | 214 MB/sn | 303 MB/sn |
| Zstd (seviye 19) | 97.481 B | 3.03x | 8 MB/sn | 481 MB/sn |
| LZ4 (hızlı) | 228.684 B | 1.29x | 909 MB/sn | 3.110 MB/sn |
| Brotli (q11) | 82.471 B | 3.59x | 1 MB/sn | 286 MB/sn |
| Deflate | 167.385 B | 1.77x | 62 MB/sn | 538 MB/sn |

ElBâri bu veri setinde **hem en yüksek orana** hem (LZ4 dışında) **en yüksek hıza**
sahip. LZ4 açmada daha hızlı ama oranı 1.29x — dört kat geride.

Ayrıca tabloda **görünmeyen** farklar: paket kaybı dayanıklılığı, sıfır tahsisat,
deterministik gecikme, bağımlılıksızlık. Rakiplerin hiçbirinde yok.

---

## 9. Teorik alt sınır — ne kadar yer kaldı?

"Daha fazla sıkıştırabilir miyiz?" sorusu ölçüldü.

### Gerçek GPS

| Sınır | Oran | Bize göre |
| --- | ---: | --- |
| **Bizim çıktımız** | **4.95x** | — |
| Bit paketleme tabanı (etiket bedava) | 5.56x | %11 küçük |
| Shannon entropisi (model bedava) | 6.30x | %21 küçük |
| **Entropi + frekans tablosu** | **4.54x** | **%8 BÜYÜK** |

### İHA telemetrisi

| Sınır | Oran | Bize göre |
| --- | ---: | --- |
| **Bizim çıktımız** | **10.51x** | — |
| Bit paketleme tabanı | 12.06x | %13 küçük |
| Entropi + frekans tablosu | 14.18x | %25 küçük |

**Sonuç: entropi kodlaması gerçek GPS verisinde işe yaramaz.** Kanallarda 1715-2063
farklı sembol var; frekans tablosunun kendisi entropinin kazandırdığından fazlasını
götürüyor.

Tablo maliyetinden kaçmanın yolu uyarlanabilir model kullanmaktır — ama o da çözücünün
**önceki tüm veriyi görmüş olmasını** gerektirir, yani **bağımsız çerçeveleri yok eder.**
Entropi kodlaması ile paket kaybı dayanıklılığı temelde uyuşmaz.

---

## 10. Doğrulama durumu

| Kontrol | Sonuç |
| --- | --- |
| C ↔ .NET ikili uyumluluk | ✅ 59.695 bayt birebir aynı |
| Uygunluk vektörleri | ✅ 27 vektör, 54 kontrol, 0 hata |
| C# test paketi | ✅ 32 senaryo, 0 hata |
| Fuzz (düşmanca girdi) | ✅ 0 tampon taşması, bozuk çerçeve reddi %100 |
| MSVC `/W4` + `/Wall /analyze` | ✅ 0 uyarı, 0 bulgu |
| GCC / Clang (Linux, CI) | ✅ temiz |
| ASan + UBSan (CI) | ✅ temiz |
| Tahsisat | ✅ 0 bayt |

### Bilinen eksikler

| Konu | Durum |
| --- | --- |
| Gerçek ARM donanımında ölçüm | ⏳ Yapılmadı |
| RTOS üzerinde WCET analizi | ⏳ Yapılmadı |
| Sertifikalı MISRA aracı doğrulaması | ⏳ Yapılmadı |
| Elle yazılmış SIMD (C) | ⏳ Yok — öncelik değil |

---

## 11. Ölçümü tekrarlama

```bash
# C# tarafı (veri setlerini de üretir)
dotnet run -c Release            # test paketi, 32 senaryo

# C tarafı
cd c
derle.bat                        # Windows
make                             # Linux/macOS

kapsamli.exe <veri_dizini>       # oran + hız tablosu
olcum.exe <referans_dizini>      # verim + gecikme dağılımı
uygunluk.exe ../testverisi/vektorler.txt
fuzz.exe 300000
```

---

**© 2025 İmran Kağan. Tüm hakları saklıdır.**
