# ElBari - C Surumu

.NET surumunun bagimsiz, bagimliliksiz C portu.

## Neden var

Savunma ve gomulu sistemlerde ucan yazilim neredeyse tamamen C'dir.
.NET/Native AOT bir isletim sistemi (Linux) bekler; gercek zamanli
isletim sistemleri (VxWorks, PikeOS, NuttX) ve bare-metal hedefler
.NET calistirmaz. C surumu bu hedeflerin tamamina girer.

Ayrica: sertifikasyon (DO-178C) icin C'nin olgun arac zinciri vardir,
denetim (guvenlik incelemesi) icin duz C okunabilir, ve C ABI'si
kararli oldugu icin her dil bu kutuphaneyi baglayabilir.

## Tasarim kurallari

- Kaynak C99 uyumlu, C17 ile derlenir (C11 ozellikleri yalnizca
  `#if` korumasi arkasinda, istege bagli ek denetim olarak)
- Dinamik bellek YOK - tum tamponlari cagiran verir
- Ozyineleme YOK - yigin derinligi sabit
- Tum donguler sinirli - sonsuz dongu olusamaz
- Istisna yok - hatalar donus kodu ile bildirilir
- Harici bagimlilik yok (yalnizca <stdint.h>, <string.h>)
- Isaretli tasma yok - tum fark hesaplari isaretsiz aritmetik uzerinden
- Bayt duzeni acikca little-endian - big-endian islemcide de ayni bicim

## Dosyalar

    src/elbari.h          Genel API (tek baslik)
    src/elbari_ic.h       Ic yardimcilar (guvenli donusum, bayt duzeni)
    src/elbari.c          Cekirdek: elbari_kabid / elbari_basit
    src/elbari_kanal.c    Kanal katmani (cok kanalli telemetri)
    src/elbari_cerceve.c  Cerceve katmani (paket kaybi dayanikliligi + CRC32)
    test/dogrulama.c      .NET ile ikili uyumluluk dogrulamasi

## Derleme

Windows (MSVC):

    derle.bat

Linux/macOS (gcc veya clang):

    cc -std=c17 -O2 -Wall -Wextra -o dogrulama \
       test/dogrulama.c src/elbari.c src/elbari_kanal.c src/elbari_cerceve.c

## Dogrulama

C surumu, .NET surumuyle BIT BIT AYNI cikti uretmelidir. Referans
ciktilari .NET tarafinda uretilir, sonra:

    dogrulama <referans_dizini>

Gercek GPS verisiyle (24.642 kayit) alinan sonuc:

    kanal: C ciktisi == .NET ciktisi        59695 bayt birebir ayni
    kanal: C round-trip kayipsiz            tum elemanlar birebir geri geldi
    kanal: C, .NET ciktisini cozebiliyor    capraz uyumluluk dogrulandi
    cerceve: yaz/oku bagimsiz ve kayipsiz   247 cerceve, 4.30x
    cerceve: tek-bit bozulma CRC ile        247/247 yakalandi

## MISRA C uyumu

Kod MISRA C:2012 disipliniyle yazilmistir. Uyum matrisi ve sapma kaydi:
../belgeler/MISRA_UYUM.md

Ozet:
  - Zorunlu ve Gerekli kurallarda bilinen ihlal yok
  - 2 bilincli sapma (tek cikis noktasi, kayan nokta) gerekceli olarak kayitli
  - MSVC /Wall /analyze: 0 bulgu
  - MSVC /W4: 0 uyari

Not: Elle yapilmis oz-degerlendirmedir; sertifikali bir MISRA araciyla
henuz dogrulanmamistir.

## Saglamlik (fuzz) testi

Cozucu, dusmanca ve bozuk girdilere karsi 400.000 tur fuzz testinden
gecirildi. Cikti tamponlari "kanarya" desenleriyle cevrelenerek cokmeye
yol acmayan sessiz tasmalar da yakalandi.

    fuzz.exe [tur_sayisi]

Sonuc:
    Toplam tur           : 400.000
    TAMPON TASMASI       : 0
    Bozulmus cerceveler  : %100 reddedildi (99.790/99.790)

Katman kirilimi (butunluk kontrolu tasarimi):
    cekirdek   kabul     17  red 100.568   <- yapisal tuketim kontrolu
    kanal      kabul      0  red 199.625   <- baslik tutarlilik kontrolu
    cerceve    kabul      0  red  99.790   <- CRC32 KORUMALI

YAPISAL TUKETIM KONTROLU:
Gecerli bir akis girdinin tamamini tuketir. Geriye artik kalmissa girdi
reddedilir. Maliyeti tek bir karsilastirmadir; cop girdinin %99,98'ini eler
(88.963 -> 17). Saglama toplaminin yerini tutmaz.

DIKKAT: elbari_basit'e sikistirilmis verinin TAM boyutu verilmelidir.

GUVENLIK KURALI:
Butunluk kontrolu yalnizca cerceve katmanindadir. Cekirdek cozucu bozuk
girdiyi kabul edip anlamsiz veri uretebilir (tampon tasirmaz, cokmez).
Bu yuzden GUVENILMEYEN kaynaktan gelen veri DAIMA cerceve katmanindan
gecirilmelidir. Ayrinti icin src/elbari.h basindaki guvenlik notuna bakin.

## Float kuantalama

src/elbari_float.c - ondalikli telemetriyi istenen hassasiyete gore
tamsayiya cevirir; sonuc mevcut boru hattina verilir, bicim degismez.

!!! KAYIPLIDIR !!! Secilen hassasiyetin altindaki kisim atilir. Tam degerin
korunmasi gereken veriler bu katmandan gecirilmemelidir. Kayipsiz float
sikistirma (XOR tabanli) bu surumde yoktur.

Olculen (12.000 kayit x 6 kanal, gercekci ucus verisi):
  ham float32                 : 288.000 bayt
  kuantalama + kanal katmani  :  27.403 bayt  (10.51x)
  float bit desenini dogrudan : 195.039 bayt  (1.48x)
  -> kuantalama 6.9 kat daha iyi

Ikili uyumluluk: 72.000 degerin tamami C ve .NET'te ayni yuvarlandi.
Yuvarlama cift duyarlikta ve acikca "sifirdan uzaga" yapilir; C#'in
varsayilan bankaci yuvarlamasi kullanilmaz.

## Kayipsiz float (XOR)

src/elbari_float_xor.c - ardisik float'larin bit desenleri XOR'lanir;
yalnizca anlamli bitler yazilir. Deger degismemisse tek bit yeter.
Literaturde Gorilla / Chimp.

OLCULEN - durust beklenti yonetimi:
  gurultulu ucus verisi : XOR  1.21x  vs  kuantalama 10.51x
  duragan veri          : XOR 15.08x  vs  kuantalama 10.86x
  duzgun sinyal         : XOR  1.00x  vs  kuantalama 15.83x

Kayipsiz float GURULTULU veride az kazandirir; gurultu mantisin alt
bitlerini surekli degistirir ve bu bitler sikistirilamaz. XOR yalnizca
degerler AYNEN tekrar ettiginde parlar.

KURAL: Tam deger gerekmiyorsa kuantalama kullanin.

Kayipsizlik dogrulandi: NaN, -0.0, sonsuz, epsilon dahil tum degerler
BIT BIT ayni geri geliyor.

## Uygunluk (conformance) testi

Bicim spesifikasyonu: ../belgeler/BICIM_SPESIFIKASYONU.md
Dondurulmus vektorler: ../testverisi/vektorler.txt

    uygunluk.exe ../testverisi/vektorler.txt

C surumu 18 vektorun tamamini gecer (36 kontrol: 18 kodlama + 18 cozme).
Vektorler C# referansindan uretilmistir; C bunlari birebir tutturuyorsa
iki implementasyon bicim acisindan denktir.

## Olcum

    derle.bat
    olcum.exe <referans_dizini>      verim + gecikme dagilimi
    dogrulama.exe <referans_dizini>  .NET ile ikili uyumluluk
    analiz.bat                       MSVC statik analiz

Olculen (gercek GPS verisi, 24.642 kayit):
  encode 1102 MB/sn, decode 1450 MB/sn (saf skaler C)
  cerceve basina gecikme: medyan 1.80us, p99 2.70us, p99.9 4.00us
  oynama orani (p99/medyan): 1.50x

## Durum

Tamamlanan: uc katman, .NET ile ikili uyumluluk, verim ve gecikme
olcumu, MISRA oz-degerlendirmesi, tamsayi tasma korumasi.

Ayrica: 400.000 turluk fuzz testi (tampon tasmasi 0), tamsayi tasma
korumasi, katmanli butunluk modeli belgelendi.

Yapilacak: sertifikali MISRA araci ile dogrulama, GCC/Clang derleme,
ARM ve big-endian dogrulama, RTOS uzerinde WCET analizi, elle SIMD.
