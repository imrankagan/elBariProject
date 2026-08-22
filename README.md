# ElBâri — Telemetri Sıkıştırma Motoru

[![Lisans: Akademik serbest / Ticari lisanslı](https://img.shields.io/badge/lisans-akademik%20serbest%20%7C%20ticari%20lisansl%C4%B1-orange.svg)](LICENSE.txt)
[![.NET 10](https://img.shields.io/badge/.NET-10-purple.svg)](https://dotnet.microsoft.com/)
[![C99 / C17](https://img.shields.io/badge/C-99%20%2F%2017-blue.svg)](c/)
[![Biçim sürümü 4](https://img.shields.io/badge/bi%C3%A7im-s%C3%BCr%C3%BCm%204-lightgrey.svg)](belgeler/BICIM_SPESIFIKASYONU.md)
[![Derleme ve Test](https://github.com/imrankagan/elBariProject/actions/workflows/derleme-ve-test.yml/badge.svg)](https://github.com/imrankagan/elBariProject/actions/workflows/derleme-ve-test.yml)

**Kayıplı telsiz linkleri üzerinden akan İHA telemetrisi için kayıpsız tamsayı
sıkıştırma motoru.** İki bağımsız implementasyon (C# ve C), tek biçim, bit bit aynı
çıktı.

---

## Bu ne, ne değil

Telemetri dar bir telsiz linkinden gider ve **paketler düşer.** Klasik delta kodlamanın
ölümcül zayıflığı zincirleme bağımlılıktır: her değer bir öncekine dayandığı için tek
bir paket düşünce ondan sonraki *her şey* çözülemez.

ElBâri akışı, her biri kendi mutlak referansını taşıyan **bağımsız çerçevelere** böler.
Bir paket düşerse yalnızca o çerçevenin kayıtları kaybolur.

**Ayırt edici özelliği en yüksek sıkıştırma oranı değildir.** Bu dürüstçe söylenmelidir:
kendi ailesindeki kodeklerle (Simple8b, Sprintz, OptPFD, BP128) arasındaki fark tek
haneli yüzdelerdedir. Ayırt edici olan şudur — **ailede kayıplı linkte çalışabilen tek
üye odur**, ve bunu yaparken üç garantiyi bir arada verir:

1. **Sıfır heap tahsisatı** — GC duraklaması yok, deterministik davranış
2. **Bağımlılıksız ve taşınabilir** — C sürümü RTOS ve bare-metal dâhil derleyicisi olan
   her mimariye girer
3. **İki bağımsız implementasyon, tek biçim** — C ve C# bit bit aynı çıktıyı üretir; bu
   aynı zamanda biçim spesifikasyonunun da kanıtıdır

> **Ölçüm tabanı — hepsi gerçek veri.** OpenStreetMap GPS iz arşivi ve
> [ALFA veri setinden](https://theairlab.org/alfa-dataset/) gerçek bir ArduPilot uçuş
> logu (yönelim, IMU, GPS, servo, kumanda, titreşim). Sentetik veri yalnızca kenar durum
> testlerinde kullanılır.

---

## Bir bakışta — nerede iyiyiz, nerede değiliz

| Konu | Durum |
| --- | --- |
| **Sıkıştırma oranı, çerçevesiz** | Yedi veri setinin **beşinde** aile lideri; IMU ve titreşimde %2–7 geride |
| **Sıkıştırma oranı, çerçeveli** | 100 kayıt/çerçevede **beşte beş** lider; 25 kayıtta %2–4 geride |
| **Paket kaybı dayanıklılığı** | Ailede **rakipsiz** — diğerlerinde hiç yok |
| **Encode hızı** | Aynı derleme sınıfında 2–2,6 kat önde — ama iddia **kurulmadı**, rakiplerin SIMD kütüphaneleri bağlanmadı |
| **Decode hızı** | Orta sıralarda; bir üstünlük yok |
| **Gömülü uygunluk** | MISRA C:2012, sıfır tahsisat, bağımlılıksız, `/W4` temiz |
| **ARM'da doğrulama** | ⏳ Donanım yok — kod derleniyor, ölçülmedi |
| **Sertifikalı MISRA aracı** | ⏳ Açık kaynak araç temiz; nitelikli araç raporu yok |

---

## Nasıl çalışır — üç katman

Her katman bir problemi çözer ve bir bedeli vardır. **İhtiyacın kadarını kullanırsın.**

```
┌─ Çerçeve ────────────────────────────────────┐   Paket kaybı dayanıklılığı
│ başlık(10) │ ┌─ Kanal ────────────────────┐  │   + CRC32 + sıra no
│            │ │ başlık │ ┌─ Çekirdek ───┐  │  │   Kayıt akışını kanallara ayırır
│            │ │        │ │ bit akışı    │  │  │   Delta + adaptif bit paketleme
│            │ │        │ └──────────────┘  │  │
│            │ └────────────────────────────┘  │
└──────────────────────────────────────────────┘
```

| Katman | Çözdüğü problem | Bedeli |
| --- | --- | --- |
| **Çekirdek** | Ardışık farklar küçüktür; az bitle yaz | — |
| **Kanal** | Telemetri tek sayı akışı değil, **kayıt** akışıdır | Kanal başına küçük başlık |
| **Çerçeve** | Paket düşünce zincir kopar | Çerçeve başına ~20 bayt + oran kaybı |

**Kanal katmanı neden zorunlu?** Kayıt akışı olduğu gibi verilirse ardışık farklar
kanallar arasında zıplar (`enlem → boylam` farkı milyonlarca birim olur) ve veri
*"sıkıştırılamaz"* diye reddedilir. Ölçüldü: kanal ayrımı olmadan **reddediliyor**,
ayrımla **5.05x**. Bu ElBâri'ye özgü değil — kanal ayrımı olmadan ailenin tamamı çöküyor
(Simple8b veriyi *ikiye katlıyor*).

👉 Ayrıntı, gerekçeler ve biçim sürümleri: **[belgeler/MIMARI.md](belgeler/MIMARI.md)**

---

## Ölçülen sonuçlar

### 1. Kendi ailesiyle — yedi veri seti, çerçevesiz

Rakiplere kanal ayrımı + fark + zigzag **bedava verilerek** ölçüldü.

| Veri seti | K | **ElBâri** | Ailenin en iyisi | Fark |
| --- | ---: | ---: | --- | ---: |
| Kumanda (RCIN) | 8 | **92.44x** | Sprintz 74.39x | **+%24,3** |
| Servo (RCOU) | 8 | **37.54x** | Sprintz 34.18x | **+%9,8** |
| Yönelim (ATT) | 3 | **15.58x** | Sprintz 15.55x | **+%0,2** |
| GPS (ALFA uçuş) | 3 | **5.60x** | Simple8b 5.43x | **+%3,1** |
| GPS (OSM referans) | 3 | **5.05x** | Sprintz 4.67x | **+%8,1** |
| IMU (jiro+ivme) | 6 | 6.88x | Simple8b 7.43x | −%7,4 |
| Titreşim (VIBE) | 3 | 5.38x | Simple8b 5.51x | −%2,4 |

### 2. Kendi ailesiyle — çerçeveleme herkese verildiğinde

Rakiplerde çerçeveleme yoktur; adil kıyas için aynı yük onlara da verilir (bağımsız
parçalar + aynı başlık). **Çerçeveleme herkesi vurur, ama ElBâri'yi en az vurur:**

| Veri seti (100 kayıt/çerçeve) | **ElBâri** | Sprintz | Simple8b |
| --- | ---: | ---: | ---: |
| Kumanda (RCIN) | **33.44x** | 17.67x | 12.14x |
| Yönelim | **13.24x** | 9.04x | 8.28x |
| IMU | **6.35x** | 5.58x | 6.07x |
| Titreşim | **4.92x** | 4.47x | 4.61x |
| GPS | **4.63x** | 3.94x | 3.83x |

IMU ve titreşim, çerçevesiz kıyasta *kaybettiğimiz* iki settir. Sıralama tersine dönüyor.

👉 Tam rapor, metodoloji ve dürüst sınırlar:
**[belgeler/KIYAS_TAMSAYI_KODEKLER.md](belgeler/KIYAS_TAMSAYI_KODEKLER.md)**

### 3. Paket kaybı — ayırt edici özellik

Gerçek yönelim verisi, 100 kayıt/çerçeve, MTU 250 bayt. Kurtarma **tahmin edilmez**:
düşen paketler gerçekten atılır, hayatta kalan çerçeveler gerçekten çözülür ve orijinalle
**bit bit** karşılaştırılır.

| Paket kaybı | **Çerçeveli** — kurtarılan kayıt | Çerçevesiz — akışın tamamının hayatta kalma olasılığı |
| ---: | ---: | ---: |
| %1 | **%98,9** | %12 |
| %5 | **%94,4** | ~%0 |
| %10 | **%90,3** | ~%0 |
| %25 | **%75,0** | ~%0 |

Sağdaki sütun meselenin özü. Çerçevesiz akış 211 pakete bölünür ve **tek bir paket
düşerse hiçbir şey çözülemez**; %1 kayıpta bile akışın tamamının sağ çıkma olasılığı
%12'dir. Nominal 15,58x'lik oran, etkin olarak **1,9x**'e iner.

Çerçeveli yaklaşımda ise kayıp lineerdir — ne düştüyse o kadar.

**Peki çerçeve kaç kayıt olmalı?** Optimum, kayıt sayısıyla değil **çerçevenin kaç
pakete bölündüğüyle** belirlenir. Bir sürpriz de var: patlamalı kayıp, bağımsız kayıptan
*iyidir* — çünkü hasar az sayıda çerçevede yoğunlaşır.

👉 Kayıp × çerçeve boyutu süpürmesi:
**[belgeler/KAYIP_DAYANIKLILIK.md](belgeler/KAYIP_DAYANIKLILIK.md)**

### 4. Canlı telemetri — iki kademeli MAVLink vekili

Telsizin iki ucuna şeffaf birer vekil konur; kritik mesajlar sıfır gecikmeyle geçer,
yüksek hızlı telemetri biriktirilip sıkıştırılır.

| Hız profili | 2 sn bütçe | 5 sn bütçe |
| --- | ---: | ---: |
| SR1 — telemetri telsizi (dar bant) | 1,16x | **1,68x** |
| SR0 — USB / companion (geniş bant) | **1,71x** | 1,71x |

**Vekil, akış hızı yükseldikçe kazanır.** Dar bantta 2 saniyelik bütçeye yalnızca 4–8
kayıt sığar ve çerçeve başlığı bu kadar az kaydın üzerine dağıtılamaz. Tek bir kazanç
rakamı vermek yanıltıcıdır; profil belirtilmelidir.

👉 **[belgeler/MAVLINK_VEKIL.md](belgeler/MAVLINK_VEKIL.md)**

### 5. Genel amaçlı sıkıştırıcılar (yanlış aile, referans olsun)

| Yöntem | Oran | Encode |
| --- | ---: | ---: |
| **ElBâri** | **5.05x** | **863 MB/sn** |
| Brotli q11 | 3.59x | 1 MB/sn |
| Zstd-19 | 3.03x | 8 MB/sn |
| Zstd-1 | 1.61x | 210 MB/sn |
| LZ4 | 1.29x | 923 MB/sn |

Hem orandan hem hızdan önde — ama bunlar telemetri kodeği değil. Anlamlı kıyas §1'dir.

👉 **[belgeler/KIYAS_GENEL_AMACLI.md](belgeler/KIYAS_GENEL_AMACLI.md)**

---

## Hızlı başlangıç

```bash
# C sürümü (Windows)
c\derle.bat
c\uygunluk.exe testverisi\vektorler.txt      # 29 biçim vektörü
c\deger_fuzz.exe 300000                      # kodlayıcı tam turu

# C sürümü (Linux / macOS)
cd c && make test

# .NET sürümü
dotnet run --configuration Debug             # 32 senaryoluk test paketi
```

```csharp
// C# — çok kanallı telemetri, kayıplı link
int kanal = 3;
int[] calisma = new int[ElBâriKanal.GerekliCalismaAlani(veri.Length, kanal)];
byte[] paket  = new byte[ElBâriÇerçeve.EnKotuDurumCerceveBoyutu(kayit, kanal)];

int n = ElBâriÇerçeve.CerceveYaz(veri, kanal, siraNo, calisma, paket);
bool tamam = ElBâriÇerçeve.CerceveOku(gelen, kanal, calisma, geri,
                                      out uint sira, out int kayitSayisi);
```

> ⚠️ **Güvenilmeyen kaynaktan gelen veri daima çerçeve katmanından geçirilmelidir.**
> Bütünlük kontrolü (CRC32) yalnızca orada vardır.

👉 API örnekleri, tampon boyutları, dönüş kodları:
**[belgeler/KULLANIM.md](belgeler/KULLANIM.md)**

---

## Belge haritası

| Belge | İçerik |
| --- | --- |
| [MIMARI.md](belgeler/MIMARI.md) | Üç katmanın **neden** var olduğu, float katmanları, algoritma ailesi, biçim sürümleri |
| [KULLANIM.md](belgeler/KULLANIM.md) | C# ve C API'leri, katmanlı bütünlük kuralı, derleme, ölçümü kendiniz koşma |
| [BICIM_SPESIFIKASYONU.md](belgeler/BICIM_SPESIFIKASYONU.md) | **Arayüz Kontrol Dokümanı (ICD)** — bayt düzeyinde tam tanım |
| [C_SURUMU.md](belgeler/C_SURUMU.md) | Neden ikinci implementasyon, MISRA uyumu, ikili uyumluluk, CI, gecikme dağılımı |
| [TEST_VE_DOGRULAMA.md](belgeler/TEST_VE_DOGRULAMA.md) | Test envanteri, iki fuzz, gerçek veriyle bulunan hatalar, **bilinen boşluklar** |
| [KIYAS_TAMSAYI_KODEKLER.md](belgeler/KIYAS_TAMSAYI_KODEKLER.md) | Asıl kıyas: kendi ailesiyle, yedi veri seti |
| [KIYAS_GENEL_AMACLI.md](belgeler/KIYAS_GENEL_AMACLI.md) | Zstd / LZ4 / Brotli — referans |
| [KAYIP_DAYANIKLILIK.md](belgeler/KAYIP_DAYANIKLILIK.md) | Kayıp × çerçeve boyutu süpürmesi; optimumun ölçümü |
| [MAVLINK_VEKIL.md](belgeler/MAVLINK_VEKIL.md) | Canlı telemetri senaryosu, iki hız profili |
| [OLCUM_SONUCLARI.md](belgeler/OLCUM_SONUCLARI.md) | Ham ölçüm raporu: verim, gecikme, teorik alt sınır |
| [MISRA_UYUM.md](belgeler/MISRA_UYUM.md) | Kural kural uyum matrisi ve sapma kaydı |
| [AKIS_SEMASI.md](belgeler/AKIS_SEMASI.md) | Görsel akış şemaları |

---

## Proje yapısı

```
kaynak/          C# kaynak kodu (5 dosya, üç katman + float)
c/               C sürümü — bağımsız, bağımlılıksız
  src/             kütüphane (~2.500 satır)
  test/            doğrulama, ölçüm, iki fuzz, uygunluk, kayıp süpürmesi
  kiyas/           tamsayı kodek ailesiyle karşılaştırma
  mavlink/         iki kademeli MAVLink vekili ve ölçümü
  veri/            ArduPilot DataFlash log okuyucusu → ölçüm fikstürleri
benchmark/       .NET test ve ölçüm paketi (32 senaryo)
belgeler/        spesifikasyon ve ölçüm raporları
testverisi/      gerçek GPS verisi + 29 dondurulmuş uygunluk vektörü
.github/         sürekli tümleştirme
```

---

## Bilinen sınırlar

Bunlar eksiklik değil, **henüz yapılmamış** işlerdir — ve açıkça yazılmaları önemlidir:

| Sınır | Neden önemli |
| --- | --- |
| **Hız iddiası kurulmadı** | Kıyastaki rakipler yeniden yazılmış **skaler** sürümlerdir. FastPFor ve streamvbyte'ın SIMD kütüphaneleri bağlanana kadar hız sütunu rakipler için bir *alt sınırdır* |
| **Tek uçuş, tek platform** | Uçuş fikstürlerinin hepsi aynı ALFA uçuşundan. Platform **sabit kanattır**; yönelimi bir çoklu rotordan düzgündür, dolayısıyla oranlar çoklu rotora göre **iyimser** |
| **ARM / big-endian ölçülmedi** | Kod derleniyor, davranış ve hız donanımda doğrulanmadı |
| **RTOS'ta WCET analizi yok** | Gecikme dağılımı genel amaçlı işletim sisteminde ölçüldü |
| **Sertifikalı MISRA aracı yok** | Açık kaynak Cppcheck temiz geçiyor; nitelikli araç raporu ayrı bir adım |
| **Gerçek telsizle uçtan uca test yok** | Kayıp altında kurtarma simülasyonla ölçüldü |

Sıradaki somut adımlar için ilgili raporların "sıradaki adımlar" bölümlerine bakınız.

---

## Lisans, IP ve sorumluluk

**Lisans.** Akademik ve eğitim amaçlı kullanım, inceleme ve atıf **serbesttir** — tez
çalışmaları dâhil. Ticari kullanım ayrı bir lisans gerektirir; dağıtım ve yeniden
yayımlama yasaktır. Ayrıntı: [LICENSE.txt](LICENSE.txt) · İletişim: imrankagant@gmail.com

**Patent notu.** Kullanılan teknikler (delta encoding, bit packing, variable bit-width,
PFOR patching) onlarca yıldır halka açık ve yayınlanmıştır ve literatürde prior art
oluşturur. Bu bir **hukukî görüş değildir** ve patent taraması yapılmamıştır; ticari
dağıtım öncesinde yetkin bir tarafça inceleme gerekir.

**Dağıtım koruması.** C# sürümü Native AOT ile derlenir (`PublishAot`, `DebugType=none`,
`IlcDisableReflection`, full trimming) — IL yoktur, .NET decompiler'ları işe yaramaz.
Bu yalnızca C# sürümünü ilgilendirir; **C sürümü kaynak olarak dağıtılır ve okunabilir
olması amaçlanmıştır** — denetlenebilirlik onun varlık sebebidir.

**Sorumluluk reddi.** Bu yazılım **"OLDUĞU GİBİ"** sağlanmaktadır; hiçbir garanti
verilmez. Kritik sistemlerde (İHA, askerî, medikal) kullanmadan önce kapsamlı test
yapın, simülasyonda doğrulayın, sertifikasyon gereksinimlerinizi kontrol edin ve
watchdog ile koruyun.

---

**© 2025-2026 İmran Kağan.** Akademik kullanım serbest · Ticari kullanım lisansa tabidir
→ [LICENSE.txt](LICENSE.txt)
