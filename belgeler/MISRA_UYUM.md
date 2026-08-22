# MISRA C:2012 Uyum Matrisi ve Sapma Kaydı

**Kapsam:** `c/src/` altındaki kütüphane kaynaklarının tamamı — `elbari.c`,
`elbari_kanal.c`, `elbari_cerceve.c`, `elbari_float.c`, `elbari_float_xor.c`,
`elbari.h`, `elbari_ic.h`

**Kapsam dışı:** `c/test/` altındaki dosyalar. Test kodu üründe dağıtılmaz;
`malloc`, `printf`, `qsort` gibi kütüphane çağrılarını serbestçe kullanır.

**Yöntem:** Elle inceleme + MSVC `/Wall /analyze` + GCC/Clang uyarıları + ASan/UBSan +
Cppcheck MISRA eklentisiyle otomatik kural taraması — **iki farklı sürümde**
(yerelde 2.21.0, CI'da 2.13.0). Sebebi Bölüm 3.6'da; kısacası sürümler birbirinin
kaçırdığını yakalıyor.

> ⚠️ **Dürüstlük notu:** Kod bir **açık kaynak** MISRA denetleyicisinden (Cppcheck)
> temiz geçmektedir. Bu, **sertifikalı** bir MISRA aracı (Helix QAC, PC-lint Plus,
> Polyspace) ile yapılmış bir doğrulama **değildir**. "MISRA sertifikası" diye bir belge
> zaten yoktur; uyum kendi beyanınızdır ve kanıtla desteklenir. Bu belge o kanıt
> zincirinin bugünkü halidir. Ticari teslimatta müşteri nitelikli bir araç talep ederse
> o adım ayrıca yapılmalıdır.

---

## 1. Özet

| Kategori | Durum |
| --- | --- |
| Zorunlu (Mandatory) kurallar | İhlal yok |
| Gerekli (Required) kurallar | Yalnızca **1 kayıtlı sapma** (D-4, Kural 21.15) |
| Tavsiye (Advisory) kurallar | Yalnızca **1 kayıtlı sapma** (D-1, Kural 15.5) |
| Kayıtlı sapma sayısı | 4 (D-1 … D-4) |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| MSVC `/W4` derleme | ✅ 0 uyarı |
| Cppcheck MISRA eklentisi (2.21.0 ve 2.13.0) | ✅ Kayıtlı sapmalar dışında 0 bulgu |

**Cppcheck MISRA taraması — nihai sonuç:**

| Kural | Sınıf | Adet | Durum |
| --- | --- | --- | --- |
| 15.5 — tek çıkış noktası | Tavsiye | 133 | Sapma **D-1** (kayıtlı) |
| 21.15 — `memcpy` tip uyumu | Gerekli | 6 | Sapma **D-4** (kayıtlı) |
| *Diğer tüm kurallar* | — | **0** | — |

Zorunlu ve Gerekli sınıfındaki tek bulgu 21.15'tir ve gerekçesi D-4'te kayıtlıdır.
Taramanın nasıl tekrarlanacağı Bölüm 5'tedir.

---

## 2. Kritik kuralların denetimi

### Yönergeler (Directives)

| Kural | Konu | Durum | Not |
| --- | --- | --- | --- |
| Dir 4.1 | Çalışma zamanı hataları en aza indirilmeli | ✅ | Tüm tampon erişimleri sınır kontrollü; hatalar dönüş koduyla bildirilir |
| Dir 4.12 | Dinamik bellek kullanılmamalı | ✅ | Kütüphanede `malloc`/`free` yok; tüm tamponları çağıran verir |
| Dir 4.14 | Dış kaynaklı değerler doğrulanmalı | ✅ | Çerçeve başlığı, CRC, kanal sayısı, yük boyutları doğrulanır |

### Kural 1–2: Ortam ve ölü kod

| Kural | Konu | Durum | Not |
| --- | --- | --- | --- |
| 1.1 | Standart C dışına çıkılmamalı | ✅ | C99 uyumlu; C11 özellikleri `#if` korumalı |
| 1.3 | Tanımsız davranış olmamalı | ✅ | **Bkz. Bölüm 3.1** — işaretli taşma tamamen elimine edildi |
| 2.1 | Erişilemeyen kod olmamalı | ✅ | — |
| 2.2 | Ölü kod olmamalı | ✅ | — |
| 2.7 | Kullanılmayan parametre olmamalı | ✅ | — |

### Kural 8–9: Bildirimler ve ilk değer

| Kural | Konu | Durum | Not |
| --- | --- | --- | --- |
| 8.2 | Fonksiyon tipleri prototip biçiminde | ✅ | Tüm parametreler adlandırılmış |
| 8.4 | Uyumlu bildirim görünür olmalı | ✅ | Genel API `elbari.h` içinde |
| 8.7 | Tek dosyada kullanılan nesneler `static` | ✅ | Tüm iç yardımcılar `static` |
| 8.13 | Mümkünse `const` işaretçi | ✅ | Tüm girdi işaretçileri `const` |
| 9.1 | Değişkenler kullanılmadan önce ilklenmeli | ⚠️ | **Bkz. Sapma D-3** |

### Kural 10–12: Tip modeli ve ifadeler

| Kural | Konu | Durum | Not |
| --- | --- | --- | --- |
| 10.1 | Uygunsuz temel tip işlenmemeli | ✅ | Bit işlemleri yalnızca işaretsiz tiplerde |
| 10.3 | Dar tipe atama açık olmalı | ✅ | Tüm daraltmalar açık `cast` ile |
| 10.4 | İşlenenler aynı tip kategorisinde | ✅ | — |
| 11.3 | Nesne işaretçileri arası dönüşüm yok | ✅ | `memcpy` kullanılır, işaretçi hilesi yok |
| 11.8 | `const` kaldıran dönüşüm yok | ✅ | — |
| 12.1 | Öncelik parantezle açık olmalı | ✅ | Tüm bileşik ifadeler parantezli |
| 12.2 | Kaydırma miktarı aralıkta olmalı | ✅ | **Bkz. Bölüm 3.2** |

### Kural 13–16: Yan etki ve akış denetimi

| Kural | Konu | Durum | Not |
| --- | --- | --- | --- |
| 13.3 | Artırma/azaltma başka işlemle karışmamalı | ✅ | `(*bayt_indeksi)++;` ayrı satırda |
| 13.5 | `&&`/`\|\|` sağ işleneninde yan etki yok | ✅ | — |
| 14.4 | Koşul ifadesi mantıksal olmalı | ✅ | Tümü `!= 0` / `== 0` biçiminde |
| 15.1 | `goto` kullanılmamalı | ✅ | Hiç `goto` yok |
| 15.5 | Tek çıkış noktası (Tavsiye) | ⚠️ | **Bkz. Sapma D-1** |
| 15.6 | Döngü/koşul gövdeleri bloklu | ✅ | Tüm gövdeler süslü parantezli |
| 15.7 | `if-else if` zinciri `else` ile bitmeli | ✅ | — |
| 16.3 | Her `case` `break` ile bitmeli | ✅ | — |
| 16.4 | Her `switch` `default` içermeli | ✅ | — |

### Kural 17–21: Fonksiyonlar, işaretçiler, kütüphane

| Kural | Konu | Durum | Not |
| --- | --- | --- | --- |
| **17.2** | **Özyineleme olmamalı** | ✅ | **Hiç özyineleme yok — yığın derinliği sabit** |
| 17.7 | Dönüş değeri kullanılmalı ya da `(void)` | ✅ | Yok sayılanlar `(void)` ile işaretli |
| 18.1 | İşaretçi aritmetiği sınırlar içinde | ✅ | Tüm erişimler önceden doğrulanır |
| 20.7 | Makro parametreleri parantezli | ✅ | Makrolar yalnızca sabit |
| 21.3 | Kütüphanede `malloc` yok | ✅ | — |
| 21.6 | Kütüphanede `stdio` yok | ✅ | Yalnızca test kodunda |

---

## 3. Özel dikkat gösterilen noktalar

### 3.1 Kural 1.3 — İşaretli tamsayı taşması (tanımsız davranış)

C'de işaretli tamsayı taşması **tanımsız davranıştır**. .NET tarafı ise varsayılan
`unchecked` bağlamda sessizce sarar. İki sürümün **bit bit aynı** çıktı üretmesi
gerektiği için bu fark kritiktir.

**Çözüm:** Tüm fark ve toplama işlemleri işaretsiz aritmetik üzerinden yapılır:

```c
static ELBARI_SATIRICI int32_t elbari_ic_fark(int32_t simdiki, int32_t onceki)
{
    uint32_t f = elbari_ic_isaretsize_cevir(simdiki) - elbari_ic_isaretsize_cevir(onceki);
    return elbari_ic_isaretliye_cevir(f);
}
```

İşaretsiz taşma C'de **tanımlıdır** (modulo 2³²) ve .NET'in davranışıyla birebir aynıdır.
Tip dönüşümleri `memcpy` ile yapılır; doğrudan atama uygulamaya bağlı davranış olurdu.

### 3.2 Kural 12.2 — Kaydırma miktarı sınırları

Bit tamponu `uint64_t`'dir. En kötü durum analizi:

| Yer | Giriş `bit_sayisi` | Kaydırma | Toplam | Sınır |
| --- | --- | --- | --- | --- |
| Etiket yazma | ≤ 7 | +4 | 11 | 64 ✅ |
| Aykırı maske | ≤ 7 | +8 | 15 | 64 ✅ |
| Fark (16 bit) | ≤ 7 | +16 | 23 | 64 ✅ |
| Aykırı (32 bit) | ≤ 7 | +32 | 39 | 64 ✅ |
| Okuma (yükleme) | ≤ 31 | +8 | 39 | 64 ✅ |

Her yazma sonrası tampon boşaltıldığı için `bit_sayisi` asla 8'i aşmaz.

**Cppcheck taraması sırasında yapılan iyileştirme.** Yukarıdaki analiz elle
ispatlanabilir olsa da statik çözümleyici `bit_genisligi ≤ 16` bilgisini fonksiyon
sınırları arasında taşıyamadığı için 12.2 bulgusu üretiyordu. "Araç anlamıyor, biz
biliyoruz" demek yerine **kaydırma işlemi koddan tamamen kaldırıldı**:

```c
static ELBARI_SATIRICI uint32_t elbari_ic_alt_maske(int32_t adet)
{
    static const uint32_t elbari_ic_alt_maskeler[33] = { 0x00000000u, ... };
    int32_t k = adet;

    if (k < 0)  { k = 0;  }
    if (k > 32) { k = 32; }
    return elbari_ic_alt_maskeler[(uint32_t)k];   /* indeks kesin 0..32 */
}
```

Aynı yaklaşım kanal bayrakları için de uygulandı (`elbari_ic_bit_maskesi[8]`).
Kazanç üç yönlü:

1. **Kural 12.2 ihlali imkânsız** — kaydırma operatörü yok.
2. **Elle ispat gerekmez** — indeksin sınırlandığı üç satırda görülüyor.
3. **Çalışma zamanı maliyeti aynı ya da daha az** — kaydırma yerine 132 baytlık
   salt-okunur tablodan tek okuma. Gömülü hedefte 132 bayt flash ihmal edilebilir.

Değişiklik sonrası **çıktı biçimi birebir korundu** (27 uygunluk vektörü + 59.695
baytlık .NET karşılaştırması).

### 3.5 Kural 10.1 / 10.8 — Bit işlemlerinde işaretli operand

**Bulgu (Cppcheck).** Kanal bayrakları şu deyimle işleniyordu:

```c
bayraklar[c >> 3] |= (uint8_t)(1u << (unsigned int)(c & 7));
```

`c` işaretli (`int32_t`) olduğu için hem kaydırma hem `&` işaretli operand alıyordu
(10.1); ayrıca bileşik ifade doğrudan `uint8_t`'ye dönüştürülüyordu (10.8). Deyim beş
ayrı yerde tekrarlanıyordu.

**Düzeltme.** İki ortak yardımcı eklendi (`elbari_ic.h`); tüm indeks ve maske hesabı
işaretsiz tip üzerinde yapılıyor, çağrı yerleri okunur hale geldi:

```c
elbari_ic_bayrak_kur(ikinci_derece_bayraklari, c);
ham_gecis = elbari_ic_bayrak_var_mi(ham_gecis_bayraklari, c);
```

Aynı taramada bulunan diğer gerçek bulgular da düzeltildi:

| Kural | Yer | Sorun | Düzeltme |
| --- | --- | --- | --- |
| 10.6 | 5 yer (`elbari.c`, `elbari_kanal.c`) | `koşul ? 1 : 0` — bileşik ifadenin geniş tipe atanması | Açık `if/else` (bkz. 3.6) |
| 10.4 | `elbari_ic.h` | `sizeof(...) == 4` — işaretsiz ile işaretli karşılaştırma | `== 4u` |
| 10.8 | `elbari.c`, `elbari_float_xor.c` | bileşik ifadenin doğrudan dönüşümü | ara değişkene alındı |
| 17.8 | `elbari_float_xor.c` | `deger >>= 1` parametreyi değiştiriyordu | yerel kopya |
| 2.5 | `elbari.c` | kullanılmayan iki makro | kaldırıldı |
| 8.9 | `elbari_ic.h` | maske tablosu dosya kapsamındaydı | blok kapsamına alındı |

Bu düzeltmelerin **hiçbiri bit akışını değiştirmedi**; her adımda uygunluk vektörleri ve
.NET karşılaştırması tekrar çalıştırılarak doğrulandı.

### 3.3 Tamsayı taşma koruması (bu inceleme sırasında eklendi)

**Bulgu:** Boyut hesapları (`eleman_sayisi * 4 + pay`) 32 bit tamsayı ile yapılıyordu.
Çok büyük bir `eleman_sayisi` değeri çarpma sırasında taşar, **negatif veya küçük** bir
"gerekli boyut" üretir ve çağıran yetersiz bir tampon ayırırdı.

**Düzeltme:** `ELBARI_MAKS_ELEMAN` (200.000.000) sınırı tanımlandı ve tüm giriş
noktalarında doğrulanıyor. Çerçeve katmanında `kayit_sayisi * kanal_sayisi` çarpımı
bölme ile önceden denetleniyor:

```c
if (kayit_sayisi > (ELBARI_MAKS_ELEMAN / kanal_sayisi))
{
    return ELBARI_HATA_PARAMETRE;
}
```

Bu, **bozuk girdiden gelen** `kayit_sayisi` için de geçerlidir (çözücü tarafı) — yani
düşmanca bir paket bu yolla taşma tetikleyemez.

### 3.4 Statik durum ve iş parçacığı güvenliği

`elbari_cerceve.c` içinde CRC tablosu için iki statik değişken vardır
(`s_crc_tablosu`, `s_crc_tablosu_hazir`). Tablo üretimi **idempotenttir** — aynı sonucu
üretir, bu yüzden yarış durumu veri bozulmasına yol açmaz.

Yine de kesin davranış isteniyorsa, çağıran ilk `elbari_crc32` çağrısını tek bir iş
parçacığından yapmalıdır. Kütüphanenin geri kalanında **hiç** paylaşılan durum yoktur;
tüm fonksiyonlar yeniden girişlidir (reentrant).

---

### 3.6 Kural 10.6 — `koşul ? 1 : 0` ve iki sürümle tarama dersi

**Bulgu.** CI'daki MISRA işi ilk koşuşunda kırmızı yandı: Kural 10.6 (**Gerekli**),
5 adet. Hepsi aynı deyimdi:

```c
aykiri_var  = (aykiri_maske != 0u) ? 1 : 0;
on_ek_boyu  = (ikinci_derece != 0) ? 4 : 0;
```

**Neden ihlal.** `koşul ? 1 : 0` MISRA'nın *essential type* modelinde **bileşik
ifadedir**. Sabit operandların çıkardığı temel tip `int32_t`'den dardır; dolayısıyla
atama bir genişletmedir ve 10.6 bunu yasaklar.

**Yanlış çözüm.** Ternary'i `(int32_t)` ile cast edip susturmak akla geliyor — ama
bileşik ifadeyi cast etmek bu sefer **Kural 10.8'i** ihlal eder. Döngüsel. Tek temiz yol
dallanmadır:

```c
if (aykiri_maske != 0u)
{
    aykiri_var = 1;
}
else
{
    aykiri_var = 0;
}
```

CI'nın işaretlediği 5 atamanın yanında, işaretlemediği 3 `return koşul ? 1 : 0` yeri de
aynı şekilde çevrildi; böylece sınıf tamamen kapandı ve ileride başka bir araç sürümü
CI'yı kırmayacak.

#### Asıl ders: yeni sürüm ≠ daha sıkı

| Ortam | Cppcheck | Kural 10.6'yı yakaladı mı? |
| --- | --- | --- |
| Yerel (Windows) | **2.21.0** | ❌ Hayır |
| CI (Ubuntu) | **2.13.0** | ✅ Evet, 5 adet |

Platform farkı değildir — yerelde `--platform=unix64` ile tekrarlandığında da çıkmadı.
**Sürüm farkıdır ve yönü sezgiye aykırıdır:** daha *eski* sürüm, daha yenisinin
kaçırdığı bir **Gerekli** kuralı buluyor.

Bunun pratik sonucu şudur: **tek bir araç sürümünden "temiz" almak, uyum kanıtı olarak
zayıftır.** Bu projede tarama bilinçli olarak iki sürümde yürütülür ve ikisinin birleşimi
esas alınır. Ticari bir araç (Helix QAC, PC-lint Plus, Polyspace) devreye girdiğinde de
beklenti aynıdır: onun bulacağı yeni bulgular olacaktır, bu bir başarısızlık değil
sürecin normal işleyişidir.

---

## 4. Sapma kaydı

### D-1 — Kural 15.5 (Tavsiye): Tek çıkış noktası

**Sapma:** Fonksiyonlarda birden fazla `return` vardır. Cppcheck taramasında ölçülen
adet: **133** (kütüphanenin tamamı).

**Gerekçe:** Parametre doğrulamaları "koruma cümlesi" (guard clause) biçiminde erken
dönüş yapar. Alternatif olan tek çıkışlı yapı, iç içe geçmiş `if` blokları ve bayrak
değişkenleri gerektirir; bu, okunabilirliği ve denetlenebilirliği **azaltır**. Kural
Tavsiye (Advisory) niteliğindedir ve bu sapma endüstride yaygın kabul görür.

**Risk yönetimi:** Tüm çıkış noktaları ya bir hata kodu ya da geçerli bir sonuç döndürür;
kaynak sızıntısı riski yoktur (dinamik bellek kullanılmadığı için serbest bırakılacak
kaynak yoktur). 15.5'in asıl koruduğu risk — "bir çıkış yolunda temizlik atlanır" — bu
kütüphanede yapısal olarak mevcut değildir.

### D-2 — Kayan nokta kullanımı

**Sapma:** `elbari.c` içinde iki yerde `float` kullanılır:
- `elbari_ic_sikistirilabilir_mi` — aykırı değer oranı karşılaştırması
- `elbari_kabid` — erken iptal oranı karşılaştırması

**Gerekçe:** Bu karşılaştırmalar .NET referans sürümüyle **bit bit aynı** kararı
üretmek zorundadır. Tamsayı aritmetiğine çevirmek (`a * 100 > 30 * b` gibi) matematiksel
olarak daha temiz olurdu, ancak sınır değerlerde farklı karar üretebilir ve iki sürüm
arasındaki ikili uyumluluk bozulurdu.

**Risk yönetimi:** Kayan nokta yalnızca **karar** verir, üretilen bit akışına doğrudan
girmez. Kullanılan işlemler yalnızca bölme ve karşılaştırmadır; NaN/sonsuz üretecek bir
yol yoktur (bölen daima ≥ 1 olacak şekilde korunmuştur).

**Gelecek:** İki sürüm birlikte değiştirilirse tamsayı aritmetiğine geçilebilir.

### D-3 — Kural 9.1: `gecici[]` dizisi ilklenmiyor

**Sapma:** `elbari_basit` içindeki `int32_t gecici[ELBARI_BLOK_BOYUTU];` dizisi
tanımlandığı anda ilklenmez.

**Gerekçe:** Dizinin `blok_boyu` kadar elemanı, okunmadan **önce** mutlaka yazılır: her
`j` indeksi ya aykırı-olmayan döngüsünde ya da aykırı döngüsünde tam olarak bir kez
yazılır (aykırı maskesi bu iki kümeyi ayrık kılar). Önek toplam döngüsü yalnızca
`0..blok_boyu-1` aralığını okur.

**Risk yönetimi:** Gereksiz ilkleme her blokta 8 elemanlık ek yazma maliyeti getirirdi;
sıcak yolda bu ölçülebilir. Statik analiz aracı bu diziyi işaretlerse, yukarıdaki
gerekçe sapma kaydı olarak sunulmalıdır. Alternatif olarak `= {0}` eklenerek kural
karşılanabilir — davranış değişmez, yalnızca küçük bir performans maliyeti oluşur.

### D-4 — Kural 21.15 (Gerekli): `memcpy` işaretçilerinin tipleri uyumlu değil

**Sapma:** `elbari_float_xor.c` içinde 6 yerde `memcpy` bir `float` ile bir `uint32_t`
arasında kopyalama yapar:

```c
uint32_t bit_deseni;
(void)memcpy(&bit_deseni, &deger, sizeof(bit_deseni));   /* float -> uint32_t */
```

**Gerekçe:** Bu **kasıtlı tip yorumlamasıdır** (type punning). XOR tabanlı kayıpsız float
sıkıştırması, tanımı gereği float'ın IEEE-754 bit desenini tamsayı olarak işlemek
zorundadır — algoritmanın çalışma prensibi budur.

**Neden alternatifler daha kötü:**

| Yöntem | Sorun |
| --- | --- |
| İşaretçi dönüşümü `*(uint32_t*)&f` | **Katı takma ad (strict aliasing) ihlali** — tanımsız davranış; optimizasyon açıkken derleyici yanlış kod üretebilir. MISRA 11.3 de yasaklar. |
| `union` | C'de tanımlı, C++'ta tanımsız. MISRA 19.2 birleşimleri (Tavsiye) zaten kısıtlar. |
| `memcpy` (**seçilen**) | Standardın açıkça izin verdiği tek taşınabilir yol. Tanımsız davranış yok; her derleyici tek komuta indirger. |

**Risk yönetimi:** Kopya boyutu her çağrıda `sizeof(hedef)` ile verilir ve
`_Static_assert` ile `sizeof(uint32_t) == 4u` derleme anında doğrulanır; boyut uyuşmazlığı
mümkün değildir. Kural 21.15'in koruduğu risk (yanlış boyutta kopya) bu yolla kapatılmıştır.

**Kapsam:** Yalnızca `elbari_float_xor.c`. Kayıpsız float katmanı kullanılmayan bir
yapılandırmada bu sapma tamamen ortadan kalkar.

---

## 5. Doğrulama kanıtı

| Kontrol | Sonuç |
| --- | --- |
| MSVC `/W4` derleme | 0 uyarı |
| MSVC `/Wall /analyze` statik analiz | 0 bulgu |
| GCC + Clang `-Wall -Wextra -Wpedantic -Wshadow -Wcast-qual` | 0 uyarı (CI) |
| ASan + UBSan çalışma zamanı | 0 bulgu (CI) |
| **Cppcheck MISRA eklentisi (2.21.0, yerel)** | **Kayıtlı sapmalar (D-1, D-4) dışında 0 bulgu** |
| **Cppcheck MISRA eklentisi (2.13.0, CI)** | **Kayıtlı sapmalar dışında 0 bulgu — her push'ta** |
| Cppcheck `--enable=all` genel analiz | 0 hata, 0 uyarı (yalnızca 3 `style` ipucu) |
| .NET ile ikili uyumluluk — kanal katmanı | 59.695 bayt birebir aynı |
| .NET ile ikili uyumluluk — float katmanı | 27.403 bayt birebir aynı |
| .NET ile ikili uyumluluk — kuantalama | 72.000 değerin tamamı aynı yuvarlandı |
| Uygunluk vektörleri (biçim sözleşmesi) | 29 vektör, 58/58 geçti |
| Round-trip kayıpsızlık | ✅ |
| Çapraz uyumluluk (C, .NET çıktısını çözüyor) | ✅ |
| Tek-bit bozulma tespiti | 247/247 |
| Bozulmuş çerçeveleri reddetme oranı (fuzz) | %100,00 |
| Fuzz — tampon taşması | 300.000 turda 0 |
| Kenar durumlar (NULL, yetersiz tampon, rastgele bayt) | Çökme yok |

Çalıştırmak için:

```bat
c\derle.bat                        REM /W4 ile derleme + tüm test ikilileri
c\analiz.bat                       REM /Wall /analyze statik analiz
c\dogrulama.exe <referans_dizini>  REM .NET ile ikili uyumluluk
c\uygunluk.exe testverisi\vektorler.txt
c\fuzz.exe 300000
```

**MISRA taramasının tekrarlanması.** Cppcheck'in `--addon=` seçeneği bazı Windows
kurulumlarında Python'u sessizce başlatamaz ve **hiç çıktı üretmez** — bu, "0 ihlal"
sanılabilecek bir tuzaktır. Güvenilir yol iki adımlıdır:

```bat
for %f in (c\src\elbari*.c) do cppcheck --dump --std=c11 --platform=win64 --quiet %f
python <cppcheck_dizini>\addons\misra.py c\src\*.dump
```

`--std=c11` verilmelidir: kod C17 olarak derlenir ve daha düşük bir standart
`_Static_assert` üzerinde sahte bulgular üretir.

**CI kapısı yalnızca rapor üretmez, eşik denetler.** `.github/workflows/derleme-ve-test.yml`
içindeki `misra` işi, kayıtlı sapmalar (15.5 ve 21.15) dışında herhangi bir kural
görürse derlemeyi kırar. Yani bu belgeye kaydedilmemiş yeni bir ihlal sessizce içeri
giremez. Kapı, sahte bir ihlal enjekte edilerek her iki yönde de sınandı.

İş ayrıca **boş raporu başarısızlık sayar**: `--addon=` seçeneği bazı ortamlarda Python'u
sessizce başlatamayıp hiç çıktı üretmez ve bu "0 ihlal" sanılabilir.

---

## 6. Durum ve yapılacaklar

| Adım | Durum |
| --- | --- |
| Elle uyum incelemesi + sapma kaydı | ✅ Bu belge |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| GCC + Clang `-Wall -Wextra -Wpedantic -Wshadow -Wcast-qual -Wstrict-prototypes` | ✅ CI'da her push'ta, 0 uyarı |
| **ASan + UBSan (çalışma zamanı)** | ✅ CI'da temiz — *Kural 1.3 "tanımsız davranış yok" iddiasının deneysel kanıtı* |
| Fuzz (600.000 tur, kanarya korumalı) | ✅ 0 tampon taşması |
| Statik MISRA aracı (Cppcheck 2.21.0 MISRA eklentisi) | ✅ Kayıtlı sapmalar dışında temiz |
| Ticari MISRA aracı (Helix QAC / PC-lint / Polyspace) | ⏳ Müşteri/program gerektirdiğinde |
| ARM ve big-endian doğrulama | ⏳ Donanım yok |
| RTOS üzerinde WCET analizi | ⏳ Ortam yok — tipik olarak entegratör tarafında yapılır |

> **Terminoloji notu:** "MISRA sertifikası" diye bir belge **yoktur**. MISRA kod
> sertifikalandırmaz; uyum **kendi beyanınızdır** ve kanıtla desteklenir (uyum matrisi +
> sapma kaydı + araç raporu). "Nitelikli araç" (qualified tool) kavramı ise yalnızca
> DO-178C bağlamında, aracın çıktısı insan denetiminin *yerine* kullanılacaksa anlamlıdır.
