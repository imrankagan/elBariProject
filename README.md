# ElBâri — Telemetri Sıkıştırma Motoru

[![License: Proprietary](https://img.shields.io/badge/license-Proprietary-red.svg)](LICENSE.txt)
[![.NET 10](https://img.shields.io/badge/.NET-10-purple.svg)](https://dotnet.microsoft.com/)
[![AOT Ready](https://img.shields.io/badge/AOT-Native-green.svg)](https://learn.microsoft.com/dotnet/core/deploying/native-aot)

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
3. **Bağımlılıksız + Native AOT** — harici kütüphane yok, IL yerine doğrudan makine kodu.

Kodun görüntülenmesi, değiştirilmesi veya kullanılması için geçerli bir lisans gereklidir.
GitHub'daki görünürlük **sadece tanıtım amaçlıdır**.

## 🧱 Mimari — Üç Katman

ElBâri üç bağımsız katmandan oluşur. Her katman bir öncekinin üzerine oturur; ihtiyacına
göre yalnızca gerekeni kullanırsın.

| Katman | Dosya | Ne işe yarar |
| --- | --- | --- |
| **Çekirdek** | [ElBâri.cs](ElB%C3%A2ri.cs) | `ElKâbıd` (kodlayıcı) / `ElBâsıt` (çözücü). Tek bir tamsayı akışını delta + adaptif bit-packing ile sıkıştırır. SIMD hızlandırmalı. |
| **Kanal** | [ElBâriKanal.cs](ElB%C3%A2riKanal.cs) | Çok kanallı telemetriyi (kayıt akışı) kanallara ayırıp her kanalı kendi içinde sıkıştırır. Kanal başına adaptif fark derecesi seçer. |
| **Çerçeve** | [ElBâriÇerçeve.cs](ElB%C3%A2ri%C3%87er%C3%A7eve.cs) | Akışı bağımsız çözülebilir, sıra numaralı, CRC32 korumalı çerçevelere böler. Paket kaybına dayanıklılık buradan gelir. |

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

### Konumlandırma (dürüst karşılaştırma)

zstd / LZ4 / Snappy ile **birebir head-to-head benchmark yapılmamıştır**; bu yüzden
buraya uydurma bir karşılaştırma tablosu koymuyoruz. Genel beklenti:

- **Ham sıkıştırma oranı**: zstd muhtemelen önde. ElBâri'nin hedefi bu değil.
- **Determinizm + sıfır tahsisat + kayıplı-link dayanıklılığı**: ElBâri'nin öne çıktığı
  alan. Genel amaçlı sıkıştırıcılar TCP benzeri kayıpsız taşıma varsayar ve paket
  kaybını çözmez.

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
| **Obfuscation** | Obfuscar (opsiyonel) | Kuruluysa string/kontrol akışı karıştırma |

> **Not:** Önceki sürümlerde bulunan runtime "Anti-Tamper / Anti-Debug" katmanı
> kaldırılmıştır. `Debugger.IsAttached` ve assembly-hash kontrolü gibi teknikler
> .NET 6+ / Native AOT bağlamında güvenilir bir koruma sağlamıyordu ve bakım yükü
> getiriyordu. Gerçek koruma Native AOT + trimming + sembolsüz derlemedir.

Obfuscar kuruluysa Release build otomatik çalıştırır:

```bash
dotnet tool install --global Obfuscar.GlobalTool
dotnet build -c Release
```

## 🚁 İHA ve Gömülü Sistem Uyumluluğu

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
