# Kıyas — Genel Amaçlı Sıkıştırıcılar (Zstd / LZ4 / Brotli / Deflate)

> ⚠️ **Bunlar doğru rakip ailesi değildir.** ElBâri bir PFOR-Delta uygulamasıdır; asıl
> rakipleri Simple8b, OptPFD, Sprintz ve BP128'dir — o kıyas
> **[KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md)**'dedir ve orada fark çok
> daha dardır.
>
> Bu belge, telemetriye genel amaçlı bir sıkıştırıcı yapıştırmanın **yaygın bir refleks**
> olması nedeniyle var: o refleksin ne kadar pahalı olduğunu gösterir.

---

## Metodoloji

Aynı makinede, aynı gerçek GPS verisiyle (295.704 B ham), 20 tur ısınma + 200 tur ölçüm.
Rakipler **resmî .NET paketleriyle** çalıştırıldı: `ZstdSharp.Port`,
`K4os.Compression.LZ4`, ve .NET yerleşik `BrotliStream` / `DeflateStream` / `GZipStream`.
Tüm yöntemlerde round-trip doğrulandı (kayıpsız). Hız değerleri **ham veri üzerinden**
MB/sn'dir.

---

## 1. Ham baytlar — yaygın "naif" entegrasyon

Telemetriyi olduğu gibi sıkıştırıcıya vermek:

| Yöntem | Boyut | Oran | Encode | Decode |
| --- | ---: | ---: | ---: | ---: |
| **ElBâri — kanal ayrımı** | **58.513 B** | **5.05x** | **863 MB/sn** | **846 MB/sn** |
| **ElBâri — çerçeveli (100)** | 63.836 B | 4.63x | 224 MB/sn | 245 MB/sn |
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

**Neden bu kadar zayıf kalıyorlar?** Genel sıkıştırıcılar veriyi anlamsız bir bayt
yığını olarak görür. Telemetride kanallar iç içe geçmiştir
(`lat, lon, alt, lat, lon, alt, ...`) ve bu, örüntü aramalarını köreltir. ElBâri verinin
**kayıt yapısını bilir** ve önce kanallara ayırır.

---

## 2. "Ön işlemeyi biz de yaparız" itirazı — ölçüldü

Haklı bir itiraz: kanal ayrımı ve kuantalama herkesin yapabileceği ön işlemlerdir. Bu
yüzden ikisi de rakiplere **bedava verildi.**

### Kanal ayrımı verildiğinde

| Yöntem | Boyut | Oran | Encode |
| --- | ---: | ---: | ---: |
| Zstd (seviye 1) | 188.464 B | 1.57x | 590 MB/sn |
| Zstd (seviye 19) | 87.076 B | 3.40x | 7 MB/sn |
| LZ4 (hızlı) | 234.028 B | 1.26x | 1.247 MB/sn |
| Brotli (q11) | **76.241 B** | **3.88x** | 1 MB/sn |
| Deflate (optimal) | 162.898 B | 1.82x | 62 MB/sn |

### Kuantalama da verildiğinde

| Yöntem | Oran | Encode |
| --- | ---: | ---: |
| **ElBâri (kuantalanmış float)** | **10.51x** | **981 MB/sn** |
| Brotli q11 + kanal ayrımı | 7.84x | 1 MB/sn |
| Zstd sev.19 + kanal ayrımı | 7.34x | 4 MB/sn |
| Zstd sev.1 + kanal ayrımı | 4.80x | 392 MB/sn |

Her iki ön işlem de verildikten sonra bile ElBâri **%34 önde** ve **~1.000 kat hızlı.**

---

## 3. Ne çıkıyor bu tablodan

**Hız/oran ödünleşiminde boş bir köşe.** Rakipler iki uçtan birinde: ya hızlı ama zayıf
oran (LZ4 1.29x, Zstd-1 1.61x), ya iyi oran ama çok yavaş (Brotli-q11 3.59x @ 1 MB/sn,
Zstd-19 3.03x @ 8 MB/sn). **Hem 3x üzeri oran hem 800+ MB/sn hızı** aynı anda veren tek
yöntem ElBâri'dir.

**Tabloda görünmeyen farklar.** Bu ölçüm yalnızca oran ve hızı kapsar. Listedeki
rakiplerin **hiçbirinde** şunlar yoktur:

| Özellik | ElBâri | Zstd / LZ4 / Brotli |
| --- | --- | --- |
| Paket kaybına dayanıklılık | ✅ Bağımsız çerçeveler | ❌ Akış bozulur |
| Sıfır heap tahsisatı | ✅ Ölçüldü (0 bayt) | ❌ Bellek ayırır |
| Deterministik gecikme | ✅ Sabit blok yapısı | ❌ Değişken |
| Harici bağımlılık | ✅ Yok | ❌ Native kütüphane |
| Çerçeve başına bütünlük | ✅ CRC32 | ❌ Yok (akış seviyesi) |

---

## 4. Ne zaman hangisi?

| Durum | Seçim |
| --- | --- |
| Kayıplı/dar RF linki, gerçek-zaman kısıtı, gömülü/denetlenebilir ortam, çok kanallı telemetri | **ElBâri** |
| Güvenilir taşıma (TCP/dosya), hızın önemsiz olduğu arşivleme, maksimum oran | **Zstd / Brotli** |
| Saf hız gerekiyorsa ve düşük oran kabul edilebilirse | **LZ4** |

---

## 5. Bu üstünlük nereye kadar geçerli

> ⚠️ **Yalnızca bu tablo için.** Doğru rakip ailesiyle (Simple8b, OptPFD, Sprintz)
> ölçüldüğünde fark **çok daralıyor** ve veri setine göre işaret bile değiştiriyor:
> yedi veri setinin beşinde önde, ikisinde geride.
>
> Ayrıntı ve dürüst sınırlar:
> **[KIYAS_TAMSAYI_KODEKLER.md](KIYAS_TAMSAYI_KODEKLER.md)**

### Tarihçe — biçim sürümlerinin bu tabloya etkisi

Sürüm 1'de Brotli-q11 kanal-ayrılmış veride **3.88x** ile ElBâri'yi (3.56x) geçiyordu.
Sürüm 2'deki bit genişliği tablosu genişletmesi bu değeri aştı; sürüm 3'teki blok üstü
sıfır koşusuyla birlikte ElBâri **5.05x**'e çıktı — üstelik Brotli'den ~800 kat hızlı
encode ederek.
