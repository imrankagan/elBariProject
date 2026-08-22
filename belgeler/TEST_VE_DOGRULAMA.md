# Test ve Doğrulama

> Çalıştırma komutları: [KULLANIM.md](KULLANIM.md) ·
> MISRA ve statik analiz: [C_SURUMU.md §6](C_SURUMU.md)

Bu belge **neyin nasıl kanıtlandığını** anlatır. Her testin bir amacı ve bir **bilinen
boşluğu** vardır; ikisi de yazılıdır.

---

## Özet

| Test | Ne kanıtlar | Sıklık |
| --- | --- | --- |
| **Uygunluk vektörleri** (29) | Biçim dondurulmuştur; bağımsız bir implementasyon aynı baytı üretir | Her push |
| **Değer fuzz'ı** (300.000 tur) | Kodlayıcının **kabul ettiği** her girdi kayıpsız geri gelir | Her push |
| **Çözücü fuzz'ı** (300.000 tur) | Çözücü **hiçbir** girdide çökmez, taşmaz | Her push |
| **Kodek öz-testi** | Kıyastaki rakip uygulamalar da kayıpsız | Her push |
| **.NET takımı** (32 senaryo) | Uçtan uca davranış, kenar durumlar | Her push |
| **İkili uyumluluk** | C ve C# bit bit aynı | Biçim değiştiğinde |
| **ASan + UBSan** | Tanımsız davranış yok — iddia değil, çalıştırarak | Her push |

---

## 1. Dondurulmuş uygunluk vektörleri

[`testverisi/vektorler.txt`](../testverisi/vektorler.txt) — **29 referans vektör.**

Bir implementasyon uyumlu sayılır **ancak ve ancak**:

1. Her vektörün girdisinden **birebir aynı bayt dizisini** üretiyorsa, **ve**
2. Her vektörün çıktısından **birebir aynı girdiyi** geri kurabiliyorsa.

| Implementasyon | Uygunluk |
| --- | --- |
| C# (.NET 10) | ✅ Referans — vektörler bundan üretildi |
| C (C99/C17) | ✅ **29 vektör, 58 kontrol, 0 hata** |

Kapsam: bit genişliği tablosu, aykırı değerler, kısmi bloklar, ikinci derece fark,
ham geçiş, çerçeve başlığı, blok üstü sıfır koşusu, float kuantalaması, XOR katmanı ve
`cekirdek_isaret_biti` (aşağıya bakınız).

> **Neden bu önemli:** Savunma ve havacılık tedarikinde satın alınan şey koddan çok
> **spesifikasyondur.** İki bağımsız implementasyonun aynı vektörleri üretmesi, biçimin
> belgeyle tutarlı olduğunun kanıtıdır — belge ile kod arasında sessiz bir sapma yoktur.

### Ayrıştırıcının kendi kusuru da düzeltildi

Uygunluk aracı, eleman sınırını aşan bir vektörü **sessizce atlıyordu**. 300 değerlik
yeni bir vektör eklendiğinde sayı 29 yerine 28 kaldığı için fark edildi. Sınır
yükseltildi ve aşım artık **hata olarak bildiriliyor** — bozuk bir vektör fark edilmeden
geçemez.

---

## 2. İki ayrı fuzz — ve neden iki tane gerekti

Uzun süre tek bir fuzz vardı ve bir boşluk bırakıyordu. Bedeli ölçüldü.

### 2a. Çözücü sağlamlık fuzz'ı — `c/test/fuzz.c`

**Soru:** Çözücü, düşmanca ve bozuk girdide çöker mi?

Telemetri çözücüsü kayıplı ve düşmanca bir telsiz ortamından veri alır. Saldırgan özel
hazırlanmış bir paketle alıcıyı çökertmeye çalışabilir; çözücünün hiçbir girdide
çökmemesi bir **güvenlik gereksinimidir**.

**Yöntem:** Çıktı tamponlarının önüne ve arkasına bilinen bir desen ("kanarya") yazılır.
Çağrı sonrası desen bozulmuşsa kütüphane tampon dışına yazmıştır. Bu, çökmeye yol
açmayan **sessiz taşmaları** da yakalar. Üreteç deterministiktir; bulunan her hata
tohumla birebir yeniden üretilebilir.

**Sonuç (300.000 tur):**

| Ölçüt | Sonuç |
| --- | --- |
| Tampon taşması | **0** |
| Süreç çökmesi | Yok |
| Bozulmuş çerçevelerin reddi | **%100,00** |

| Katman | Kabul | Red | Bütünlük kontrolü |
| --- | ---: | ---: | --- |
| Çekirdek | 6 | 60.701 | Yapısal tüketim kontrolü |
| Kanal | 0 | 119.561 | Başlık tutarlılığı |
| Çerçeve (bozulmuş) | 0 | 59.792 | **CRC32** |
| Float XOR | 5.847 | 54.093 | Sağlama toplamı yok |

> **Float XOR'un kabul oranı neden yüksek?** O katmanda bütünlük kontrolü yoktur ve bit
> deseni akışında hemen her bayt dizisi geçerli bir float dizisi olarak çözülebilir.
> Tasarım gereğidir; bütünlük isteyen çerçeve katmanını kullanmalıdır.

### 2b. Kodlayıcı değer fuzz'ı — `c/test/deger_fuzz.c`

**Soru:** Kodlayıcının **kabul ettiği** her girdi kayıpsız geri geliyor mu?

Yukarıdaki fuzz kodlayıcıyı hiç çalıştırmaz — bozuk baytları çözücüye verir. Bu yüzden
**kodlayıcı tarafında oluşan sessiz veri kayıplarını göremez.**

Bu boşluğun bedeli ölçüldü: 2³¹'lik fark hatası (aşağıda) ne 27 uygunluk vektörüne ne de
300.000 turluk çözücü fuzz'ına takıldı. Ancak gerçek bir ArduPilot logu bağlanınca
ortaya çıktı.

`deger_fuzz.c` bu boşluğu kapatır: **12 düşmanca değer dağılımını** dört katmanda
(çekirdek, kanal, çerçeve, float XOR) encode → decode turundan geçirir ve **bit bit**
karşılaştırır. Üreteçler kenar durumları bilerek hedefler:

| Üreteç | Neyi zorlar |
| --- | --- |
| `saf_rastgele` | Tüm 32 bit rastgele — çoğu blok aykırı |
| `duzgun_kucuk` | Dar bit genişliği yolu |
| `sabit` | Sıfır blok yolu |
| `isaret_donusu_2p31` | **Tam 2³¹'lik fark** — kayıpsızlık hatası buradan çıktı |
| `sinir_degerleri` | `INT32_MIN/MAX`, 0, ±32767, ±32768 |
| `aykiri_esiginde` | Farklar tam eşiğin iki yanında |
| `float_bitleri` | Gerçekçi float bit desenleri |
| `seyrek_sicrama` | Çoğu sıfır + nadir dev sıçrama |
| `sabit_hiz` | İkinci derece fark yolu |
| `sifir_kosusu` | Uzun sıfır blokları + arada tek sıçrama |
| `artan_genislik` | Her blok bir sonraki genişliğe geçer |
| `karisik` | Eleman başına rastgele üreteç seçimi |

**Aracın kendisi sınandı:** düzeltme geçici olarak geri alındığında hatayı **20.000
turda** yakaladı ve `fark: 0x80000000` teşhisiyle raporladı. Düzeltilmiş sürümde
300.000 turda 0 kayıp.

---

## 3. Gerçek veriyle bulunan hatalar

Bu bölüm, testlerin *neyi kaçırdığını* gösterdiği için burada duruyor.

### 3a. Kayıpsızlık hatası — tam 2³¹'lik fark

Ardışık iki değerin farkı tam olarak `INT32_MIN` olduğunda, 32 bitlik mutlak değer
negatif kalıyordu. Bu yüzden fark ne "aykırı" işaretleniyor (tam 32 bitle yazılmıyor)
ne de blok bit genişliğini yükseltiyordu; sonra dar maskeyle paketlenip **üst bitini
kaybediyordu.**

Pratikte bir float'ın işareti değişip büyüklüğü aynı kaldığında oluşur:
`+0.001f` → `-0.001f`, bit desenleri `0x3A83126F` / `0xBA83126F`.

**Nasıl bulundu:** Gerçek ArduPilot logunun jiroskop kanalı MAVLink vekiline bağlanınca
kayıpsız modda tam tur doğrulaması düştü.

**Neden daha önce bulunamadı:** O tarihte var olan 27 vektörün hiçbirinde 2³¹'lik fark yoktu; fuzz ise
çözücü sağlamlığını sınıyordu. Sentetik yönelim verisi sıfırı geçiyordu ama işaret
değişimi hep büyüklük değişimiyle birlikteydi — tam olarak 2³¹ hiç çıkmadı.

**Sonuç:** Düzeltildi (C ve C#), `cekirdek_isaret_biti` regresyon vektörü eklendi,
ve değer fuzz'ı yazıldı.

### 3b. Ölçüm hatası — reddetme oranı

İlk fuzz koşusunda 17 bozulmuş çerçeve kabul edilmiş görünüyordu. CRC32 çarpışması bu
sıklıkta olamayacağı için araştırıldı: fuzzer bozma yaparken rastgele bir değer
*atıyordu*, yani 1/256 olasılıkla aynı baytı yazıp paketi aslında hiç bozmuyordu.

Bozma XOR'a çevrildi; reddetme oranı %100,00 oldu.
**Kütüphanede hata yoktu, ölçümde vardı.**

---

## 4. Yapısal tüketim kontrolü — sağlama toplamı olmadan çöpü elemek

Geçerli bir sıkıştırılmış akış girdinin **tamamını** tüketir: kodlayıcı tam olarak
gerektiği kadar bayt yazar, çözücü de tam olarak o kadarını okur. Geriye artık kalmışsa
girdi bu kodlayıcıdan çıkmamıştır.

**Maliyeti tek bir karşılaştırmadır.** Etkisi ölçüldü: çekirdeğin kabul ettiği çöp girdi
**88.963 → 17** (%99,98 azalma).

Bu, sağlama toplamının yerini **tutmaz** — tam bütünlük için çerçeve katmanı gerekir —
ama tuzağın büyük kısmını kapatır.

> ⚠️ **Kullanım şartı:** `ElBâsıt`/`elbari_basit`'e sıkıştırılmış verinin **tam boyutu**
> verilmelidir. Çözücü verinin nerede bittiğini kendi başına bilemez.

Biçim sürümü 4'te kanal katmanı kanalları ardışık çözdüğü için bu kontrol **kanal
düzeyine** taşındı: tüm kanallar bittiğinde girdinin tamamı tüketilmiş olmalıdır.

---

## 5. .NET test paketi

```bash
dotnet run --configuration Debug
```

```
Total Tests:  32
Passed:       25
Rejected:      7   (sıkıştırılamaz / anlamsız veri — beklenen)
Failed:        0
Success Rate: 100.0%
```

| Kategori | Test | Kapsam |
| --- | ---: | --- |
| Correctness | 4 | Kenar durum, boş dizi, tek eleman |
| Compression Quality | 5 | Sequential, constant, dense |
| Performance | 3 | Random, sparse, büyük veri |
| Real-World | 5 | Sine, Gaussian, telemetri |
| Stress | 4 | Worst case, 1M eleman |
| Edge Cases | 4 | Min/max int, mixed, zigzag |
| **Real Data** | **4** | **Gerçek GPS: kanal ayrımsız / ayrımlı / çerçeveli** |
| **Multi-Channel** | **3** | **6 kanallı İHA telemetrisi** |

`Rejected` bir başarısızlık değildir: kodlayıcı rastgele/anlamsız veriyi bilinçli
reddeder ve çağıran ham gönderir.

### Gerçek veri senaryolarının çıktısı

| Senaryo | Sonuç |
| --- | --- |
| GERÇEK GPS — kanal ayrımsız | ⊘ REDDEDİLDİ (beklenen) |
| GERÇEK GPS — kanal ayrımı | **5.05x** |
| GERÇEK GPS — çerçeveli (100) | **4.63x** (CRC dahil) |
| GERÇEK GPS — çerçeveli (500) | **4.98x** |
| İHA 6 kanal — kanal ayrımsız | ⊘ REDDEDİLDİ (beklenen) |
| İHA 6 kanal — kanal ayrımı | **7.12x** |
| İHA 6 kanal — çerçeveli (250) | **6.78x** |

### Tahsisat

```
100 encode+decode turu, gerçek GPS verisi:
  Tahsis edilen bayt: 0
  ✓ SIFIR tahsisat — heap'e hiç dokunulmadı, GC baskısı yok.
```

---

## 6. Kayıpsızlık doğrulaması (.NET tarafı)

Her iki katman için **1.617 test** (14 hedefli senaryo + 1.600 tur rastgele fuzz)
çalıştırıldı; tamamı kayıpsız. Çerçeveler ters sırada ve birbirinden bağımsız
çözülebiliyor. Kenar durumlar dahil: tek eleman, eksik kayıt, `K=255`,
`int.MinValue/MaxValue`, tümü sıfır, saf rastgele veri.

Özel float değerleri de bit bit korunuyor:

```
gurultulu       : TAM AYNI (bit bit)
duragan         : TAM AYNI (bit bit)
duzgun          : TAM AYNI (bit bit)
ozel degerler   : TAM AYNI (bit bit)   <- NaN, -0.0, ±sonsuz, epsilon, MaxValue
```

NaN karşılaştırması `==` ile yapılamadığı için doğrulama **bit deseni** üzerinden yapılır.

---

## 7. Bilinen boşluklar

Testlerin kapsamadığı yerler — açıkça:

1. **ARM / big-endian donanımda çalıştırma.** Kod derleniyor, davranış ölçülmedi.
2. **RTOS üzerinde WCET analizi.** Gecikme dağılımı genel amaçlı işletim sisteminde
   ölçüldü; gerçek en-kötü-durum garantisi yok.
3. **Sertifikalı MISRA aracı.** Açık kaynak Cppcheck temiz geçiyor; nitelikli araç
   raporu yok.
4. **Paket kaybı altında uçtan uca sistem testi.** Kurtarma oranı simülasyonla ölçüldü
   ([KAYIP_DAYANIKLILIK.md](KAYIP_DAYANIKLILIK.md)), gerçek telsizle değil.
5. **Rakiplerin SIMD kütüphaneleri.** Kıyastaki rakipler yeniden yazılmış skaler
   sürümlerdir; hız iddiası bu yüzden **kurulmamıştır**.
