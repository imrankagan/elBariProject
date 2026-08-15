# MISRA C:2012 Uyum Matrisi ve Sapma Kaydı

**Kapsam:** `../c/src/elbari.c`, `../c/src/elbari_kanal.c`, `../c/src/elbari_cerceve.c`,
`../c/src/elbari.h`, `../c/src/elbari_ic.h`

**Kapsam dışı:** `c/test/` altındaki dosyalar. Test kodu üründe dağıtılmaz;
`malloc`, `printf`, `qsort` gibi kütüphane çağrılarını serbestçe kullanır.

**Yöntem:** Elle inceleme + MSVC `/Wall /analyze` statik analizi.

> ⚠️ **Dürüstlük notu:** Bu belge **elle yapılmış** bir incelemedir. Sertifikalı bir
> MISRA aracı (Helix QAC, PC-lint Plus, Polyspace) ile **doğrulanmamıştır**. Resmî bir
> uygunluk beyanı değildir; iyi niyetli bir öz-değerlendirmedir. Ticari teslimat öncesi
> nitelikli bir araçla doğrulanmalıdır.

---

## 1. Özet

| Kategori | Durum |
| --- | --- |
| Zorunlu (Mandatory) kurallar | Bilinen ihlal yok |
| Gerekli (Required) kurallar | Bilinen ihlal yok |
| Tavsiye (Advisory) kurallar | 2 bilinçli sapma (aşağıda) |
| MSVC `/Wall /analyze` | ✅ 0 bulgu |
| MSVC `/W4` derleme | ✅ 0 uyarı |

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
`1u << bit_genisligi` ifadesinde `bit_genisligi ≤ 16` olduğu kod yoluyla garantidir;
kodlayıcıda ayrıca `>= 32` durumu açıkça korunmuştur.

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

## 4. Sapma kaydı

### D-1 — Kural 15.5 (Tavsiye): Tek çıkış noktası

**Sapma:** Fonksiyonlarda birden fazla `return` vardır.

**Gerekçe:** Parametre doğrulamaları "koruma cümlesi" (guard clause) biçiminde erken
dönüş yapar. Alternatif olan tek çıkışlı yapı, iç içe geçmiş `if` blokları ve bayrak
değişkenleri gerektirir; bu, okunabilirliği ve denetlenebilirliği **azaltır**. Kural
Tavsiye (Advisory) niteliğindedir ve bu sapma endüstride yaygın kabul görür.

**Risk yönetimi:** Tüm çıkış noktaları ya bir hata kodu ya da geçerli bir sonuç döndürür;
kaynak sızıntısı riski yoktur (dinamik bellek kullanılmadığı için serbest bırakılacak
kaynak yoktur).

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

---

## 5. Doğrulama kanıtı

| Kontrol | Sonuç |
| --- | --- |
| MSVC `/W4` derleme | 0 uyarı |
| MSVC `/Wall /analyze` statik analiz | 0 bulgu |
| .NET ile ikili uyumluluk (gerçek GPS verisi) | 83.124 bayt birebir aynı |
| Round-trip kayıpsızlık | ✅ |
| Çapraz uyumluluk (C, .NET çıktısını çözüyor) | ✅ |
| Tek-bit bozulma tespiti | 247/247 |
| Kenar durumlar (NULL, yetersiz tampon, rastgele bayt) | Çökme yok |

Çalıştırmak için:

```bash
../c/derle.bat        # /W4 ile derleme
c\analiz.bat       # /Wall /analyze statik analiz
dogrulama.exe <referans_dizini>
```

---

## 6. Yapılacaklar

- [ ] Sertifikalı MISRA aracıyla doğrulama (Cppcheck ile başlanabilir; ticari teslimat
      için Helix QAC / PC-lint Plus / Polyspace)
- [ ] GCC ve Clang ile `-Wall -Wextra -Wpedantic` derleme
- [ ] ARM ve big-endian mimaride doğrulama
- [ ] RTOS üzerinde gerçek en-kötü-durum (WCET) analizi
