# Kullanım

> Katmanların ne işe yaradığı: [MIMARI.md](MIMARI.md) ·
> Bayt düzeyindeki tanım: [BICIM_SPESIFIKASYONU.md](BICIM_SPESIFIKASYONU.md)

---

## ⚠️ Önce şunu okuyun: hangi katmanı kullanmalısınız?

Bütünlük kontrolü **yalnızca çerçeve katmanındadır.** Bu bilinçli bir tercihtir — alt
katmanlar sıcak yolda çalışır ve zaten doğrulanmış veri üzerinde işlem yapmaları
beklenir; sağlama toplamını her katmanda tekrarlamak gereksiz maliyet olurdu.

Ama bunun bir sonucu vardır:

| Katman | Bütünlük kontrolü | Güvenilmeyen veriye uygulanır mı? |
| --- | --- | --- |
| `elbari_basit` (çekirdek) | Yapısal tüketim kontrolü, sağlama toplamı yok | ⚠️ Tercih edilmez |
| `elbari_kanal_basit` (kanal) | Yalnızca başlık tutarlılığı | ❌ Hayır |
| `elbari_cerceve_oku` (çerçeve) | ✅ CRC32 | ✅ Evet |

Çekirdek çözücüye bozuk bayt verilirse **hata döndürmeyebilir** — bit akışını olduğu
gibi yorumlar ve anlamsız veri üretir. Bu bir güvenlik açığı değildir (tampon taşmaz,
süreç çökmez), ama **sessizce yanlış veri** demektir.

> ### KURAL
> Güvenilmeyen kaynaktan (telsiz linki, ağ, disk) gelen veri **daima çerçeve
> katmanından** geçirilmelidir.

Aynı uyarı [`c/src/elbari.h`](../c/src/elbari.h) başında da yer alır.

---

## C# kullanımı

### Tek akış (çekirdek)

```csharp
using ElBâri;

int[] veri = { 100, 102, 103, 105, 200, 201 };
byte[] cikti = new byte[veri.Length * 4 + 16];

int n = ElBâri.ElKâbıd(veri, cikti);   // n < 0  =>  veri sıkıştırılamaz
if (n > 0)
{
    int[] geri = new int[veri.Length];
    ElBâri.ElBâsıt(new ReadOnlySpan<byte>(cikti, 0, n), geri);
}
```

> `ElBâsıt`'a sıkıştırılmış verinin **tam boyutu** verilmelidir. Çözücü verinin nerede
> bittiğini kendi başına bilemez; fazla büyük bir dilim verilirse akış reddedilir
> (yapısal tüketim kontrolü).

### Çok kanallı telemetri (kanal katmanı)

```csharp
int kanal = 3;                                  // enlem, boylam, irtifa
int[] calisma = new int[ElBâriKanal.GerekliCalismaAlani(veri.Length, kanal)];
byte[] cikti  = new byte[ElBâriKanal.EnKotuDurumCiktiBoyutu(veri.Length, kanal)];

int n = ElBâriKanal.ElKâbıdKanal(veri, kanal, calisma, cikti);
ElBâriKanal.ElBâsıtKanal(new ReadOnlySpan<byte>(cikti, 0, n), calisma, geri);
```

### Kayıplı link (çerçeve katmanı)

```csharp
// Gönderen: her çerçeve bağımsız bir pakettir
int n = ElBâriÇerçeve.CerceveYaz(kayitDilimi, kanal, siraNo, calisma, paket);

// Alıcı: bozuk/eksik paket false döner ve ATILIR — kalanları etkilemez
bool tamam = ElBâriÇerçeve.CerceveOku(gelenPaket, kanal, calisma, geri,
                                      out uint siraNo, out int kayitSayisi);
```

### Ondalıklı veri (float katmanı)

```csharp
float[] olcekler = { 1000f, 1000f, 1000f, 100f, 100f, 100f };  // kanal başına
int[] tamsayi = new int[veri.Length];

ElBâriFloat.KuantalaKanalli(veri, kanal, olcekler, tamsayi);
int n = ElBâriKanal.ElKâbıdKanal(tamsayi, kanal, calisma, cikti);
// ...
ElBâriFloat.CozKanalli(geriTamsayi, kanal, olcekler, geriFloat);
```

`olcekler` **biçim içinde taşınmaz**; iki taraf da aynı diziyi bilmek zorundadır.

---

## C kullanımı

```c
#include "elbari.h"

int32_t kanal = 3;                 /* enlem, boylam, irtifa */
int32_t calisma[ELBARI_CALISMA];   /* elbari_kanal_gerekli_calisma_alani(...) */
uint8_t cikti[ELBARI_CIKTI];       /* elbari_kanal_en_kotu_durum_boyutu(...) */

int32_t n = elbari_kanal_kabid(kayitlar, eleman_sayisi, kanal,
                               calisma, ELBARI_CALISMA,
                               cikti, ELBARI_CIKTI);
if (n < 0) { /* hata kodu: ELBARI_HATA_* */ }

/* Kayıplı link: her çerçeve bağımsız gönderilir ve bağımsız çözülür */
int32_t paket_boyu = elbari_cerceve_yaz(kayit_dilimi, adet * kanal, kanal, sira_no,
                                        calisma, ELBARI_CALISMA,
                                        paket, ELBARI_PAKET);

int32_t durum = elbari_cerceve_oku(gelen_paket, gelen_boyut, kanal,
                                   calisma, ELBARI_CALISMA,
                                   geri, ELBARI_GERI, &sira_no, &kayit_sayisi);
/* durum != ELBARI_TAMAM  =>  paket bozuk/eksik, atılmalı */
```

### Tampon boyutları — asla tahmin etmeyin

Kütüphane **hiç bellek ayırmaz**; bütün tamponları çağıran verir. Boyutları hesaplayan
işlevler vardır ve bunlar kullanılmalıdır:

| İşlev | Ne döner |
| --- | --- |
| `elbari_cekirdek_en_kotu_durum_boyutu(eleman)` | Çekirdek çıktısı için güvenli en kötü durum |
| `elbari_kanal_en_kotu_durum_boyutu(eleman, kanal)` | Kanal çıktısı için |
| `elbari_kanal_gerekli_calisma_alani(eleman, kanal)` | Çalışma alanı (int cinsinden) |
| `elbari_cerceve_en_kotu_durum_boyutu(kayit, kanal)` | Çerçeve çıktısı için |
| `elbari_cerceve_gerekli_calisma_alani(kayit, kanal)` | Çerçeve çalışma alanı |

### Dönüş kodları

| Kod | Anlamı |
| --- | --- |
| `ELBARI_TAMAM` (0) | Başarılı |
| `ELBARI_SIKISTIRILAMAZ` (−1) | **Hata değil.** Veri sıkıştırılamaz; çağıran ham göndermeyi seçmelidir |
| `ELBARI_HATA_TAMPON_KUCUK` (−2) | Verilen tampon yetersiz |
| `ELBARI_HATA_PARAMETRE` (−3) | NULL işaretçi, aralık dışı kanal sayısı vb. |
| `ELBARI_HATA_BOZUK_GIRDI` (−4) | Girdi bozuk ya da beklenen biçimde değil |

`ELBARI_SIKISTIRILAMAZ`'ı hata gibi ele almayın — rastgele/gürültülü veride beklenen
cevaptır ve kanal katmanı zaten ham geçişle bunu kendi içinde halleder.

---

## Derleme

### Windows (MSVC)

```bat
c\derle.bat            :: kütüphane + test programları
c\kiyas\derle.bat      :: kıyas takımı
c\mavlink\derle.bat    :: MAVLink vekili ölçümü
c\veri\derle.bat       :: ArduPilot log dönüştürücü
c\analiz.bat           :: MSVC /Wall /analyze statik analiz
```

### Linux / macOS

```bash
cd c
make                 # tüm test programları
make CC=clang        # clang ile
make sanitize        # ASan + UBSan sürümleri
make test            # derle + uygunluk + iki fuzz + kodek öz-testi
make temizle
```

### .NET

```bash
dotnet build
dotnet run --configuration Debug     # 32 senaryoluk test paketi
```

---

## Ölçümü kendiniz çalıştırın

```bash
c\uygunluk.exe   testverisi\vektorler.txt        # 29 dondurulmuş biçim vektörü
c\deger_fuzz.exe 300000                          # kodlayıcı tam turu
c\fuzz.exe       300000                          # çözücü sağlamlığı
c\kayip.exe      testverisi\gercek_gps.bin 250   # kayıp altında kurtarma
c\kiyas\kiyas.exe testverisi\gercek_gps.bin 200  # tamsayı kodek ailesiyle kıyas
c\dogrulama.exe  <referans_dizini>               # .NET ile ikili uyumluluk
c\olcum.exe      <referans_dizini>               # verim + gecikme dağılımı
```

Gerçek uçuş verisiyle çalışmak için önce fikstür üretin
([`c/veri/`](../c/veri/)):

```bash
c\veri\donustur.exe "<ArduPilot .bin log>" --dok   # logda ne var, listele
c\veri\donustur.exe "<ArduPilot .bin log>"         # alfa_*.bin fikstürleri
```

MAVLink vekili ölçümü, gerçek yönelim/IMU verisini de alabilir:

```bash
c\mavlink\mav_olcum.exe alfa_gps.bin 300 --att alfa_att.bin --imu alfa_imu.bin
```
