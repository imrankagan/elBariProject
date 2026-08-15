# ElBâri — Telemetri Sıkıştırma Motoru

[![License: Proprietary](https://img.shields.io/badge/license-Proprietary-red.svg)](LICENSE.txt)
[![.NET 10](https://img.shields.io/badge/.NET-10-purple.svg)](https://dotnet.microsoft.com/)
[![AOT Ready](https://img.shields.io/badge/AOT-Native-green.svg)](https://learn.microsoft.com/dotnet/core/deploying/native-aot)
[![Derleme ve Test](https://github.com/imrankagan/elBariProject/actions/workflows/derleme-ve-test.yml/badge.svg)](https://github.com/imrankagan/elBariProject/actions/workflows/derleme-ve-test.yml)

## 🎯 Genel Bakış

ElBâri, tamsayı telemetri verisi için **kayıpsız**, **tahsisatsız** (zero-allocation)
ve **bağımlılıksız** bir sıkıştırma motorudur. Özellikle **İHA/drone telemetrisi,
gömülü sistemler ve kayıplı telsiz linkleri** için tasarlanmıştır.

Öne çıkan ayırt edici özelliği ham sıkıştırma oranı **değildir** — o alanda zstd/LZ4
gibi olgun kütüphaneler öndedir. ElBâri'nin değeri, bu üç garantiyi **bir arada**
sunmasıdır:

1. **Kayıplı link üzerinde çalışabilme** — bir paket düşerse yalnızca o paketin
   kayıtları kaybolur; sonraki veri etkilenmez (aşağıya bkz. *Paket Kaybı Dayanıklılığı*).
2. **Sıfır heap tahsisatı** — GC duraklaması yok, deterministik davranış.
3. **Bağımlılıksız ve her yere taşınabilir** — harici kütüphane yok. .NET sürümü Native
   AOT ile makine koduna derlenir; **C sürümü** ise RTOS ve bare-metal dahil derleyicisi
   olan her mimariye girer. İkisi bit bit aynı çıktı üretir.

Kodun görüntülenmesi, değiştirilmesi veya kullanılması için geçerli bir lisans gereklidir.
GitHub'daki görünürlük **sadece tanıtım amaçlıdır**.

## 🧱 Mimari — Üç Katman

ElBâri üç bağımsız katmandan oluşur. Her katman bir öncekinin üzerine oturur; ihtiyacına
göre yalnızca gerekeni kullanırsın.

| Katman | C# | C | Ne işe yarar |
| --- | --- | --- | --- |
| **Çekirdek** | [ElBâri.cs](ElB%C3%A2ri.cs) | [elbari.c](c/src/elbari.c) | `ElKâbıd` (kodlayıcı) / `ElBâsıt` (çözücü). Tek bir tamsayı akışını delta + adaptif bit-packing ile sıkıştırır. |
| **Kanal** | [ElBâriKanal.cs](ElB%C3%A2riKanal.cs) | [elbari_kanal.c](c/src/elbari_kanal.c) | Çok kanallı telemetriyi (kayıt akışı) kanallara ayırıp her kanalı kendi içinde sıkıştırır. Kanal başına adaptif fark derecesi seçer. |
| **Çerçeve** | [ElBâriÇerçeve.cs](ElB%C3%A2ri%C3%87er%C3%A7eve.cs) | [elbari_cerceve.c](c/src/elbari_cerceve.c) | Akışı bağımsız çözülebilir, sıra numaralı, CRC32 korumalı çerçevelere böler. Paket kaybına dayanıklılık buradan gelir. |

**İki implementasyon, tek biçim.** C# sürümü SIMD hızlandırmalıdır ve sunucu/yardımcı
bilgisayar tarafını hedefler; C sürümü bağımlılıksızdır ve gömülü/RTOS hedeflerine girer.
İkisi **bit bit aynı** çıktı üretir ve birbirinin çıktısını çözebilir — bu, gerçek GPS
verisiyle doğrulanmıştır (bkz. [C Sürümü](#-c-sürümü-gömülü-ve-savunma-hedefleri)).

### Neden Kanal Katmanı gerekli?

Gerçek telemetri tek bir sayı akışı değil, bir **kayıt akışıdır**:

```
[lat, lon, alt, roll, pitch, yaw,  lat, lon, alt, roll, pitch, yaw,  ...]
```

Bu akış olduğu gibi çekirdek `ElKâbıd`'a verilirse, ardışık farklar kanallar arasında
zıplar (`lat → lon` farkı milyonlarca birim olur), aykırı oranı %100'e çıkar ve veri
**"sıkıştırılamaz" diye reddedilir**. Kanal katmanı akışı önce ayırır; böylece her
kanal kendi içinde düzgün delta üretir.

> Ölçülmüş etki (gerçek GPS verisi): kanal ayrımı **olmadan REDDEDİLİYOR** → kanal
> ayrımı **ile 3.56x**. Yani birincil hedef veri tipi ancak bu katmanla çalışıyor.

## ✨ Özellikler

- **Kayıpsız Sıkıştırma** — 1600+ test ve fuzz turu ile doğrulandı, %100 veri bütünlüğü
- **Zero-Allocation** — `Span<T>` tabanlı; çalışma alanı çağıran tarafından verilir
  (ölçüldü: 100 encode+decode turunda **0 bayt** heap tahsisatı)
- **Çok Mimarili SIMD**:
  - ✅ **Intel/AMD**: AVX2 (8×32-bit paralel) — bu makinede ölçüldü
  - 🧩 **ARM**: NEON (4×32-bit paralel) — kod mevcut, gerçek ARM donanımında henüz
    benchmark edilmedi
  - ✅ **Eski işlemciler**: Scalar fallback (her zaman çalışır)
- **Adaptif Bit-Width** — blok başına 2/4/8/16 bit, aykırı değerler için 32 bit
- **Kanal Başına Adaptif Fark Derecesi** — düzgün kanallar (sabit hızlı GPS) ikinci
  derece farkı, gürültülü kanallar birinci dereceyi seçer
- **Paket Kaybı Dayanıklılığı** — bağımsız çerçeveler + CRC32
- **Ham Geçiş Güvenliği** — sıkışmayan kanal ham yazılır; "reddedildi" durumunda veri
  kaybı olmaz
- **Native AOT** — IL yok, JIT ısınması yok, deterministik çalışma süresi
- **Gömülü Sistem Modu** — `EMBEDDED_MODE` ile exception-free çalışma

## 📊 Ölçülen Performans

> **Metodoloji:** Aşağıdaki sayılar **gerçek** veri üzerinde ölçülmüştür — sentetik
> değil. Veri: OpenStreetMap halka açık GPS iz arşivinden 24.642 kayıt (lat/lon/zaman).
> Ortam: .NET 10, 24 çekirdekli x64, AVX2 aktif, Release + Native AOT, tek iş parçacığı.
> Kaynak/lisans: [TestData/KAYNAK.md](TestData/KAYNAK.md).

### Çekirdek + Kanal Katmanı (tek blok)

| İşlem | Verim | Hız | Oran |
| --- | --- | --- | --- |
| encode | ~76M kayıt/sn | **871 MB/sn** | 3.56x |
| decode | ~92M kayıt/sn | **1.049 MB/sn** | — |

### Çerçeve Katmanı (100 kayıt/çerçeve, paket kaybına dayanıklı)

| İşlem | Verim | Hız | Çerçeve başına |
| --- | --- | --- | --- |
| encode | ~22M kayıt/sn | 253 MB/sn | 4.5 µs |
| decode | ~53M kayıt/sn | 606 MB/sn | 1.9 µs (CRC dahil) |

Çerçeveleme, dayanıklılık karşılığında oranı hafifçe (3.56x → 3.37x) ve encode hızını
düşürür (küçük bloklar + kanal başına heuristik + CRC). Buna karşılık kayıplı linkte
çalışabilirlik kazanılır.

### Tahsisat

```
100 encode+decode turu, gerçek GPS verisi:
  Tahsis edilen bayt: 0
  ✓ SIFIR tahsisat — heap'e hiç dokunulmadı, GC baskısı yok.
```

## ⚖️ Karşılaştırma — Zstd / LZ4 / Brotli / Deflate

> **Metodoloji:** Aynı makinede, aynı gerçek GPS verisiyle (295.704 B ham), 20 tur ısınma
> + 200 tur ölçüm. Rakipler resmî .NET paketleriyle çalıştırıldı:
> `ZstdSharp.Port`, `K4os.Compression.LZ4`, ve .NET yerleşik `BrotliStream` /
> `DeflateStream` / `GZipStream`. Tüm yöntemlerde round-trip doğrulandı (kayıpsız).
> Hız değerleri **ham veri üzerinden** MB/sn'dir.

### Ham baytlar (telemetriyi olduğu gibi sıkıştırıcıya vermek — yaygın "naif" entegrasyon)

| Yöntem | Boyut | Oran | Encode | Decode |
| --- | ---: | ---: | ---: | ---: |
| **ElBâri — kanal ayrımı** | **83.124 B** | **3.56x** | **873 MB/sn** | **1.109 MB/sn** |
| **ElBâri — çerçeveli (100)** | 87.853 B | 3.37x | 222 MB/sn | 251 MB/sn |
| Zstd (seviye 1) | 184.181 B | 1.61x | 210 MB/sn | 313 MB/sn |
| Zstd (seviye 3) | 175.535 B | 1.68x | 156 MB/sn | 323 MB/sn |
| Zstd (seviye 9) | 172.483 B | 1.71x | 81 MB/sn | 804 MB/sn |
| Zstd (seviye 19) | 97.481 B | 3.03x | 8 MB/sn | 471 MB/sn |
| LZ4 (hızlı) | 228.684 B | 1.29x | 923 MB/sn | 3.101 MB/sn |
| LZ4 (HC-9) | 219.544 B | 1.35x | 62 MB/sn | 2.958 MB/sn |
| Brotli (q5) | 90.367 B | 3.27x | 36 MB/sn | 346 MB/sn |
| Brotli (q11) | 82.471 B | 3.59x | 1 MB/sn | 291 MB/sn |
| Deflate (optimal) | 167.385 B | 1.77x | 62 MB/sn | 538 MB/sn |
| Gzip (optimal) | 167.403 B | 1.77x | 61 MB/sn | 521 MB/sn |

### Kanal-ayrılmış baytlar (rakiplere avantaj: veriyi biz ön-işleyip verdik)

| Yöntem | Boyut | Oran | Encode | Decode |
| --- | ---: | ---: | ---: | ---: |
| Zstd (seviye 1) | 188.464 B | 1.57x | 590 MB/sn | 887 MB/sn |
| Zstd (seviye 19) | 87.076 B | 3.40x | 7 MB/sn | 394 MB/sn |
| LZ4 (hızlı) | 234.028 B | 1.26x | 1.247 MB/sn | 5.947 MB/sn |
| Brotli (q11) | **76.241 B** | **3.88x** | 1 MB/sn | 305 MB/sn |
| Deflate (optimal) | 162.898 B | 1.82x | 62 MB/sn | 490 MB/sn |

### Sonuçların yorumu

**1. "Sıkıştırıcı yapıştırmak" telemetride yetersiz kalıyor.**
Yaygın yaklaşım telemetriyi olduğu gibi Zstd/LZ4'e vermektir. Ölçüm bunun zayıf kaldığını
gösteriyor: Zstd-1 yalnızca **1.61x**, LZ4 **1.29x** veriyor. ElBâri **3.56x** ile bunların
**iki katından fazla** sıkıştırıyor ve aynı zamanda daha hızlı. Sebep basit — genel
sıkıştırıcılar veriyi anlamsız bir bayt yığını olarak görür; kanalların iç içe geçmesi
onların örüntü aramasını köreltir. ElBâri verinin **kayıt yapısını bilir**.

**2. Hız/oran ödünleşiminde boş bir köşe dolduruluyor.**
Rakipler iki uçtan birinde: ya hızlı ama zayıf oran (LZ4 1.29x, Zstd-1 1.61x), ya iyi oran
ama çok yavaş (Brotli-q11 3.59x @ 1 MB/sn, Zstd-19 3.03x @ 8 MB/sn). **Hem 3x üzeri oran
hem 800+ MB/sn hızı** aynı anda veren tek yöntem ElBâri'dir.

**3. Dürüst zayıflık: en yüksek oran bizde değil.**
Kanal-ayrılmış veride **Brotli-q11 3.88x** ile ElBâri'yi (3.56x) geçiyor. Ancak bunu
**~870 kat daha yavaş** encode hızıyla (1 MB/sn) yapıyor; ayrıca bellek ayırır,
deterministik değildir ve paket kaybına dayanıklı değildir. Sadece en yüksek oran
gerekiyorsa ve hız/determinizm önemsizse Brotli daha uygundur.

**4. Tabloda görünmeyen farklar.**
Bu ölçüm yalnızca oran ve hızı kapsar. Listedeki rakiplerin **hiçbirinde** şunlar yoktur:

| Özellik | ElBâri | Zstd / LZ4 / Brotli |
| --- | --- | --- |
| Paket kaybına dayanıklılık | ✅ Bağımsız çerçeveler | ❌ Akış bozulur |
| Sıfır heap tahsisatı | ✅ Ölçüldü (0 bayt) | ❌ Bellek ayırır |
| Deterministik gecikme | ✅ Sabit blok yapısı | ❌ Değişken |
| Harici bağımlılık | ✅ Yok (saf C#) | ❌ Native kütüphane |
| Çerçeve başına bütünlük | ✅ CRC32 | ❌ Yok (akış seviyesi) |

**Ne zaman ElBâri, ne zaman diğerleri?**

- **ElBâri**: kayıplı/dar RF linki, gerçek-zaman kısıtı, gömülü/denetlenebilir ortam,
  çok kanallı telemetri.
- **Zstd/Brotli**: güvenilir taşıma (TCP/dosya), hızın önemsiz olduğu arşivleme,
  maksimum oran hedefi.
- **LZ4**: saf hız gerekiyorsa ve düşük oran kabul edilebilirse.

## 🛡️ Paket Kaybı Dayanıklılığı (Ayırt Edici Özellik)

Klasik delta kodlamanın ölümcül zayıflığı **zincirleme bağımlılıktır**: her değer bir
öncekine dayanır, dolayısıyla tek bir paket düşerse ondan sonraki **tüm** veri çözülemez.
İHA telemetrisi ise kayıplı bir RF linki üzerinden gider — paket düşmesi normaldir.

Çerçeve katmanı akışı, her biri **kendi mutlak referansını taşıyan**, sıra numaralı ve
CRC32 korumalı bağımsız çerçevelere böler. Hata yayılımı tek çerçeve ile sınırlıdır.

Ölçülmüş sonuç (gerçek GPS verisi, 100 kayıt/çerçeve, rastgele paket kaybı):

| Paket kaybı | **Çerçeveli** (kurtarılan) | Çerçevesiz (klasik tek blok) |
| --- | --- | --- |
| %0 | %100.0 | %100.0 |
| %1 | **%99.2** | %0.0 |
| %5 | **%93.5** | %0.0 |
| %10 | **%88.6** | %0.0 |
| %25 | **%71.6** | %0.0 |
| %50 | **%45.6** | %0.0 |

Sağdaki sütun meselenin özü: klasik yaklaşımda **tek bir paket düşerse her şey gider**.
Çerçeveli yaklaşımda kayıp lineerdir — ne düştüyse o kadar.

**Bozulma tespiti:** 247 çerçeveye tek-bit bozulma enjekte edildi; **247'sinin tamamı
CRC ile yakalandı**, sessizce kabul edilen sıfır.

## 🛠️ Kullanım

### Tek akış (çekirdek)

```csharp
using ElBâri;

int[] data = { 100, 102, 103, 105, 200, 201 };
byte[] compressed = new byte[data.Length * 4 + 16];
int n = ElBâri.ElKâbıd(data, compressed);   // n == -1 => veri sıkıştırılamaz

int[] restored = new int[data.Length];
ElBâri.ElBâsıt(compressed.AsSpan(0, n), restored);
```

### Çok kanallı telemetri (kanal katmanı)

```csharp
using ElBâri;

int kanal = 3;                       // lat, lon, zaman
int[] kayitlar = /* iç içe akış */;

int[] calisma = new int[ElBâriKanal.GerekliCalismaAlani(kayitlar.Length, kanal)];
byte[] cikti  = new byte[ElBâriKanal.EnKotuDurumCiktiBoyutu(kayitlar.Length, kanal)];

int n = ElBâriKanal.ElKâbıdKanal(kayitlar, kanal, calisma, cikti);

int[] geri = new int[kayitlar.Length];
ElBâriKanal.ElBâsıtKanal(cikti.AsSpan(0, n), calisma, geri);
```

### Kayıplı link (çerçeve katmanı)

```csharp
using ElBâri;

int kanal = 6, kayitPerCerceve = 100;
int[] calisma = new int[ElBâriÇerçeve.GerekliCalismaAlani(kayitPerCerceve, kanal)];
byte[] paket  = new byte[ElBâriÇerçeve.EnKotuDurumCerceveBoyutu(kayitPerCerceve, kanal)];

// Gönderici: her çerçeveyi ayrı bir pakette yolla
int n = ElBâriÇerçeve.CerceveYaz(kayitDilimi, kanal, siraNo, calisma, paket);
Gonder(paket.AsSpan(0, n));

// Alıcı: her paketi bağımsız çöz — eksik sıra numaraları kayıptır
bool ok = ElBâriÇerçeve.CerceveOku(gelenPaket, kanal, calisma, cikti,
                                   out uint siraNo, out int kayitSayisi);
// ok == false => paket bozuk/eksik, atılmalı (kalanları etkilemez)
```

## 🔧 C Sürümü (gömülü ve savunma hedefleri)

Kaynak: [c/](c/) — ayrıntılı notlar [c/BENIOKU.md](c/BENIOKU.md)

### Neden var?

Savunma ve gömülü sistemlerde uçan yazılım neredeyse tamamen C'dir. Bunun üç sebebi var
ve üçü de .NET sürümünü dışarıda bırakır:

1. **Hedef donanım** — RTOS'lar (VxWorks, PikeOS, NuttX) ve bare-metal MCU'lar .NET
   çalıştırmaz. C her yere girer.
2. **Sertifikasyon** — DO-178C gibi havacılık standartları için C'nin olgun araç zinciri
   (nitelikli derleyici, statik analiz) vardır; .NET için pratikte yoktur.
3. **Denetim** — müşterinin güvenlik ekibi ~1.200 satır C'yi satır satır okuyabilir;
   içinde runtime gömülü birkaç MB'lık bir ikiliyi okuyamaz. Savunmada bu belirleyicidir.

Ek olarak C'nin **kararlı ABI**'si sayesinde kütüphaneyi her dil bağlayabilir
(C#, Python, Rust, MATLAB, C++).

### Tasarım kuralları

Kod baştan MISRA C disipliniyle yazılmıştır:

- **Kaynak C99 uyumlu, C17 ile derlenir.** C11 özellikleri yalnızca `#if` koruması
  arkasında, isteğe bağlı ek denetim olarak kullanılır — böylece eski/sertifikalı araç
  zincirleri de derleyebilir.
- **Dinamik bellek yok** — tüm tamponları çağıran verir
- **Özyineleme yok** — yığın derinliği sabit ve öngörülebilir
- **Tüm döngüler sınırlı** — sonsuz döngü oluşamaz
- **İstisna yok** — hatalar dönüş kodu ile bildirilir
- **Harici bağımlılık yok** — yalnızca `<stdint.h>`, `<string.h>`
- **İşaretli taşma yok** — C'de işaretli taşma tanımsız davranıştır; tüm fark hesapları
  işaretsiz aritmetik üzerinden yapılır. Bu, .NET'in `unchecked` davranışıyla birebir
  aynı sonucu üretir.
- **Bayt düzeni açık** — little-endian elle yazılır/okunur, big-endian işlemcide de
  aynı biçim üretilir

### İkili uyumluluk — ölçülmüş

C ve C# sürümleri **aynı bit dizisini** üretmelidir. Gerçek GPS verisiyle (24.642 kayıt)
doğrulandı:

```
--- Kanal katmanı ---
  [GEÇTİ] C çıktısı == .NET çıktısı            83124 bayt BİREBİR AYNI
  [GEÇTİ] C round-trip kayıpsız                tüm elemanlar birebir geri geldi
  [GEÇTİ] C, .NET çıktısını çözebiliyor        çapraz uyumluluk doğrulandı

--- Çerçeve katmanı ---
  [GEÇTİ] yaz/oku bağımsız ve kayıpsız         247 çerçeve, 3.37x
  [GEÇTİ] tek-bit bozulma CRC ile yakalandı    247/247

--- Kenar durumlar ---
  [GEÇTİ] NULL girdi reddedildi                çökme yok
  [GEÇTİ] yetersiz tampon reddedildi           çökme yok
  [GEÇTİ] rastgele bayt çerçeve değil          sihirli sayı/CRC tuttu

  SONUÇ: 10 geçti, 0 kaldı
```

> **Yan fayda:** İki bağımsız implementasyonun aynı çıktıyı üretmesi, **biçim
> spesifikasyonunun da doğrulandığı** anlamına gelir. Arayüz kontrol dokümanı (ICD)
> yazarken bu doğrudan kanıt olur.

### Derleme

```bash
# Windows (MSVC)
c\derle.bat

# Linux / macOS (gcc veya clang)
cc -std=c17 -O2 -Wall -Wextra -o dogrulama \
   c/test/dogrulama.c c/src/elbari.c c/src/elbari_kanal.c c/src/elbari_cerceve.c
```

### Kullanım (C)

```c
#include "elbari.h"

int32_t kanal = 3;                 /* enlem, boylam, zaman */
int32_t calisma[ELBARI_CALISMA];   /* boyut: elbari_kanal_gerekli_calisma_alani(...) */
uint8_t cikti[ELBARI_CIKTI];       /* boyut: elbari_kanal_en_kotu_durum_boyutu(...) */

int32_t n = elbari_kanal_kabid(kayitlar, eleman_sayisi, kanal,
                               calisma, ELBARI_CALISMA,
                               cikti, ELBARI_CIKTI);
if (n < 0) { /* hata kodu: ELBARI_HATA_* */ }

/* Kayıplı link: her çerçeve bağımsız gönderilir ve bağımsız çözülür */
int32_t paket_boyu = elbari_cerceve_yaz(kayit_dilimi, adet * kanal, kanal, sira_no,
                                        calisma, ELBARI_CALISMA, paket, ELBARI_PAKET);

int32_t durum = elbari_cerceve_oku(gelen_paket, gelen_boyut, kanal,
                                   calisma, ELBARI_CALISMA,
                                   geri, ELBARI_GERI, &sira_no, &kayit_sayisi);
/* durum != ELBARI_TAMAM  =>  paket bozuk/eksik, atılmalı (kalanları etkilemez) */
```

### Mevcut durum ve bilinen sınırlar

| Konu | Durum |
| --- | --- |
| Çekirdek / kanal / çerçeve katmanları | ✅ Tamamlandı |
| .NET ile ikili uyumluluk | ✅ Gerçek veriyle doğrulandı |
| MSVC x64 derleme | ✅ `/W4` ile 0 uyarı |
| Verim ve gecikme dağılımı ölçümü | ✅ Ölçüldü (aşağıda) |
| MISRA C:2012 uyum incelemesi | ✅ Elle yapıldı, belgelendi ([MISRA_UYUM.md](c/MISRA_UYUM.md)) |
| MSVC `/Wall /analyze` statik analiz | ✅ 0 bulgu |
| Sağlamlık (fuzz) testi | ✅ 400.000 tur, 0 tampon taşması |
| Sertifikalı MISRA aracıyla doğrulama | ⏳ Yapılmadı |
| GCC / Clang derleme | ✅ CI'da her push'ta (Linux) |
| ASan + UBSan (çalışma zamanı) | ✅ CI'da temiz |
| ARM / big-endian üzerinde doğrulama | ⏳ Henüz yapılmadı |
| Elle yazılmış SIMD | ⏳ Yok — saf skaler (derleyici otomatik vektörleştirmesi var) |

### C mi hızlı, C# mı? — ölçüldü

Sezgiye aykırı bir sonuç: **saf skaler C, SIMD'li C#'tan hızlı çıktı.**

| İşlem | C (skaler) | C# (AVX2) | Fark |
| --- | ---: | ---: | --- |
| encode | **1.055 MB/sn** | 873 MB/sn | C %21 hızlı |
| decode | **1.447 MB/sn** | 1.109 MB/sn | C %30 hızlı |

Sebebi: C# tarafındaki AVX2 yalnızca **fark hesabını ve aykırı maskeyi** hızlandırır.
İşin asıl yükü olan **bit paketleme döngüsü** her iki sürümde de skalerdir — yani SIMD
toplam işin küçük bir kısmına dokunur. Buna karşılık C tarafında dizi sınır kontrolü
yoktur ve derleyici bazı döngüleri kendiliğinden vektörleştirir.

> Pratik sonuç: gömülü/savunma hedefleri, aynı zamanda **daha hızlı** olan sürümü alır.
> Elle SIMD eklemek ileride ek kazanç sağlayabilir, ancak öncelik değildir.

### Gecikme dağılımı — "deterministik" iddiasının sınavı

Gerçek-zamanlı sistemde ortalama gecikme neredeyse anlamsızdır; önemli olan **en kötü
ihtimalle ne kadar sürdüğüdür**. Çerçeve başına (100 kayıt × 3 kanal), 246 çerçeve ×
200 tekrar:

| İşlem | en küçük | medyan | p95 | p99 | p99.9 | en büyük |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| encode | 1.20 µs | 1.80 µs | 2.10 µs | 2.70 µs | 4.00 µs | 18.30 µs |
| decode | 0.90 µs | 1.40 µs | 1.70 µs | 1.90 µs | 2.70 µs | 19.70 µs |

**Oynama oranı (p99 / medyan): encode 1.50x, decode 1.36x.** Bu dar bir bant.

**Veriye bağlı (algoritmik) değişkenlik: 1.78x.** Her çerçeve için 200 tekrarın ortalaması
alınarak işletim sistemi gürültüsü bastırıldığında, en yavaş çerçeve en hızlının iki
katından az sürüyor. Bu, sabit blok yapısının beklenen davranışıdır.

> **Dürüstlük notu:** Bu ölçüm genel amaçlı bir işletim sistemi üzerinde yapılmıştır.
> En büyük değerler (18–20 µs) büyük ölçüde **işletim sistemi gürültüsüdür** — zamanlayıcı
> kesintileri, sayfa hataları, frekans ölçekleme. Algoritmanın kendisi değildir. Gerçek
> en-kötü-durum (WCET) analizi ancak bir RTOS üzerinde ve statik analizle yapılabilir;
> bu henüz yapılmamıştır.

### MISRA C:2012 uyumu

Kod baştan MISRA disipliniyle yazıldı; ardından kural kural denetlendi. Tam matris ve
sapma kaydı: **[c/MISRA_UYUM.md](c/MISRA_UYUM.md)**

| Kategori | Durum |
| --- | --- |
| Zorunlu (Mandatory) kurallar | Bilinen ihlal yok |
| Gerekli (Required) kurallar | Bilinen ihlal yok |
| Tavsiye (Advisory) kurallar | 2 bilinçli sapma (gerekçeli) |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| MSVC `/W4` | ✅ 0 uyarı |

Öne çıkan noktalar:

- **Özyineleme yok** (Kural 17.2) — yığın derinliği sabit ve öngörülebilir
- **Dinamik bellek yok** (Dir 4.12) — tüm tamponları çağıran verir
- **Tanımsız davranış yok** (Kural 1.3) — C'de işaretli taşma tanımsızdır; tüm fark ve
  toplama işlemleri işaretsiz aritmetik üzerinden yapılır. Bu hem C'de tanımlıdır hem de
  .NET'in `unchecked` davranışıyla birebir aynı sonucu verir — ikili uyumluluğun temeli
- **Kaydırma sınırları kanıtlandı** (Kural 12.2) — en kötü durum 39 bit < 64
- **Dış girdi doğrulanır** (Dir 4.14) — çerçeve başlığı, CRC, kanal sayısı, yük boyutları

Kayıtlı sapmalar: tek çıkış noktası (koruma cümlesi tercihi) ve kayan nokta kullanımı
(.NET ile bit-bit aynı kararı üretmek için zorunlu; yalnızca karar verir, üretilen bit
akışına girmez).

#### İnceleme sırasında bulunan ve düzeltilen zafiyet

Boyut hesapları (`eleman_sayisi * 4 + pay`) 32 bit tamsayı ile yapılıyordu. Çok büyük bir
`eleman_sayisi` değeri çarpma sırasında taşarak **negatif ya da küçük** bir "gerekli
boyut" üretebilir, çağıran da yetersiz bir tampon ayırabilirdi — yani **tampon taşması**.

Kritik nokta: bu değer çözücü tarafında **bozuk/düşmanca bir paketten** de gelebiliyordu.

Düzeltme: `ELBARI_MAKS_ELEMAN` sınırı tanımlandı ve tüm giriş noktalarında doğrulanıyor;
çarpım taşması bölme ile önceden denetleniyor:

```c
if (kayit_sayisi > (ELBARI_MAKS_ELEMAN / kanal_sayisi))
{
    return ELBARI_HATA_PARAMETRE;
}
```

> **Dürüstlük notu:** [MISRA_UYUM.md](c/MISRA_UYUM.md) **elle yapılmış bir
> öz-değerlendirmedir.** Sertifikalı bir MISRA aracıyla (Helix QAC, PC-lint Plus,
> Polyspace) doğrulanmamıştır ve resmî bir uygunluk beyanı değildir. Ticari teslimat
> öncesi nitelikli bir araçla doğrulanmalıdır.

### Sağlamlık (fuzz) testi — düşmanca girdiye dayanıklılık

Telemetri çözücüsü kayıplı ve **düşmanca** bir telsiz ortamından veri alır. Saldırgan,
özel hazırlanmış bir paketle alıcı sistemi çökertmeye çalışabilir. Bu yüzden çözücünün
**hiçbir girdide** çökmemesi, taşmaması ve kilitlenmemesi bir güvenlik gereksinimidir.

**Yöntem:** Çıktı tamponlarının önüne ve arkasına bilinen bir desen ("kanarya") yazılır;
çağrı sonrası desen bozulmuşsa kütüphane tampon dışına yazmış demektir. Bu, çökmeye yol
açmayan **sessiz taşmaları** da yakalar. Üreteç deterministiktir — bulunan her hata
birebir yeniden üretilebilir.

**Sonuç (400.000 tur):**

| Ölçüt | Sonuç |
| --- | --- |
| **Tampon taşması** | **0** |
| Süreç çökmesi | Yok |
| Bozulmuş çerçevelerin reddi | **%100.00** (99.790 / 99.790) |

Katman kırılımı:

| Katman | Kabul | Red | Bütünlük kontrolü |
| --- | ---: | ---: | --- |
| Çekirdek | **17** | 100.568 | Yapısal tüketim kontrolü |
| Kanal | **0** | 199.625 | Başlık tutarlılık kontrolü |
| Çerçeve (bozulmuş) | **0** | 99.790 | **CRC32** |

> **Yapısal tüketim kontrolü — sağlama toplamı olmadan çöpü elemek.**
> Geçerli bir sıkıştırılmış akış girdinin **tamamını** tüketir: kodlayıcı tam olarak
> gerektiği kadar bayt yazar, çözücü de tam olarak o kadarını okur. Geriye artık
> kalmışsa girdi bu kodlayıcıdan çıkmamıştır. Maliyeti **tek bir karşılaştırmadır**.
>
> Etkisi ölçüldü: çekirdeğin kabul ettiği çöp girdi **88.963 → 17** (%99,98 azalma).
> Bu, sağlama toplamının yerini tutmaz — tam bütünlük için çerçeve katmanı gerekir —
> ama tuzağın büyük kısmını kapatır.
>
> ⚠️ **Kullanım şartı:** `ElBâsıt`/`elbari_basit`'e sıkıştırılmış verinin **tam boyutu**
> verilmelidir. Çözücü verinin nerede bittiğini kendi başına bilemez; fazla büyük bir
> tampon verilirse akış reddedilir.

> **Ölçüm düzeltmesi:** İlk koşuda 17 bozulmuş çerçeve kabul edilmiş görünüyordu. CRC32
> çarpışması bu sıklıkta olamayacağı için araştırıldı: fuzzer bozma yaparken rastgele bir
> değer *atıyordu*, yani 1/256 olasılıkla aynı baytı yazıp paketi aslında hiç bozmuyordu.
> Bozma XOR'a çevrildi; reddetme oranı %100.00 oldu. **Kütüphanede hata yoktu, ölçümde
> vardı.**

### ⚠️ Katmanlı bütünlük modeli — API kullanım kuralı

Fuzz sonuçları önemli bir tasarım gerçeğini görünür kılıyor: **çekirdek çözücü 88.963
rastgele girdiyi kabul etti.**

Bunun nedeni, bütünlük kontrolünün **yalnızca çerçeve katmanında** olmasıdır:

| Katman | Bütünlük kontrolü | Güvenilmeyen veriye uygulanabilir mi? |
| --- | --- | --- |
| `elbari_basit` (çekirdek) | Yapısal tüketim kontrolü (sağlama toplamı yok) | ⚠️ **Tercih edilmez** |
| `elbari_kanal_basit` (kanal) | Yalnızca başlık tutarlılığı | ❌ **Hayır** |
| `elbari_cerceve_oku` (çerçeve) | ✅ CRC32 | ✅ **Evet** |

Bu bilinçli bir tasarım tercihidir — alt katmanlar sıcak yolda çalışır ve zaten
doğrulanmış veri üzerinde işlem yapmaları beklenir; sağlama toplamını her katmanda
tekrarlamak gereksiz maliyet olurdu.

Ancak bunun bir sonucu vardır: çekirdek çözücüye bozuk bayt verilirse **hata
döndürmeyebilir** — bit akışını olduğu gibi yorumlar ve **anlamsız veri üretir**. Bu bir
güvenlik açığı değildir (tampon taşmaz, süreç çökmez), ama **sessizce yanlış veri**
demektir.

> **KURAL:** Güvenilmeyen kaynaktan (telsiz linki, ağ, disk) gelen veri **daima çerçeve
> katmanından** geçirilmelidir. `elbari_basit` ve `elbari_kanal_basit` doğrudan
> güvenilmeyen veriye uygulanmamalıdır.

Aynı uyarı [`c/src/elbari.h`](c/src/elbari.h) başında da yer alır.

### Sürekli tümleştirme (CI)

Geliştirme Windows/MSVC üzerinde yapılıyor; ancak C sürümünün **varlık sebebi** Linux'lu
yardımcı bilgisayarlar, ARM kartlar ve RTOS'lardır — bunların derleyicisi neredeyse
istisnasız GCC ya da Clang'dır. Yani kod, hedef kitlenin hiç kullanmayacağı derleyiciyle
test ediliyordu.

[GitHub Actions](.github/workflows/derleme-ve-test.yml) her `push`'ta:

| İş | Ne yapar |
| --- | --- |
| **C (gcc)** | Gerçek Linux'ta sıkı uyarılarla derler, uygunluk + fuzz koşar |
| **C (clang)** | Aynısı Clang ile — farklı derleyici farklı hata yakalar |
| **C denetleyicileri** | **ASan + UBSan** altında koşar |
| **.NET** | Derler ve 32 senaryoluk test paketini çalıştırır |

> **Denetleyiciler bu işin en değerli parçası.** MISRA belgesinde *"Kural 1.3 — tanımsız
> davranış yok"* diye iddia ediyoruz; bunu daha önce yalnızca **elle inceleyerek**
> doğrulamıştık. UBSan bunu **çalıştırarak** kanıtlar. `-fno-sanitize-recover=all` ile
> herhangi bir ihlalde süreç sıfır dışı kodla biter.

#### CI kurulur kurulmaz gerçek bir hata yakaladı

İlk koşuda GCC ve Clang şu hatayı verdi:

```
error: 'CLOCK_MONOTONIC' undeclared
error: call to undeclared function 'clock_gettime'
```

`clock_gettime` bir POSIX işlevidir, ISO C'nin parçası değildir. `-std=c17` (katı ISO C)
ile derlenince glibc onu gizler; `_POSIX_C_SOURCE` tanımlanmalıydı.

**Bu hata Windows'ta asla görülemezdi** — orada `QueryPerformanceCounter` dalı derleniyor,
POSIX dalı hiç ziyaret edilmiyordu. Tam olarak CI'ın var olma sebebi olan türden bir hata.

> Önemli not: **kütüphanenin kendisi ilk denemede temiz derlendi.** Sorun yalnızca ölçüm
> test dosyasındaydı; `c/src/` altındaki kod taşınabilir çıktı.

### Linux / macOS derleme

```bash
cd c
make                 # tüm test programları
make CC=clang        # clang ile
make sanitize        # ASan + UBSan sürümleri
make test            # derle + uygunluk + fuzz
```

### Ölçümü kendiniz çalıştırın

```bash
c\derle.bat                      # /W4 ile derleme
c\analiz.bat                     # MSVC statik analiz
dogrulama.exe <referans_dizini>  # .NET ile ikili uyumluluk
olcum.exe <referans_dizini>      # verim + gecikme dağılımı
fuzz.exe [tur_sayisi]            # düşmanca girdi sağlamlık testi
```

## 🔢 Float (Ondalıklı) Telemetri Desteği

Çekirdek motor tamsayı üzerinde çalışır. Gerçek telemetrinin önemli bir kısmı ise
ondalıklı taşınır: yönelim açıları, hız, ivme, batarya gerilimi, quaternion bileşenleri.

**Kuantalama katmanı** ([ElBâriFloat.cs](ElB%C3%A2riFloat.cs) / [elbari_float.c](c/src/elbari_float.c))
ondalıklı değerleri istenen **hassasiyete** göre ölçekleyip tamsayıya çevirir. Sonuç mevcut
kanal ve çerçeve katmanlarına olduğu gibi verilir — **biçim değişmez**.

### ⚠️ Bu katman kayıplıdır

Kuantalama, seçilen hassasiyetin altındaki kısmı atar. Telemetri için genellikle istenen
davranış budur: bir yönelim açısını 0.001 radyan (0.06°) hassasiyetle taşımak fazlasıyla
yeterlidir ve tam float taşımak bant genişliği israfıdır.

**Ancak** tam değerin korunması gereken veriler (ham sensör kaydı, uçuş sonrası analiz,
kriptografik malzeme) bu katmandan **geçirilmemelidir**. Onlar için **kayıpsız XOR
katmanı** vardır (aşağıya bakınız).

### Ölçülen sonuç — gerçekçi uçuş verisi

12.000 kayıt × 6 kanal (roll, pitch, yaw, hız, batarya, irtifa), 50 Hz:

| Yöntem | Boyut | Oran |
| --- | ---: | ---: |
| Ham float32 | 288.000 B | — |
| **Kuantalama + kanal katmanı** | **35.935 B** | **8.01x** |
| Float bit desenini doğrudan vermek | 195.039 B | 1.48x |

> Kuantalama, float bit desenini doğrudan sıkıştırmaktan **5.4 kat** daha iyi. Sebebi:
> float bit desenlerinin ardışık farkları büyük ve düzensizdir; kuantalanmış tamsayılar
> ise düzgün delta üretir.

### Kuantalama hatası — ölçüldü

| Kanal | Hedef hassasiyet | Ölçülen maks hata |
| --- | ---: | ---: |
| roll / pitch / yaw | 0.001 rad | 5.0 × 10⁻⁴ |
| hız / batarya / irtifa | 0.01 birim | 5.0 × 10⁻³ |

Her kanalda hata **hassasiyetin tam yarısını** aşmıyor — kuantalamanın matematiksel
olarak ulaşabileceği en iyi sonuç bu.

### İkili uyumluluk

Kayan nokta yuvarlaması iki dilde kolayca ayrışır (C# varsayılanı bankacı yuvarlamasıdır).
Bu yüzden her iki sürümde de hesap çift duyarlıkta yapılır ve yuvarlama açıkça
**sıfırdan uzağa** uygulanır. Doğrulandı:

```
[GEÇTİ] float: C kuantalaması == .NET      72.000 değerin tamamı aynı yuvarlandı
[GEÇTİ] float: sıkıştırma C == .NET        35.935 bayt birebir aynı
[GEÇTİ] float: tam tur hata sınırı içinde  maks hata 4.999e-03, sınır 5.005e-03
```

### Kullanım

```csharp
float[] olcekler = { 1000f, 1000f, 1000f, 100f, 100f, 100f };  // kanal başına
int[] tamsayi = new int[veri.Length];

ElBâriFloat.KuantalaKanalli(veri, kanal, olcekler, tamsayi);
int n = ElBâriKanal.ElKâbıdKanal(tamsayi, kanal, calisma, cikti);
// ...
ElBâriFloat.CozKanalli(geriTamsayi, kanal, olcekler, geriFloat);
```

> **Ölçekler biçim içinde taşınmaz.** Gönderici ve alıcı aynı ölçek dizisini kullanmak
> zorundadır (telemetri şemasının parçası olarak, bant dışı). Bu, MAVLink gibi
> protokollerin çalışma biçimiyle aynıdır: alan tanımları iki tarafta da bilinir.

### Kayıpsız float — XOR katmanı

Tam değerin korunması şartsa: [ElBâriFloatXor.cs](ElB%C3%A2riFloatXor.cs) /
[elbari_float_xor.c](c/src/elbari_float_xor.c)

Ardışık float'ların **bit desenleri XOR'lanır**. Birbirine yakın değerlerde işaret, üstel
kısım ve mantisin üst bitleri aynıdır; XOR sonucunun başında ve sonunda çok sayıda sıfır
bulunur ve yalnızca ortadaki anlamlı bitler yazılır. Değer hiç değişmemişse **tek bit**
yeter. Literatürde Gorilla (Facebook, 2015) / Chimp olarak bilinir.

#### ⚠️ Ölçüldü: kayıpsız float çoğu telemetride az kazandırır

| Veri tipi | Kayıpsız (XOR) | Kayıplı (kuantalama) |
| --- | ---: | ---: |
| Gürültülü uçuş verisi (gerçekçi) | **1.21x** | **8.01x** |
| Durağan veri (çok tekrar eden) | **15.08x** | 8.71x |
| Düzgün sinyal (gürültüsüz) | **1.00x** | 12.71x |

Bu tablo dürüst bir beklenti yönetimi sunar:

- **Gürültülü sensör verisinde XOR neredeyse hiç kazandırmaz** (1.21x). Sebep: gürültü
  mantisin alt bitlerini her örneklemde değiştirir ve bu bitler tanımı gereği
  sıkıştırılamaz.
- **Düzgün sinyalde 1.00x** — yani hiç sıkışmaz, ham geçişe düşer. Sürekli değişen bir
  değerin ardışık bit desenleri çok farklıdır.
- **XOR yalnızca değerler AYNEN tekrar ettiğinde parlar** (15.08x). Gorilla'nın tasarlandığı
  senaryo tam olarak budur: izleme verisinde değerler çoğu zaman hiç değişmez.

> **Kural:** Tam değer gerekmiyorsa **kuantalama kullanın** — çoğu telemetride kat kat
> iyidir. XOR katmanı "mecbur kalınca" içindir: ham sensör kaydı, adli inceleme, uçuş
> sonrası tam veri saklama.

#### Kayıpsızlık doğrulandı

Özel float değerleri dahil **bit bit** aynı geri geliyor:

```
gurultulu       : TAM AYNI (bit bit)
duragan         : TAM AYNI (bit bit)
duzgun          : TAM AYNI (bit bit)
ozel degerler   : TAM AYNI (bit bit)   <- NaN, -0.0, ±sonsuz, epsilon, MaxValue
```

NaN karşılaştırması `==` ile yapılamadığı için doğrulama **bit deseni** üzerinden
yapılmaktadır.

## 📐 Biçim Spesifikasyonu ve Uygunluk Vektörleri

Bayt düzeyindeki veri biçimi tam olarak belgelenmiştir:
**[BICIM_SPESIFIKASYONU.md](BICIM_SPESIFIKASYONU.md)** (Arayüz Kontrol Dokümanı / ICD)

Belge, bağımsız bir tarafın sıfırdan uyumlu bir kodlayıcı/çözücü yazabilmesi için
gereken her şeyi içerir: üç katmanın bayt düzeni, blok/etiket kodlaması, bit genişliği
seçim kuralları, aykırı değer mekanizması, ikinci derece fark, ham geçiş, CRC kapsamı
ve doğrulama sırası.

### Dondurulmuş uygunluk vektörleri

[`TestVectors/vektorler.txt`](TestVectors/vektorler.txt) — 18 referans vektör. Her bit
genişliğini (2/4/8/16), aykırı değerleri, kısmi blokları, ikinci derece farkı, ham
geçişi ve çerçeve başlığını kapsar.

Bir implementasyon uyumlu sayılır **ancak ve ancak**:

1. Her vektörün girdisinden **birebir aynı bayt dizisini** üretiyorsa, **ve**
2. Her vektörün çıktısından **birebir aynı girdiyi** geri kurabiliyorsa.

| Implementasyon | Uygunluk |
| --- | --- |
| C# (.NET 10) | ✅ Referans — vektörler bundan üretildi |
| C (C99/C17) | ✅ **18 vektör, 36 kontrol, 0 hata** |

```bash
c\derle.bat
uygunluk.exe ../TestVectors/vektorler.txt
```

> **Neden bu önemli:** Savunma ve havacılık tedarikinde satın alınan şey koddan çok
> **spesifikasyondur**. İki bağımsız implementasyonun aynı vektörleri üretmesi, biçimin
> belgeyle tutarlı olduğunun kanıtıdır — belge ile kod arasında sessiz bir sapma yoktur.

## 🧪 Test ve Doğrulama

Benchmark suite'i projenin kendi içinde çalışır ve **gerçek GPS verisini** kullanır:

```bash
dotnet run --configuration Debug
```

### Suite Sonucu (32 senaryo)

```
Total Tests:  32
Passed:       25
Rejected:     7   (sıkıştırılamaz / anlamsız veri — beklenen)
Failed:       0
Success Rate: 100.0%
```

| Kategori | Test | Not |
| --- | --- | --- |
| Correctness | 4 | Edge case, boş dizi, tek eleman |
| Compression Quality | 5 | Sequential, constant, dense |
| Performance | 3 | Random, sparse, büyük veri |
| Real-World | 5 | Sine, Gaussian, telemetri |
| Stress | 4 | Worst case, 1M eleman |
| Edge Cases | 4 | Min/max int, mixed, zigzag |
| **Real Data** | **4** | **Gerçek GPS: kanal ayrımsız / ayrımlı / çerçeveli** |
| **Multi-Channel** | **3** | **6 kanallı İHA telemetrisi** |

### Gerçek veri senaryolarının çıktısı

| Senaryo | Sonuç |
| --- | --- |
| GERÇEK GPS — kanal ayrımsız | ⊘ REDDEDİLDİ (beklenen) |
| GERÇEK GPS — kanal ayrımı | **3.56x** |
| GERÇEK GPS — çerçeveli (100) | **3.37x** (CRC dahil) |
| GERÇEK GPS — çerçeveli (500) | **3.68x** |
| İHA 6 kanal — kanal ayrımsız | ⊘ REDDEDİLDİ (beklenen) |
| İHA 6 kanal — kanal ayrımı | **6.36x** |
| İHA 6 kanal — çerçeveli (250) | **5.91x** |

### Kayıpsızlık doğrulaması

Her iki katman için **1.617 test** (14 hedefli senaryo + 1.600 tur rastgele fuzz)
çalıştırıldı; **tamamı kayıpsız**. Çerçeveler ters sırada ve birbirinden bağımsız
çözülebiliyor. Kenar durumlar dahil: tek eleman, eksik kayıt, `K=255`,
`int.MinValue/MaxValue`, tümü sıfır, saf rastgele veri.

## 🔐 Kod Koruma

ElBâri'nin koruması **Native AOT temellidir** — abartılı runtime hileleri değil.

| Katman | Teknoloji | Etki |
| --- | --- | --- |
| **Native AOT** | `PublishAot=true` | IL bytecode yok; ILSpy/dnSpy/dotPeek gibi .NET decompiler'lar işe yaramaz (C++ assembly zorluğu) |
| **No Debug Symbols** | `DebugType=none` (Release) | PDB yok, sembol bilgisi yok |
| **Reflection Disabled** | `IlcDisableReflection=true` | Runtime type inspection kapalı |
| **Full Trimming** | IL trimmer | Kullanılmayan metadata çıkarılır |

> **Not:** Önceki sürümlerde bulunan runtime "Anti-Tamper / Anti-Debug" katmanı ve
> Obfuscar build pipeline'ı kaldırılmıştır. `Debugger.IsAttached` / assembly-hash
> kontrolü gibi teknikler .NET 6+ / Native AOT bağlamında güvenilir koruma sağlamıyor;
> Obfuscar ise hiç kurulmamıştı ve IL'i karıştırdığı için Native AOT'un ürettiği makine
> koduna bir katkısı olmuyordu. Gerçek koruma Native AOT + trimming + sembolsüz derlemedir.

## 🚁 İHA ve Gömülü Sistem Uyumluluğu

### Nerede çalışır?

Hangi sürümü kullandığına bağlı:

| Donanım | Örnek | C# / .NET AOT | C |
| --- | --- | --- | --- |
| **Yer istasyonu / sunucu** | Masaüstü, kenar sunucusu | ✅ | ✅ |
| **Yardımcı bilgisayar** (Linux) | Raspberry Pi, NVIDIA Jetson, x86 SBC | ✅ | ✅ |
| **Görev bilgisayarı** (RTOS) | VxWorks, PikeOS, INTEGRITY | ❌ | ✅ |
| **Uçuş kartı** (bare-metal MCU) | Pixhawk, STM32, Cortex-M | ❌ | ✅ |
| **DSP / özel donanım** | TI, RISC-V | ❌ | ✅ |

.NET/Native AOT, altında bir işletim sistemi (Windows/Linux/macOS) bekler; gerçek-zamanlı
işletim sistemleri ve bare-metal hedefler .NET çalıştırmaz. Ayrıca bir AOT ikilisi tipik
olarak birkaç MB'dır — Pixhawk sınıfı bir kartın *toplam* flash'ı ~2 MB'dır, yani hedef
desteklense bile sığmazdı.

**C sürümü bu kısıtı tamamen kaldırır.** Dinamik bellek, işletim sistemi çağrısı ve harici
bağımlılık kullanmadığı için derleyicisi olan her mimariye girer.

### Neden bu alana uygun?

- **Deterministik süre** — sabit blok yapısı sayesinde en-kötü-durum gecikmesi
  öngörülebilir (genel amaçlı sıkıştırıcılar bunu vermez)
- **Sıfır tahsisat** — GC duraklaması olmayan gerçek-zaman uyumu
- **Kayıplı link toleransı** — RF/telsiz senaryoları için çerçeveleme
- **Bağımlılıksız** — cross-compile edilebilir

### SIMD desteği

| Platform | SIMD | Durum |
| --- | --- | --- |
| Intel/AMD (Xeon/Core) | AVX2 | ✅ Ölçüldü |
| ARM (Cortex-A, Snapdragon, Jetson) | NEON | 🧩 Kod mevcut, gerçek donanımda benchmark bekliyor |
| Diğer / eski | Scalar | ✅ Her zaman çalışır |

> ARM NEON hızlanma rakamları henüz gerçek donanımda doğrulanmadığı için buraya somut
> "Nx hızlanma" sayısı yazılmamıştır. Doğrulandığında eklenecektir.

### Gömülü Sistem Modu (`EMBEDDED_MODE`)

Derleme zamanı anahtarıdır:

```xml
<DefineConstants>EMBEDDED_MODE</DefineConstants>
```

veya:

```bash
dotnet build -p:DefineConstants=EMBEDDED_MODE
```

Aktifken: exception fırlatılmaz (tampon taşmasında sessiz erken çıkış), gerçek-zaman
kısıtlarına uyumlu, kritik gömülü uygulamalar için uygundur.

## 🏗️ Algoritma Detayları

ElBâri, bilinen ve yayınlanmış tekniklerin bir uygulamasıdır — yeni bir algoritma
iddiasında değildir:

1. **Delta encoding** — ardışık değerler arasındaki farkı kodlar
2. **Frame of Reference (FOR)** — blok başına adaptif bit genişliği
3. **PFOR / patching** — büyük sapmaları (aykırı değer) ayrı 32-bit liste ile işler
4. **SIMD hızlandırma** — AVX2 / NEON / scalar

Bu kombinasyon literatürde **PFOR-Delta** olarak bilinir (Zukowski ve ark., 2006;
Lemire & Boytsov'un SIMD çalışmaları). ElBâri'nin katkısı algoritmanın kendisi değil,
onu **bağımlılıksız, tahsisatsız, AOT-hazır ve kayıplı-link-dayanıklı** bir .NET
telemetri kodeki olarak paketlemesidir.

## ⚠️ Patent ve IP Notu

Kullanılan teknikler (delta encoding, bit packing, variable bit-width, PFOR patching)
onlarca yıldır halka açık ve yayınlanmıştır; bilinen bir patent ihlali içermez. Yayınlanmış
akademik teknikler prior art oluşturduğu için bu implementasyon patent açısından güvenlidir.

## ⚠️ Sorumluluk Reddi

Bu yazılım **"OLDUĞU GİBİ"** sağlanmaktadır; hiçbir garanti verilmez. Kritik sistemlerde
(İHA, askeri, medikal) kullanmadan önce:

1. Kapsamlı testler yapın
2. Simülasyon ortamında doğrulayın
3. Sertifikasyon gereksinimlerinizi kontrol edin
4. `EMBEDDED_MODE`'u değerlendirin
5. Watchdog timer ile koruyun

---

**© 2025 İmran Kağan. Tüm hakları saklıdır.**
Proprietary and Confidential. Commercial License Required.
