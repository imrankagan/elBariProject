# ElBâri — Telemetri Sıkıştırma Motoru

[![Lisans: Akademik serbest / Ticari lisanslı](https://img.shields.io/badge/lisans-akademik%20serbest%20%7C%20ticari%20lisansl%C4%B1-orange.svg)](LICENSE.txt)
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

**Ölçüm tabanı — hepsi gerçek veri.** OpenStreetMap GPS iz arşivi ve
[ALFA veri setinden](https://theairlab.org/alfa-dataset/) gerçek bir ArduPilot uçuş logu
(yönelim, IMU, GPS, servo, kumanda, titreşim). Canlı telemetri senaryosu ayrıca
[iki kademeli bir MAVLink vekiliyle](belgeler/MAVLINK_VEKIL.md) ölçülür. Sentetik veri
yalnızca kenar durum testlerinde kullanılır.

**Lisans:** Akademik ve eğitim amaçlı kullanım, inceleme ve atıf **serbesttir** — tez
çalışmaları dâhil. Ticari kullanım ayrı bir lisans gerektirir; dağıtım ve yeniden
yayımlama yasaktır. Ayrıntı: [LICENSE.txt](LICENSE.txt) · İletişim: imrankagant@gmail.com

## 📁 Proje Yapısı

```
kaynak/          C# kaynak kodu (5 dosya, üç katman + float)
c/               C sürümü — bağımsız, bağımlılıksız
  src/             kütüphane kaynağı (~2.500 satır)
  test/            doğrulama, ölçüm, fuzz, uygunluk
  kiyas/           tamsayı kodek ailesiyle karşılaştırma (Simple8b, Sprintz, ...)
  mavlink/         iki kademeli MAVLink vekili ve ölçümü
  veri/            ArduPilot DataFlash log okuyucusu → ölçüm fikstürleri
  Makefile         Linux/macOS derleme
  derle.bat        Windows derleme (MSVC)
benchmark/       .NET test ve ölçüm paketi (32 senaryo)
belgeler/        biçim spesifikasyonu (ICD), ölçüm raporları, MISRA uyum matrisi
testverisi/      gerçek GPS verisi + dondurulmuş uygunluk vektörleri
.github/         sürekli tümleştirme iş akışı
```

| Klasör | İçerik |
| --- | --- |
| [kaynak/](kaynak/) | `ElBâri.cs` (çekirdek), `ElBâriKanal.cs`, `ElBâriÇerçeve.cs`, `ElBâriFloat.cs`, `ElBâriFloatXor.cs` |
| [c/](c/) | Aynı üç katmanın C sürümü — RTOS ve bare-metal hedefleri için ([BENIOKU](c/BENIOKU.md)) |
| [benchmark/](benchmark/) | Test senaryoları, veri üreticileri, ölçüm koşucusu |
| [c/kiyas/](c/kiyas/) | ElBâri'yi **kendi ailesiyle** ölçen kıyas takımı ([BENIOKU](c/kiyas/BENIOKU.md)) |
| [c/mavlink/](c/mavlink/) | **İki kademeli MAVLink vekili** — canlı telemetride sıkıştırma ([BENIOKU](c/mavlink/BENIOKU.md)) |
| [c/veri/](c/veri/) | **ArduPilot DataFlash log okuyucusu** — gerçek uçuş logundan ölçüm fikstürü üretir ([BENIOKU](c/veri/BENIOKU.md)) |
| [belgeler/](belgeler/) | [Biçim spesifikasyonu](belgeler/BICIM_SPESIFIKASYONU.md) (ICD), [ölçüm sonuçları](belgeler/OLCUM_SONUCLARI.md), [tamsayı kodek kıyası](belgeler/KIYAS_TAMSAYI_KODEKLER.md), [MAVLink vekili ölçümü](belgeler/MAVLINK_VEKIL.md), [kayıp dayanıklılığı süpürmesi](belgeler/KAYIP_DAYANIKLILIK.md), [MISRA uyum matrisi](belgeler/MISRA_UYUM.md), [akış şemaları](belgeler/AKIS_SEMASI.md) |
| [testverisi/](testverisi/) | `gercek_gps.bin` (24.642 gerçek kayıt), `vektorler.txt` (29 uygunluk vektörü) |

## 🧱 Mimari — Üç Katman

> 📊 Görsel anlatım: **[belgeler/AKIS_SEMASI.md](belgeler/AKIS_SEMASI.md)** — hangi katmanı
> ne zaman kullanacağın, kodlama/çözme akışı, blok yapısı ve paket kaybı senaryosu
> şemalarla anlatılıyor.

ElBâri üç bağımsız katmandan oluşur. Her katman bir öncekinin üzerine oturur; ihtiyacına
göre yalnızca gerekeni kullanırsın.

| Katman | C# | C | Ne işe yarar |
| --- | --- | --- | --- |
| **Çekirdek** | [ElBâri.cs](kaynak/ElB%C3%A2ri.cs) | [elbari.c](c/src/elbari.c) | `ElKâbıd` (kodlayıcı) / `ElBâsıt` (çözücü). Tek bir tamsayı akışını delta + adaptif bit-packing ile sıkıştırır. |
| **Kanal** | [ElBâriKanal.cs](kaynak/ElB%C3%A2riKanal.cs) | [elbari_kanal.c](c/src/elbari_kanal.c) | Çok kanallı telemetriyi (kayıt akışı) kanallara ayırıp her kanalı kendi içinde sıkıştırır. Kanal başına adaptif fark derecesi seçer. |
| **Çerçeve** | [ElBâriÇerçeve.cs](kaynak/ElB%C3%A2ri%C3%87er%C3%A7eve.cs) | [elbari_cerceve.c](c/src/elbari_cerceve.c) | Akışı bağımsız çözülebilir, sıra numaralı, CRC32 korumalı çerçevelere böler. Paket kaybına dayanıklılık buradan gelir. |

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
> ayrımı **ile 5.05x**. Yani birincil hedef veri tipi ancak bu katmanla çalışıyor.
>
> Bu ElBâri'ye özgü bir zayıflık değil: kanal ayrımı olmadan **tamsayı kodek ailesinin
> tamamı** çöküyor (BP128 1.00x, Sprintz 0.97x, Simple8b 0.50x — veriyi ikiye katlıyor).
> Ölçüm: [belgeler/KIYAS_TAMSAYI_KODEKLER.md](belgeler/KIYAS_TAMSAYI_KODEKLER.md).

## ✨ Özellikler

- **Kayıpsız Sıkıştırma** — 29 dondurulmuş uygunluk vektörü, 32 senaryoluk .NET takımı ve
  CI'da her push'ta 300.000 turluk çözücü fuzz'ı + 300.000 turluk kodlayıcı değer fuzz'ı
- **Zero-Allocation** — `Span<T>` tabanlı; çalışma alanı çağıran tarafından verilir
  (ölçüldü: 100 encode+decode turunda **0 bayt** heap tahsisatı)
- **Çok Mimarili SIMD**:
  - ✅ **Intel/AMD**: AVX2 (8×32-bit paralel) — bu makinede ölçüldü
  - 🧩 **ARM**: NEON (4×32-bit paralel) — kod mevcut, gerçek ARM donanımında henüz
    benchmark edilmedi
  - ✅ **Eski işlemciler**: Scalar fallback (her zaman çalışır)
- **Adaptif Bit-Width** — blok başına 8 mod (0/2/3/4/5/8/10/16 bit), aykırı değerler
  için 32 bit
- **Blok-Üstü Sıfır Koşusu** *(biçim sürümü 3)* — ardışık sıfır blokları tek kaçışla
  kodlanır; neredeyse sabit kanallarda oranı kat kat artırır (RCIN 40x → **92x**)
- **Kanal Başına Adaptif Fark Derecesi** — düzgün kanallar (sabit hızlı GPS) ikinci
  derece farkı, gürültülü kanallar birinci dereceyi seçer
- **Paket Kaybı Dayanıklılığı** — bağımsız çerçeveler + CRC32
- **Ham Geçiş Güvenliği** — sıkışmayan kanal ham yazılır; "reddedildi" durumunda veri
  kaybı olmaz
- **Native AOT** — IL yok, JIT ısınması yok, deterministik çalışma süresi
- **Gömülü Sistem Modu** — `EMBEDDED_MODE` ile exception-free çalışma

## 📊 Ölçülen Performans

> 📈 **Tam ölçüm raporu: [belgeler/OLCUM_SONUCLARI.md](belgeler/OLCUM_SONUCLARI.md)** —
> 7 veri seti, üç katman, C# ve C yan yana, gecikme dağılımı, teorik alt sınır analizi.

> **Metodoloji:** Aşağıdaki sayılar **gerçek** veri üzerinde ölçülmüştür — sentetik
> değil. Veri: OpenStreetMap halka açık GPS iz arşivinden 24.642 kayıt (lat/lon/zaman).
> Ortam: .NET 10, 24 çekirdekli x64, AVX2 aktif, Release + Native AOT, tek iş parçacığı.
> Kaynak/lisans: [TestData/KAYNAK.md](testverisi/KAYNAK.md).

### Çekirdek + Kanal Katmanı (tek blok)

| İşlem | Verim | Hız | Oran |
| --- | --- | --- | --- |
| encode | ~96M kayıt/sn | **863 MB/sn** | 5.05x |
| decode | ~127M kayıt/sn | **846 MB/sn** | — |

### Çerçeve Katmanı (100 kayıt/çerçeve, paket kaybına dayanıklı)

| İşlem | Verim | Hız | Çerçeve başına |
| --- | --- | --- | --- |
| encode | ~22M kayıt/sn | 224 MB/sn | 4.5 µs |
| decode | ~53M kayıt/sn | 245 MB/sn | 1.9 µs (CRC dahil) |

Çerçeveleme, dayanıklılık karşılığında oranı (5.05x → 4.33x) ve encode hızını düşürür
(küçük bloklar + kanal başına heuristik + CRC). Buna karşılık kayıplı linkte
çalışabilirlik kazanılır.

> ⚠️ **Ama bu 4.33x rakamı gerçekçi bir çalışma noktası değil.** 100 kayıt/çerçeve,
> 10 Hz telemetride **10 saniyelik tamponlama** demektir ve en büyük çerçeve **572 bayt**
> — tipik SiK radyo yükünün iki katından fazla. Hem tek pakete sığan hem gecikmesi kabul
> edilebilir nokta **25 kayıt/çerçeve** ve orada oran **2.82x**. Yani çerçevelemenin
> gerçek maliyeti %13 değil, **%43**. Süpürme tablosu:
> [belgeler/KIYAS_TAMSAYI_KODEKLER.md §5](belgeler/KIYAS_TAMSAYI_KODEKLER.md).

### Tahsisat

```
100 encode+decode turu, gerçek GPS verisi:
  Tahsis edilen bayt: 0
  ✓ SIFIR tahsisat — heap'e hiç dokunulmadı, GC baskısı yok.
```

### İşlemci payı — hız neden öncelik değil

Yukarıdaki hız rakamları soyut kalabiliyor; gerçek bir kullanım senaryosuna oturtalım.

Zorlayıcı bir telemetri hızı varsayalım: **saniyede 400 kayıt** (tipik İHA telemetrisi
1-50 Hz bandındadır).

```
Çerçeve başına 100 kayıt  →  saniyede 4 çerçeve
Çerçeve başına encode     →  1.8 µs (ölçüldü, C sürümü medyan)
────────────────────────────────────────────────────────
Saniyede harcanan süre    :  4 × 1.8 = 7.2 µs
Bir saniye                :  1.000.000 µs
İşlemci kullanımı         :  ~%0,0007
```

Yani **işlemcinin her saniyesinin 7 mikrosaniyesi** bu işe gidiyor. Verim olarak
bakılırsa ihtiyacın **~138.000 katı** kapasite var.

> **Dürüstlük notu:** Bu ölçüm masaüstü x64 üzerinde yapılmıştır. Gerçek hedef olan
> ARM kartlar (Raspberry Pi, Jetson) daha yavaştır ve **henüz ölçülmedi**. En kötü
> ihtimalle 10 kat yavaş olsa bile ~14.000 kat pay kalır; işlemci kullanımı yine
> %0,01'in altındadır.

**Sonucu şu:** Bu projede tıkanan yer **işlemci değil, telsizin bant genişliğidir.**
Optimizasyon çabası hıza değil, orana ve dayanıklılığa harcanmalıdır. Elle SIMD eklemek
%0,0007'yi %0,0004 yapar — ölçülebilir ama anlamsız bir kazanç.

## ⚖️ Karşılaştırma 1 — Kendi ailesi (tamsayı kodekleri)

> 📈 **Tam rapor: [belgeler/KIYAS_TAMSAYI_KODEKLER.md](belgeler/KIYAS_TAMSAYI_KODEKLER.md)** ·
> ölçüm kodu: [c/kiyas/](c/kiyas/)

ElBâri kendini **PFOR-Delta ailesinin bir uygulaması** olarak tanımlıyor (bkz. *Algoritma
Detayları*). O halde asıl rakip zstd değil, **aynı ailenin diğer üyeleridir.** Aşağıdaki
ölçüm bu boşluğu kapatıyor — ve iddiayı küçültüyor.

> **Metodoloji:** Aynı gerçek GPS verisi, aynı makine, aynı derleyici ve bayraklar
> (`/std:c17 /O2`), 200 tur. **Tüm kodekler skaler C — ElBâri dahil, SIMD yok.**
> Rakiplere kanal ayrımı + fark + zigzag ön işlemesi bedava verildi ve bu süre onların
> encode süresine dahildir. Her kodek tam tur doğrulamasından geçti.

| Kodek | Kaynak | Bayt | Oran | bit/değer | encode | decode |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| **ElBâri (kanal)** | bu çalışma | **58.525** | **5.05x** | **6.34** | 1.176 MB/sn | 1.673 MB/sn |
| Sprintz-Delta | Blalock ve ark. 2018 | 63.321 | 4.67x | 6.85 | 463 MB/sn | 1.045 MB/sn |
| Simple8b | Anh & Moffat 2010 | 63.885 | 4.63x | 6.91 | 364 MB/sn | 1.824 MB/sn |
| OptPFD (PFOR+yama) | Zukowski 2006 / Yan 2009 | 64.807 | 4.56x | 7.01 | 260 MB/sn | 1.103 MB/sn |
| ElBâri (çerçeve, 100) | bu çalışma | 68.282 | 4.33x | 7.45 | 538 MB/sn | 851 MB/sn |
| BP128 | Lemire & Boytsov 2015 | 81.130 | 3.64x | 8.78 | 556 MB/sn | 1.078 MB/sn |
| VByte (LEB128) | varint temel çizgisi | 93.540 | 3.16x | 10.12 | 2.599 MB/sn | 1.612 MB/sn |
| StreamVByte | Lemire & Kurz 2017 | 104.855 | 2.82x | 11.35 | 1.803 MB/sn | 1.671 MB/sn |

![Pareto sınırı — oran, hız](belgeler/pareto_tamsayi_kodekler.svg)

> ### ⚠️ Hız sütununu okumadan önce
>
> Rakip kodekler **yazarlarının kütüphaneleri değildir**; yayınlanmış biçim
> tanımlarından yeniden yazılmış skaler C uygulamalarıdır.
> **Oran taşınabilirdir** (biçimden gelir, uygulamadan değil) — **hız değildir.**
> FastPFor ve StreamVByte'ın SIMD sürümleri buradakinden kat kat hızlıdır; hız sütunu
> rakipler için bir **alt sınırdır.** Bu yüzden burada "ElBâri daha hızlı" iddiası
> **kurulmuyor.**

### Yedi veri setinde konum

Yukarıdaki tablo tek bir veri setine (GPS izleri) aittir. Ölçüm, gerçek bir ArduPilot
uçuş logundan üretilen altı fikstürle tekrarlandı:

| Veri seti | K | **ElBâri** | Ailenin en iyisi | Fark |
| --- | ---: | ---: | --- | ---: |
| GPS (OSM referans) | 3 | **5.05x** | Sprintz 4.67x | **+%8,1** |
| GPS (ALFA uçuş) | 3 | **5.60x** | Simple8b 5.43x | **+%3,1** |
| Titreşim | 3 | 5.38x | Simple8b 5.51x | −%2,4 |
| Yönelim | 3 | **15.57x** | Sprintz 15.55x | **+%0,1** |
| IMU | 6 | 6.88x | Simple8b 7.43x | −%7,4 |
| Servo (RCOU) | 8 | **37.50x** | Sprintz 34.18x | **+%9,7** |
| Kumanda (RCIN) | 8 | **92.26x** | Sprintz 74.39x | **+%24,0** |

**Yedi veri setinin beşinde lider; IMU ve titreşimde %2–7 geride.**

Tekrarlı PWM kanallarındaki eski açık (RCIN 40.09x, Sprintz 74.39x) bir ayar meselesi
değildi: sürüm 2'de ElBâri her 8 değere 4 bitlik etiket yazıyordu, sıfır blokta bile —
yani değer başına 0,5 bitlik taban ve **64x'lik sert bir tavan**. **Biçim sürümü 3**
ardışık sıfır bloklarını tek bir kaçışla kodlayarak bu tavanı kaldırdı; RCIN 40.09x →
**92.26x** oldu ve encode hızı da 2.197 → 4.037 MB/sn'ye çıktı. Ayrıntı:
[belgeler/KIYAS_TAMSAYI_KODEKLER.md §3](belgeler/KIYAS_TAMSAYI_KODEKLER.md) ve
[biçim spesifikasyonu §2.2b](belgeler/BICIM_SPESIFIKASYONU.md).

### Bu ölçümün üç bulgusu

**1. Katkı gerçek ama ölçülü ve veri setine bağlı.**
GPS verisinde ElBâri'nin oran üstünlüğü Sprintz'e karşı **%8,2**, Simple8b'ye karşı
%9,2, OptPFD'ye karşı %10,7. Genel amaçlı sıkıştırıcılara karşı görülen "üç kat" farkın
gerçek ailedeki karşılığı budur. Ama yukarıdaki tabloda görüldüğü gibi bu fark **her
veri setinde aynı değil** — yönelimde kıl payı, tekrarlı kanallarda belirgin, IMU ve
titreşimde ise negatif. Beklenen sonuç: ElBâri zaten aynı fikirleri kullanıyor.

**2. Çerçeveleme bedelini herkes ödediğinde sıralama çerçeve boyutuna bağlı.**
Rakiplerde çerçeveleme yok, dolayısıyla adil kıyas için aynı yükü onlara da vermek
gerekir. Verildiğinde **100 kayıt/çerçevede ElBâri lider** (GPS 4.33x vs Sprintz 3.86x,
yönelim 10.75x vs 8.65x), ama **25 kayıt/çerçevede Sprintz öne geçiyor** (3.32x vs
2.82x). Kırılma noktası 50–100 arası. Sebep ElBâri'nin çerçeve başına sabit maliyeti:
8 kanalda ~84 bayt. Bu, kayıp süpürmesiyle birlikte okunmalı — yüksek kayıpta optimum
çerçeve ~1 pakete iner, yani **en çok ihtiyaç duyulan rejim en zayıf olduğumuz rejim.**

**3. Kanal ayrımı argümanı saman adam değilmiş.**
Kanal ayrımı olmadan ailenin **tamamı** çöküyor: BP128 1.00x, Sprintz 0.97x,
VByte 0.86x, Simple8b **0.50x** (veriyi ikiye katlıyor). Bu, ElBâri'ye özgü bir
zayıflık değil, problem sınıfının zorunlu ön koşulu.

## ⚖️ Karşılaştırma 2 — Genel amaçlı sıkıştırıcılar (Zstd / LZ4 / Brotli / Deflate)

> Bunlar **doğru rakip ailesi değildir** (bkz. Karşılaştırma 1). Buraya, telemetriye
> genel amaçlı bir sıkıştırıcı yapıştırmanın yaygın bir refleks olması nedeniyle
> konulmuştur: o refleksin ne kadar pahalı olduğunu gösterir.

> **Metodoloji:** Aynı makinede, aynı gerçek GPS verisiyle (295.704 B ham), 20 tur ısınma
> + 200 tur ölçüm. Rakipler resmî .NET paketleriyle çalıştırıldı:
> `ZstdSharp.Port`, `K4os.Compression.LZ4`, ve .NET yerleşik `BrotliStream` /
> `DeflateStream` / `GZipStream`. Tüm yöntemlerde round-trip doğrulandı (kayıpsız).
> Hız değerleri **ham veri üzerinden** MB/sn'dir.

### Ham baytlar (telemetriyi olduğu gibi sıkıştırıcıya vermek — yaygın "naif" entegrasyon)

| Yöntem | Boyut | Oran | Encode | Decode |
| --- | ---: | ---: | ---: | ---: |
| **ElBâri — kanal ayrımı** | **58.525 B** | **5.05x** | **863 MB/sn** | **846 MB/sn** |
| **ElBâri — çerçeveli (100)** | 68.282 B | 4.33x | 224 MB/sn | 245 MB/sn |
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
gösteriyor: Zstd-1 yalnızca **1.61x**, LZ4 **1.29x** veriyor. ElBâri **5.05x** ile bunların
**üç katından fazla** sıkıştırıyor ve aynı zamanda daha hızlı. Sebep basit — genel
sıkıştırıcılar veriyi anlamsız bir bayt yığını olarak görür; kanalların iç içe geçmesi
onların örüntü aramasını köreltir. ElBâri verinin **kayıt yapısını bilir**.

**2. Hız/oran ödünleşiminde boş bir köşe dolduruluyor.**
Rakipler iki uçtan birinde: ya hızlı ama zayıf oran (LZ4 1.29x, Zstd-1 1.61x), ya iyi oran
ama çok yavaş (Brotli-q11 3.59x @ 1 MB/sn, Zstd-19 3.03x @ 8 MB/sn). **Hem 3x üzeri oran
hem 800+ MB/sn hızı** aynı anda veren tek yöntem ElBâri'dir.

**3. Biçim sürümü 2, bu tablodaki oran liderliğini de aldı.**
Sürüm 1'de Brotli-q11 kanal-ayrılmış veride **3.88x** ile ElBâri'yi (3.56x) geçiyordu.
Sürüm 2'deki bit genişliği tablosu genişletmesi bu değeri aştı; sürüm 3'teki sıfır
koşusuyla birlikte ElBâri **5.05x**'te — üstelik Brotli'den **~800 kat hızlı** encode
ederek. Bu liderlik **yalnızca
genel amaçlı sıkıştırıcılar arasında** geçerlidir; doğru aile için Karşılaştırma 1.

Yani **bu tablodaki genel amaçlı sıkıştırıcılar arasında** ElBâri hem en yüksek orana
hem de (LZ4 dışında) en yüksek hıza sahip. LZ4 açmada daha hızlı ama oranı 1.29x — üç
buçuk kat geride.

> ⚠️ **Bu üstünlük yalnızca bu tablo için geçerlidir.** Doğru rakip ailesiyle
> (Simple8b, OptPFD, Sprintz) ölçüldüğünde fark daralıyor, veri setine göre işaret bile
> değiştiriyor (konum +%3…+%6, yönelim/IMU −%2…−%7, tekrarlı PWM −%25…−%46) ve
> çerçeveleme açıkken sıralama çerçeve boyutuna bağlı hâle geliyor — bkz.
> [belgeler/KIYAS_TAMSAYI_KODEKLER.md](belgeler/KIYAS_TAMSAYI_KODEKLER.md).

**4. "Ön işlemeyi biz de yaparız" itirazı ölçüldü.**
Kuantalama ve kanal ayrımı, herkesin yapabileceği ön işlemlerdir. Bu yüzden ikisi de
rakiplere **bedava verildi** ve aynı veriyle ölçüldü:

| Yöntem | Oran | Encode |
| --- | ---: | ---: |
| **ElBâri (kuantalanmış float)** | **10.51x** | **981 MB/sn** |
| Brotli q11 + kanal ayrımı | 7.84x | 1 MB/sn |
| Zstd sev.19 + kanal ayrımı | 7.34x | 4 MB/sn |
| Zstd sev.1 + kanal ayrımı | 4.80x | 392 MB/sn |

Her iki ön işlem de verildikten sonra bile ElBâri **%34 önde** ve **~1.000 kat hızlı**.
Ayrıntı: [ölçüm raporu §8b](belgeler/OLCUM_SONUCLARI.md).

**5. Tabloda görünmeyen farklar.**
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

### Çerçeve boyutu kaç olmalı? — süpürüldü

Yukarıdaki tablo **tek bir işletim noktasıdır** (100 kayıt/çerçeve). Çerçeve boyutu ×
kayıp oranı süpürmesi ayrı bir raporda:
**[belgeler/KAYIP_DAYANIKLILIK.md](belgeler/KAYIP_DAYANIKLILIK.md)**.

Optimum, oranın ve kurtarmanın **çarpımındadır** — gönderilen her bayta karşılık alıcıya
ulaşan ham veri:

| Kayıp | Yönelim verisinde optimum | Etkin oran |
| ---: | ---: | ---: |
| %1 | 1000 kayıt/çerçeve | 16,09x |
| %5 | 500 | 14,14x |
| %10 | 500 | 12,65x |
| %25 | 200 | 9,05x |

Üç bulgu:

1. **Optimum, kayıp yükseldikçe küçülür.** Büyük çerçeve MTU'da parçalanır ve ancak tüm
   parçaları ulaşırsa çözülür; hayatta kalma olasılığı paket sayısıyla üstel düşer.
2. **Patlamalı kayıp bağımsız kayıptan iyidir.** %25 kayıpta 500 kayıt/çerçeve:
   bağımsız 8,65x, 10 paketlik patlamalarda **11,40x**. Patlamalar hasarı az sayıda
   çerçevede yoğunlaştırır; çerçeveler zaten bağımsız olduğu için toplam hasar azalır.
3. **Çerçeve kayıtla değil, baytla ölçülmeli.** Üç veri setinde optimum kayıt sayısı
   50-1000 arasında savruluyor, ama optimum **paket/çerçeve** oranı %10 kayıpta
   1,8-2,3 bandında sabit kalıyor. Sabit kayıt sayısı yanlış değişken.

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
3. **Denetim** — müşterinin güvenlik ekibi ~2.500 satırlık C kütüphanesini satır satır
   okuyabilir; içinde runtime gömülü birkaç MB'lık bir ikiliyi okuyamaz. Savunmada bu
   belirleyicidir.

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
  [GEÇTİ] C çıktısı == .NET çıktısı            59695 bayt BİREBİR AYNI
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
| MISRA C:2012 uyum incelemesi | ✅ Elle yapıldı, belgelendi ([MISRA_UYUM.md](belgeler/MISRA_UYUM.md)) |
| MISRA C:2012 **araç taraması** | ✅ Cppcheck MISRA eklentisi, **iki sürümde** (2.21.0 + 2.13.0) — kayıtlı sapmalar dışında 0 bulgu, CI'da her push'ta |
| MSVC `/Wall /analyze` statik analiz | ✅ 0 bulgu |
| Sağlamlık (fuzz) testi | ✅ CI'da 300.000 tur + sanitizer'lı 50.000 tur, 0 tampon taşması |
| Sertifikalı MISRA aracıyla doğrulama | ⏳ Yapılmadı — müşteri/program gerektirdiğinde |
| GCC / Clang derleme | ✅ CI'da her push'ta (Linux) |
| ASan + UBSan (çalışma zamanı) | ✅ CI'da temiz |
| ARM / big-endian üzerinde doğrulama | ⏳ Donanım yok — kod derlenir, hız ölçülmedi. [Ayrıntı](belgeler/OLCUM_SONUCLARI.md#-gerçek-arm-donanımında-ölçüm) |
| Elle yazılmış SIMD | ❌ **Yapılmayacak** — ölçüm kararı: C zaten C#'ın SIMD'li sürümünden hızlı, işlemci payı %0,0006. [Gerekçe](belgeler/OLCUM_SONUCLARI.md#-elle-yazılmış-simd-c-sürümü) |

### C mi hızlı, C# mı? — ölçüldü

Sezgiye aykırı bir sonuç: **saf skaler C, SIMD'li C#'tan hızlı çıktı.**

| İşlem | C (skaler) | C# (AVX2) | Fark |
| --- | ---: | ---: | --- |
| encode | **1.102 MB/sn** | 863 MB/sn | C %28 hızlı |
| decode | **1.450 MB/sn** | 846 MB/sn | C %71 hızlı |

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

Kod baştan MISRA disipliniyle yazıldı, kural kural elle denetlendi, ardından bir **araçla
taranıp** kalan bulgular ya düzeltildi ya da gerekçesiyle kayda geçirildi. Tam matris ve
sapma kaydı: **[MISRA_UYUM.md](belgeler/MISRA_UYUM.md)**

| Kategori | Durum |
| --- | --- |
| Zorunlu (Mandatory) kurallar | İhlal yok |
| Gerekli (Required) kurallar | Yalnızca 1 kayıtlı sapma (Kural 21.15, `memcpy` tip yorumlaması) |
| Tavsiye (Advisory) kurallar | Yalnızca 1 kayıtlı sapma (Kural 15.5, tek çıkış noktası) |
| Cppcheck MISRA eklentisi (2.21.0 + 2.13.0) | ✅ Kayıtlı sapmalar dışında 0 bulgu |
| Cppcheck `--enable=all` | ✅ 0 hata, 0 uyarı |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| MSVC `/W4` | ✅ 0 uyarı |

Araç taraması **süs değil, iş gördü** — altı gerçek bulgu düzeltildi:

| Kural | Sorun | Düzeltme |
| --- | --- | --- |
| 10.6 | `koşul ? 1 : 0` bileşik ifadedir; `int32_t`'ye atanması genişletmedir (5 yer, **Gerekli**) | Açık `if/else`. Cast ile susturmak Kural 10.8'i ihlal ederdi — döngüsel |
| 10.1 | Kanal bayraklarında işaretli operandla bit işlemi (5 yerde tekrarlanan deyim) | Ortak yardımcı: `elbari_ic_bayrak_kur` / `elbari_ic_bayrak_var_mi` |
| 12.2 | Kaydırma miktarının sınırlı olduğu elle ispatlanabiliyordu ama araç göremiyordu | **Kaydırma tamamen kaldırıldı** — 33 girişlik maske tablosu, indeks açıkça 0..32'ye kelepçeli |
| 10.4 | `sizeof(...) == 4` — işaretsizle işaretliyi karşılaştırma | `== 4u` |
| 17.8 | `deger >>= 1` parametreyi değiştiriyordu | Yerel kopya |
| 2.5 / 8.9 | Kullanılmayan makrolar, gereksiz dosya kapsamı | Temizlendi |

Bunların **hiçbiri bit akışını değiştirmedi**: her adımda uygunluk vektörleri ve .NET ile
58.525 baytlık birebir karşılaştırma tekrar çalıştırılarak doğrulandı.

> 12.2 düzeltmesi öğreticidir: "araç anlamıyor, biz biliyoruz" demek yerine kaydırma
> işlemini koddan çıkardık. Sonuç hem araç için hem insan için ispatlanabilir oldu,
> maliyeti ise 132 baytlık salt-okunur tablo.

**Tarama iki farklı araç sürümünde yürütülür** ve bunun somut bir sebebi var:

| Ortam | Cppcheck | Kural 10.6'yı yakaladı mı? |
| --- | --- | --- |
| Yerel | 2.21.0 | ❌ Hayır |
| CI | 2.13.0 | ✅ Evet, 5 adet |

Daha *eski* sürüm, daha yenisinin kaçırdığı bir **Gerekli** kuralı buldu. Platform farkı
değil, sürüm farkı — ve yönü sezgiye aykırı. Buradan çıkan sonuç: *tek bir araç
sürümünden "temiz" almak zayıf bir kanıttır.* CI'daki `misra` işi yalnızca rapor
üretmez; kayıtlı sapmalar dışında bir kural görürse **derlemeyi kırar**, böylece
belgelenmemiş yeni bir ihlal sessizce içeri giremez.

Öne çıkan noktalar:

- **Özyineleme yok** (Kural 17.2) — yığın derinliği sabit ve öngörülebilir
- **Dinamik bellek yok** (Dir 4.12) — tüm tamponları çağıran verir
- **Tanımsız davranış yok** (Kural 1.3) — C'de işaretli taşma tanımsızdır; tüm fark ve
  toplama işlemleri işaretsiz aritmetik üzerinden yapılır. Bu hem C'de tanımlıdır hem de
  .NET'in `unchecked` davranışıyla birebir aynı sonucu verir — ikili uyumluluğun temeli
- **Kaydırma sınırları kanıtlandı** (Kural 12.2) — en kötü durum 39 bit < 64; maske üretiminde kaydırma hiç kullanılmaz
- **Dış girdi doğrulanır** (Dir 4.14) — çerçeve başlığı, CRC, kanal sayısı, yük boyutları

Kayıtlı sapmalar (dördü de gerekçesiyle [MISRA_UYUM.md](belgeler/MISRA_UYUM.md)'de):

| # | Kural | Konu | Özet gerekçe |
| --- | --- | --- | --- |
| D-1 | 15.5 (Tavsiye) | Tek çıkış noktası | Koruma cümlesi tercihi; dinamik bellek olmadığı için "çıkışta temizlik atlanır" riski yapısal olarak yok |
| D-2 | — | Kayan nokta kullanımı | .NET ile bit-bit aynı kararı üretmek için zorunlu; yalnızca karar verir, üretilen bit akışına girmez |
| D-3 | 9.1 | `gecici[]` ilklenmiyor | Her eleman okunmadan önce mutlaka yazılır (aykırı maskesi iki kümeyi ayrık kılar) |
| D-4 | 21.15 (Gerekli) | `memcpy` ile float↔uint32 | **Kasıtlı tip yorumlaması** — XOR sıkıştırmasının çalışma prensibi. Alternatifleri (işaretçi dönüşümü, `union`) tanımsız davranış üretir; `memcpy` standardın izin verdiği tek taşınabilir yol |

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

> **Dürüstlük notu — burada tam olarak neredeyiz.** Kod bir **açık kaynak** MISRA
> denetleyicisinden (Cppcheck) kayıtlı sapmalar dışında temiz geçiyor ve bu her push'ta
> CI'da tekrarlanıyor. Bu, **sertifikalı** bir araçla (Helix QAC, PC-lint Plus,
> Polyspace) yapılmış bir doğrulama **değildir**.
>
> Şunu da netleştirelim: **"MISRA sertifikası" diye bir belge yoktur.** MISRA kod
> sertifikalandırmaz; uyum sizin beyanınızdır ve kanıtla desteklenir — uyum matrisi,
> sapma kaydı ve araç raporu. Elimizde üçü de var. Müşteri nitelikli bir araç raporu
> talep ederse o adım ayrıca yapılır; kod bunun için hazırdır.

### Sağlamlık (fuzz) testi — düşmanca girdiye dayanıklılık

Telemetri çözücüsü kayıplı ve **düşmanca** bir telsiz ortamından veri alır. Saldırgan,
özel hazırlanmış bir paketle alıcı sistemi çökertmeye çalışabilir. Bu yüzden çözücünün
**hiçbir girdide** çökmemesi, taşmaması ve kilitlenmemesi bir güvenlik gereksinimidir.

**Yöntem:** Çıktı tamponlarının önüne ve arkasına bilinen bir desen ("kanarya") yazılır;
çağrı sonrası desen bozulmuşsa kütüphane tampon dışına yazmış demektir. Bu, çökmeye yol
açmayan **sessiz taşmaları** da yakalar. Üreteç deterministiktir — bulunan her hata
birebir yeniden üretilebilir.

**Sonuç (300.000 tur — CI'nın koştuğu sayı):**

| Ölçüt | Sonuç |
| --- | --- |
| **Tampon taşması** | **0** |
| Süreç çökmesi | Yok |
| Bozulmuş çerçevelerin reddi | **%100.00** (59.792 / 59.792) |

Katman kırılımı:

| Katman | Kabul | Red | Bütünlük kontrolü |
| --- | ---: | ---: | --- |
| Çekirdek | **6** | 60.701 | Yapısal tüketim kontrolü |
| Kanal | **0** | 119.561 | Başlık tutarlılık kontrolü |
| Çerçeve (bozulmuş) | **0** | 59.792 | **CRC32** |
| Float XOR | **5.847** | 54.093 | Sağlama toplamı yok — bkz. aşağıdaki not |

> **Float XOR'un kabul oranı neden yüksek?** O katmanda bütünlük kontrolü yoktur ve bit
> deseni akışında hemen her bayt dizisi geçerli bir float dizisi olarak çözülebilir.
> Tasarım gereğidir: bütünlük garantisi isteyen çerçeve katmanını kullanmalıdır.

> **Yapısal tüketim kontrolü — sağlama toplamı olmadan çöpü elemek.**
> Geçerli bir sıkıştırılmış akış girdinin **tamamını** tüketir: kodlayıcı tam olarak
> gerektiği kadar bayt yazar, çözücü de tam olarak o kadarını okur. Geriye artık
> kalmışsa girdi bu kodlayıcıdan çıkmamıştır. Maliyeti **tek bir karşılaştırmadır**.
>
> Etkisi ölçüldü (400.000 turluk ayrı bir koşuda): çekirdeğin kabul ettiği çöp girdi
> **88.963 → 17** (%99,98 azalma).
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

**Kuantalama katmanı** ([ElBâriFloat.cs](kaynak/ElB%C3%A2riFloat.cs) / [elbari_float.c](c/src/elbari_float.c))
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
| **Kuantalama + kanal katmanı** | **27.403 B** | **10.51x** |
| Float bit desenini doğrudan vermek | 195.039 B | 1.48x |

> Kuantalama, float bit desenini doğrudan sıkıştırmaktan **6.9 kat** daha iyi. Sebebi:
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

Tam değerin korunması şartsa: [ElBâriFloatXor.cs](kaynak/ElB%C3%A2riFloatXor.cs) /
[elbari_float_xor.c](c/src/elbari_float_xor.c)

Ardışık float'ların **bit desenleri XOR'lanır**. Birbirine yakın değerlerde işaret, üstel
kısım ve mantisin üst bitleri aynıdır; XOR sonucunun başında ve sonunda çok sayıda sıfır
bulunur ve yalnızca ortadaki anlamlı bitler yazılır. Değer hiç değişmemişse **tek bit**
yeter. Literatürde Gorilla (Facebook, 2015) / Chimp olarak bilinir.

#### ⚠️ Ölçüldü: kayıpsız float çoğu telemetride az kazandırır

| Veri tipi | Kayıpsız (XOR) | Kayıplı (kuantalama) |
| --- | ---: | ---: |
| Gürültülü uçuş verisi (gerçekçi) | **1.21x** | **10.51x** |
| Durağan veri (çok tekrar eden) | **15.08x** | 10.86x |
| Düzgün sinyal (gürültüsüz) | **1.00x** | 15.83x |

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
**[BICIM_SPESIFIKASYONU.md](belgeler/BICIM_SPESIFIKASYONU.md)** (Arayüz Kontrol Dokümanı / ICD)

Belge, bağımsız bir tarafın sıfırdan uyumlu bir kodlayıcı/çözücü yazabilmesi için
gereken her şeyi içerir: üç katmanın bayt düzeni, blok/etiket kodlaması, bit genişliği
seçim kuralları, aykırı değer mekanizması, ikinci derece fark, ham geçiş, CRC kapsamı
ve doğrulama sırası.

### Dondurulmuş uygunluk vektörleri

[`testverisi/vektorler.txt`](testverisi/vektorler.txt) — 29 referans vektör. Bit genişliği
tablosunu, aykırı değerleri, kısmi blokları, ikinci derece farkı, ham geçişi, çerçeve
başlığını, float kuantalamasını ve XOR katmanını kapsar.

Bir implementasyon uyumlu sayılır **ancak ve ancak**:

1. Her vektörün girdisinden **birebir aynı bayt dizisini** üretiyorsa, **ve**
2. Her vektörün çıktısından **birebir aynı girdiyi** geri kurabiliyorsa.

| Implementasyon | Uygunluk |
| --- | --- |
| C# (.NET 10) | ✅ Referans — vektörler bundan üretildi |
| C (C99/C17) | ✅ **29 vektör, 58 kontrol, 0 hata** |

```bash
c\derle.bat
uygunluk.exe ../testverisi/vektorler.txt
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
| GERÇEK GPS — kanal ayrımı | **5.05x** |
| GERÇEK GPS — çerçeveli (100) | **4.33x** (CRC dahil) |
| GERÇEK GPS — çerçeveli (500) | **4.83x** |
| İHA 6 kanal — kanal ayrımsız | ⊘ REDDEDİLDİ (beklenen) |
| İHA 6 kanal — kanal ayrımı | **7.09x** |
| İHA 6 kanal — çerçeveli (250) | **6.54x** |

### Kayıpsızlık doğrulaması

.NET tarafında her iki katman için **1.617 test** (14 hedefli senaryo + 1.600 tur rastgele
fuzz) çalıştırıldı; **tamamı kayıpsız**. Çerçeveler ters sırada ve birbirinden bağımsız
çözülebiliyor. Kenar durumlar dahil: tek eleman, eksik kayıt, `K=255`,
`int.MinValue/MaxValue`, tümü sıfır, saf rastgele veri. C tarafındaki fuzz ayrıdır ve
CI'da 300.000 tur koşar.

> ⚠️ **Bilinen boşluk.** Fuzz turlarının hepsi **çözücü** sağlamlığını sınar: bozuk girdiyi
> çözücüye verir. Rastgele *değerleri* kodlayıp geri okuyan bir tur yoktur. Tam olarak
> 2³¹'lik farkta oluşan kayıpsızlık hatası bu yüzden ne uygunluk vektörlerine ne fuzz'a
> takıldı — ancak gerçek uçuş verisiyle yakalandı. Ayrıntı:
> [belgeler/MAVLINK_VEKIL.md §5](belgeler/MAVLINK_VEKIL.md).

## 🔐 Kod Koruma

ElBâri'nin koruması **Native AOT temellidir** — abartılı runtime hileleri değil.

| Katman | Teknoloji | Etki |
| --- | --- | --- |
| **Native AOT** | `PublishAot=true` | IL bytecode yok; ILSpy/dnSpy/dotPeek gibi .NET decompiler'lar işe yaramaz (C++ assembly zorluğu) |
| **No Debug Symbols** | `DebugType=none` (Release) | PDB yok, sembol bilgisi yok |
| **Reflection Disabled** | `IlcDisableReflection=true` | Runtime type inspection kapalı |
| **Full Trimming** | IL trimmer | Kullanılmayan metadata çıkarılır |

> **Not:** Runtime "Anti-Tamper / Anti-Debug" katmanı kaldırılmıştır; `Debugger.IsAttached`
> / assembly-hash kontrolü gibi teknikler Native AOT bağlamında güvenilir koruma sağlamıyor.
> Gerçek koruma Native AOT + trimming + sembolsüz derlemedir.
>
> Bu bölüm **yalnızca C# sürümünü** ilgilendirir. C sürümü kaynak olarak dağıtılır ve
> okunabilir olması **amaçlanmıştır** — denetlenebilirlik onun varlık sebebidir
> (bkz. *C Sürümü → Neden var?*).

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

**Bu katkı ölçüldü.** Aynı ailenin üyeleriyle (Simple8b, BP128, OptPFD, Sprintz-Delta)
yedi veri setinde yan yana koyulduğunda ElBâri **beş sette önde** (konum +%3…+%8,
yönelim +%0,1, tekrarlı PWM +%10…+%24), **iki sette geride** (IMU ve titreşim, %2-7);
çerçeveleme herkese eşit verildiğinde 100 kayıt/çerçevede lider, 25'te Sprintz'in
gerisinde kalıyor. Ayrıntı ve dürüst sınırlar:
**[belgeler/KIYAS_TAMSAYI_KODEKLER.md](belgeler/KIYAS_TAMSAYI_KODEKLER.md)**.

## ⚠️ Patent ve IP Notu

Kullanılan teknikler (delta encoding, bit packing, variable bit-width, PFOR patching)
onlarca yıldır halka açık ve yayınlanmıştır ve literatürde prior art oluşturur.

Bu bir **hukukî görüş değildir** ve patent taraması yapılmamıştır. Ticari dağıtım öncesinde
yetkin bir tarafça inceleme yapılması gerekir.

## ⚠️ Sorumluluk Reddi

Bu yazılım **"OLDUĞU GİBİ"** sağlanmaktadır; hiçbir garanti verilmez. Kritik sistemlerde
(İHA, askeri, medikal) kullanmadan önce:

1. Kapsamlı testler yapın
2. Simülasyon ortamında doğrulayın
3. Sertifikasyon gereksinimlerinizi kontrol edin
4. `EMBEDDED_MODE`'u değerlendirin
5. Watchdog timer ile koruyun

---

**© 2025-2026 İmran Kağan. Tüm hakları saklıdır.**
Akademik kullanım serbest · Ticari kullanım lisansa tabidir → [LICENSE.txt](LICENSE.txt)
