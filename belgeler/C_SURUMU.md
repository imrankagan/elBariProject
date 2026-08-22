# C Sürümü — Gömülü ve Savunma Hedefleri

> Kaynak: [`c/`](../c/) · Klasör notları: [`c/BENIOKU.md`](../c/BENIOKU.md) ·
> API örnekleri: [KULLANIM.md](KULLANIM.md) · MISRA matrisi: [MISRA_UYUM.md](MISRA_UYUM.md)

---

## 1. Neden ikinci bir implementasyon?

Savunma ve gömülü sistemlerde uçan yazılım neredeyse tamamen C'dir. Üç sebep var ve
üçü de .NET sürümünü dışarıda bırakır:

**Hedef donanım.** RTOS'lar (VxWorks, PikeOS, NuttX) ve bare-metal MCU'lar .NET
çalıştırmaz. Ayrıca bir AOT ikilisi tipik olarak birkaç MB'dır; Pixhawk sınıfı bir
kartın *toplam* flash'ı ~2 MB'dır — hedef desteklense bile sığmazdı.

**Sertifikasyon.** DO-178C gibi havacılık standartları için C'nin olgun araç zinciri
(nitelikli derleyici, statik analiz) vardır; .NET için pratikte yoktur.

**Denetim.** Müşterinin güvenlik ekibi ~2.500 satırlık bir C kütüphanesini satır satır
okuyabilir; içinde runtime gömülü birkaç MB'lık bir ikiliyi okuyamaz. Savunmada bu
belirleyicidir.

Ek olarak C'nin **kararlı ABI**'si sayesinde kütüphaneyi her dil bağlayabilir
(C#, Python, Rust, MATLAB, C++).

### Nerede çalışır?

| Donanım | Örnek | C# / AOT | C |
| --- | --- | :---: | :---: |
| Yer istasyonu / sunucu | Masaüstü, kenar sunucusu | ✅ | ✅ |
| Yardımcı bilgisayar (Linux) | Raspberry Pi, Jetson, x86 SBC | ✅ | ✅ |
| Görev bilgisayarı (RTOS) | VxWorks, PikeOS, INTEGRITY | ❌ | ✅ |
| Uçuş kartı (bare-metal) | Pixhawk, STM32, Cortex-M | ❌ | ✅ |
| DSP / özel donanım | TI, RISC-V | ❌ | ✅ |

---

## 2. Tasarım kuralları

Kod baştan MISRA C disipliniyle yazıldı. Her kural bir sebeple var:

| Kural | Neden |
| --- | --- |
| **Kaynak C99 uyumlu, C17 ile derlenir** | C11 özellikleri yalnızca `#if` koruması arkasında; eski/sertifikalı araç zincirleri de derleyebilsin |
| **Dinamik bellek yok** | Gerçek-zaman uyumu; tahsisat gecikmesi ve parçalanma yok. Tüm tamponları çağıran verir |
| **Özyineleme yok** | Yığın derinliği sabit ve öngörülebilir — WCET analizinin ön koşulu |
| **Tüm döngüler sınırlı** | Sonsuz döngü oluşamaz |
| **İstisna yok** | Hatalar dönüş koduyla bildirilir |
| **Harici bağımlılık yok** | Yalnızca `<stdint.h>` ve `<string.h>` |
| **İşaretli taşma yok** | C'de işaretli taşma **tanımsız davranıştır**. Tüm fark hesapları işaretsiz aritmetik üzerinden yapılır — hem C'de tanımlıdır hem de .NET'in `unchecked` davranışıyla birebir aynı sonucu verir |
| **Bayt düzeni açık** | Little-endian elle yazılır/okunur; big-endian işlemcide de aynı biçim üretilir |

Son iki madde **ikili uyumluluğun temelidir.** İki dilin taşma ve bayt düzeni
davranışını şansa bırakmak, iki implementasyonun aynı baytı üretmesini imkânsız kılardı.

---

## 3. İkili uyumluluk — iddia değil, ölçüm

C ve C# sürümleri **aynı bit dizisini** üretmelidir. Gerçek GPS verisiyle (24.642 kayıt)
doğrulanır:

```
--- Kanal katmanı ---
  [GEÇTİ] C çıktısı == .NET çıktısı            BİREBİR AYNI
  [GEÇTİ] C round-trip kayıpsız                tüm elemanlar birebir geri geldi
  [GEÇTİ] C, .NET çıktısını çözebiliyor        çapraz uyumluluk doğrulandı

--- Çerçeve katmanı ---
  [GEÇTİ] yaz/oku bağımsız ve kayıpsız
  [GEÇTİ] tek-bit bozulma CRC ile yakalandı    247/247

--- Kenar durumlar ---
  [GEÇTİ] NULL girdi reddedildi                çökme yok
  [GEÇTİ] yetersiz tampon reddedildi           çökme yok
  [GEÇTİ] rastgele bayt çerçeve değil          sihirli sayı/CRC tuttu

  SONUÇ: 10 geçti, 0 kaldı
```

Biçim değiştiğinde ayrıca çekirdek, kanal **ve** çerçeve katmanlarında doğrudan
bayt karşılaştırması yapılır — bu oturumda eklenen bir kontrol, çünkü bir sürüm
geçişinde C# tarafında kalan eski bir doğrulama yalnızca .NET test paketi sayesinde
yakalanmıştı.

> **Yan fayda:** İki bağımsız implementasyonun aynı çıktıyı üretmesi, **biçim
> spesifikasyonunun da doğrulandığı** anlamına gelir. Arayüz kontrol dokümanı (ICD)
> yazarken bu doğrudan kanıttır.

---

## 4. C mi hızlı, C# mı? — sezgiye aykırı sonuç

**Saf skaler C, SIMD'li C#'tan hızlı çıktı.**

| İşlem | C (skaler) | C# (AVX2) | Fark |
| --- | ---: | ---: | --- |
| encode | **1.102 MB/sn** | 863 MB/sn | C %28 hızlı |
| decode | **1.450 MB/sn** | 846 MB/sn | C %71 hızlı |

**Sebebi:** C# tarafındaki AVX2 yalnızca fark hesabını ve aykırı maskeyi hızlandırır.
İşin asıl yükü olan **bit paketleme döngüsü** her iki sürümde de skalerdir — yani SIMD
toplam işin küçük bir kısmına dokunur. Buna karşılık C tarafında dizi sınır kontrolü
yoktur ve derleyici bazı döngüleri kendiliğinden vektörleştirir.

**Pratik sonuç:** gömülü/savunma hedefleri, aynı zamanda daha hızlı olan sürümü alır.

### SIMD durumu (C# tarafı)

| Platform | SIMD | Durum |
| --- | --- | --- |
| Intel/AMD (Xeon/Core) | AVX2, 8×32-bit paralel | ✅ Ölçüldü |
| ARM (Cortex-A, Jetson) | NEON, 4×32-bit paralel | 🧩 Kod mevcut, gerçek donanımda **ölçülmedi** |
| Diğer / eski | Skaler | ✅ Her zaman çalışır |

ARM NEON hızlanma rakamları gerçek donanımda doğrulanmadığı için buraya somut bir
"Nx hızlanma" sayısı **yazılmamıştır.**

### Elle SIMD neden yazılmayacak

Zorlayıcı bir telemetri hızı varsayalım: saniyede 400 kayıt (tipik İHA telemetrisi
1–50 Hz bandındadır).

```
Çerçeve başına 100 kayıt  →  saniyede 4 çerçeve
Çerçeve başına encode     →  1,8 µs (ölçüldü, C medyan)
────────────────────────────────────────────────────
Saniyede harcanan süre    :  7,2 µs
İşlemci kullanımı         :  ~%0,0007
```

İhtiyacın **~138.000 katı** kapasite var. Bu projede tıkanan yer işlemci değil,
**telsizin bant genişliğidir.** Elle SIMD %0,0007'yi %0,0004 yapar — ölçülebilir ama
anlamsız.

> **Dürüstlük notu:** Bu ölçüm masaüstü x64 üzerindedir. Gerçek hedef ARM kartlar
> (Raspberry Pi, Jetson) daha yavaştır ve **henüz ölçülmedi.** En kötü ihtimalle 10 kat
> yavaş olsa bile ~14.000 kat pay kalır.

---

## 5. Gecikme dağılımı — "deterministik" iddiasının sınavı

Gerçek-zamanlı bir sistemde ortalama gecikme neredeyse anlamsızdır; önemli olan **en
kötü ihtimalle ne kadar sürdüğüdür.** Çerçeve başına (100 kayıt × 3 kanal),
246 çerçeve × 200 tekrar:

| İşlem | en küçük | medyan | p95 | p99 | p99.9 | en büyük |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| encode | 1,20 µs | 1,80 µs | 2,10 µs | 2,70 µs | 4,00 µs | 18,30 µs |
| decode | 0,90 µs | 1,40 µs | 1,70 µs | 1,90 µs | 2,70 µs | 19,70 µs |

**Oynama oranı (p99 / medyan): encode 1,50x, decode 1,36x.** Dar bir bant.

**Veriye bağlı (algoritmik) değişkenlik: 1,78x.** Her çerçeve için 200 tekrarın
ortalaması alınarak işletim sistemi gürültüsü bastırıldığında en yavaş çerçeve, en
hızlının iki katından az sürüyor. Sabit blok yapısının beklenen davranışı.

> **Dürüstlük notu:** Ölçüm genel amaçlı bir işletim sistemi üzerindedir. En büyük
> değerler (18–20 µs) büyük ölçüde **işletim sistemi gürültüsüdür** — zamanlayıcı
> kesintileri, sayfa hataları, frekans ölçekleme. Gerçek WCET analizi ancak bir RTOS
> üzerinde ve statik analizle yapılabilir; **henüz yapılmadı.**

---

## 6. MISRA C:2012 uyumu

Kod baştan MISRA disipliniyle yazıldı, kural kural elle denetlendi, ardından **araçla
tarandı** ve kalan bulgular ya düzeltildi ya da gerekçesiyle kayda geçirildi.

| Kategori | Durum |
| --- | --- |
| Zorunlu (Mandatory) | İhlal yok |
| Gerekli (Required) | 1 kayıtlı sapma (21.15, `memcpy` tip yorumlaması) |
| Tavsiye (Advisory) | 1 kayıtlı sapma (15.5, tek çıkış noktası) |
| Cppcheck MISRA (2.21.0 + 2.13.0) | ✅ Kayıtlı sapmalar dışında 0 bulgu |
| Cppcheck `--enable=all` | ✅ 0 hata, 0 uyarı |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| MSVC `/W4` | ✅ 0 uyarı |

### Araç taraması süs değil, iş gördü

Altı gerçek bulgu düzeltildi:

| Kural | Sorun | Düzeltme |
| --- | --- | --- |
| 10.6 | `koşul ? 1 : 0` bileşik ifadedir; `int32_t`'ye atanması genişletmedir (5 yer, **Gerekli**) | Açık `if/else`. Cast ile susturmak 10.8'i ihlal ederdi — döngüsel |
| 10.1 | Kanal bayraklarında işaretli operandla bit işlemi | Ortak yardımcı işlevler |
| 12.2 | Kaydırma sınırı elle ispatlanabiliyordu ama araç göremiyordu | **Kaydırma tamamen kaldırıldı** — 33 girişlik maske tablosu |
| 10.4 | `sizeof(...) == 4` — işaretsizle işaretliyi karşılaştırma | `== 4u` |
| 17.8 | `deger >>= 1` parametreyi değiştiriyordu | Yerel kopya |
| 2.5 / 8.9 | Kullanılmayan makrolar, gereksiz dosya kapsamı | Temizlendi |

Hiçbiri bit akışını değiştirmedi; her adımda uygunluk vektörleri ve .NET ile birebir
karşılaştırma tekrar koşuldu.

> 12.2 düzeltmesi öğreticidir: *"araç anlamıyor, biz biliyoruz"* demek yerine kaydırma
> işlemi koddan çıkarıldı. Sonuç hem araç hem insan için ispatlanabilir oldu; maliyeti
> 132 baytlık salt-okunur tablo.

### İki araç sürümüyle tarama — ve nedeni

| Ortam | Cppcheck | Kural 10.6'yı yakaladı mı? |
| --- | --- | --- |
| Yerel | 2.21.0 | ❌ Hayır |
| CI | 2.13.0 | ✅ Evet, 5 adet |

Daha *eski* sürüm, daha yenisinin kaçırdığı bir **Gerekli** kuralı buldu. Buradan çıkan
sonuç: *tek bir araç sürümünden "temiz" almak zayıf bir kanıttır.* CI'daki `misra` işi
yalnızca rapor üretmez; kayıtlı sapmalar dışında bir kural görürse **derlemeyi kırar.**

### İnceleme sırasında bulunan zafiyet

Boyut hesapları (`eleman_sayisi * 4 + pay`) 32 bit tamsayıyla yapılıyordu. Çok büyük bir
`eleman_sayisi` çarpma sırasında taşarak **negatif ya da küçük** bir "gerekli boyut"
üretebilir, çağıran da yetersiz tampon ayırabilirdi — yani **tampon taşması**.

Kritik nokta: bu değer çözücü tarafında **bozuk/düşmanca bir paketten** de gelebiliyordu.

Düzeltme: `ELBARI_MAKS_ELEMAN` sınırı tanımlandı, tüm giriş noktalarında doğrulanıyor ve
çarpım taşması bölmeyle önceden denetleniyor.

### Nerede duruyoruz — açıkça

Kod bir **açık kaynak** MISRA denetleyicisinden (Cppcheck) kayıtlı sapmalar dışında
temiz geçiyor ve bu her push'ta CI'da tekrarlanıyor. Bu, **sertifikalı** bir araçla
(Helix QAC, PC-lint Plus, Polyspace) yapılmış bir doğrulama **değildir.**

> Şunu da netleştirelim: **"MISRA sertifikası" diye bir belge yoktur.** MISRA kod
> sertifikalandırmaz; uyum sizin beyanınızdır ve kanıtla desteklenir — uyum matrisi,
> sapma kaydı ve araç raporu. Elimizde üçü de var. Müşteri nitelikli bir araç raporu
> talep ederse o adım ayrıca yapılır; kod bunun için hazırdır.

Tam matris ve sapma kaydı: **[MISRA_UYUM.md](MISRA_UYUM.md)**

---

## 7. Sürekli tümleştirme (CI)

Geliştirme Windows/MSVC üzerinde yapılıyor; ama C sürümünün **varlık sebebi** Linux'lu
yardımcı bilgisayarlar, ARM kartlar ve RTOS'lardır — derleyicileri neredeyse istisnasız
GCC ya da Clang'dır. Yani kod, hedef kitlenin hiç kullanmayacağı derleyiciyle test
ediliyordu.

[GitHub Actions](../.github/workflows/derleme-ve-test.yml) her `push`'ta:

| İş | Ne yapar |
| --- | --- |
| **C (gcc)** | Gerçek Linux'ta sıkı uyarılarla derler, uygunluk + iki fuzz koşar |
| **C (clang)** | Aynısı Clang ile — farklı derleyici farklı hata yakalar |
| **C denetleyicileri** | **ASan + UBSan** altında koşar |
| **MISRA** | Cppcheck taraması; kayıtlı sapma dışı bulguda derlemeyi kırar |
| **.NET** | Derler ve 32 senaryoluk test paketini çalıştırır |

> **Denetleyiciler bu işin en değerli parçası.** MISRA belgesinde *"Kural 1.3 — tanımsız
> davranış yok"* diye iddia ediyoruz; bunu önce yalnızca **elle inceleyerek**
> doğrulamıştık. UBSan bunu **çalıştırarak** kanıtlar. `-fno-sanitize-recover=all` ile
> herhangi bir ihlalde süreç sıfır dışı kodla biter.

### CI kurulur kurulmaz gerçek bir hata yakaladı

```
error: 'CLOCK_MONOTONIC' undeclared
error: call to undeclared function 'clock_gettime'
```

`clock_gettime` bir POSIX işlevidir, ISO C'nin parçası değildir. `-std=c17` (katı ISO C)
ile derlenince glibc onu gizler; `_POSIX_C_SOURCE` tanımlanmalıydı.

**Bu hata Windows'ta asla görülemezdi** — orada `QueryPerformanceCounter` dalı
derleniyor, POSIX dalı hiç ziyaret edilmiyordu. Tam olarak CI'ın var olma sebebi.

> Önemli not: **kütüphanenin kendisi ilk denemede temiz derlendi.** Sorun yalnızca ölçüm
> test dosyasındaydı; `c/src/` altındaki kod taşınabilir çıktı.

---

## 8. Mevcut durum ve bilinen sınırlar

| Konu | Durum |
| --- | --- |
| Çekirdek / kanal / çerçeve katmanları | ✅ Tamamlandı |
| .NET ile ikili uyumluluk | ✅ Gerçek veriyle doğrulandı, üç katmanda |
| MSVC x64 derleme | ✅ `/W4` ile 0 uyarı |
| GCC / Clang derleme | ✅ CI'da her push'ta (Linux) |
| ASan + UBSan | ✅ CI'da temiz |
| MISRA C:2012 elle inceleme | ✅ Belgelendi ([MISRA_UYUM.md](MISRA_UYUM.md)) |
| MISRA araç taraması | ✅ İki Cppcheck sürümü, CI'da |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| Sağlamlık + değer fuzz'ı | ✅ CI'da her push'ta 300.000'er tur |
| Verim ve gecikme dağılımı | ✅ Ölçüldü |
| **Sertifikalı MISRA aracı** | ⏳ Yapılmadı — müşteri/program gerektirdiğinde |
| **ARM / big-endian doğrulama** | ⏳ Donanım yok; kod derlenir, hız ölçülmedi |
| **RTOS üzerinde WCET analizi** | ⏳ Yapılmadı |
| Elle yazılmış SIMD | ❌ **Yapılmayacak** — gerekçe §4'te |

### Gömülü sistem modu

C# tarafında derleme zamanı anahtarı:

```xml
<DefineConstants>EMBEDDED_MODE</DefineConstants>
```

Aktifken istisna fırlatılmaz; tampon taşmasında sessiz erken çıkış yapılır. Gerçek-zaman
kısıtlarına uyumludur. C sürümü zaten istisnasızdır ve dönüş kodu kullanır.
