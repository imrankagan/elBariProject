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

    kanal: C ciktisi == .NET ciktisi        83124 bayt birebir ayni
    kanal: C round-trip kayipsiz            tum elemanlar birebir geri geldi
    kanal: C, .NET ciktisini cozebiliyor    capraz uyumluluk dogrulandi
    cerceve: yaz/oku bagimsiz ve kayipsiz   247 cerceve, 3.37x
    cerceve: tek-bit bozulma CRC ile        247/247 yakalandi

## Durum

Tamamlanan: cekirdek, kanal ve cerceve katmanlari; .NET ile ikili
uyumluluk dogrulandi.

Yapilacak: SIMD hizlandirma (opsiyonel, #ifdef arkasinda), MISRA C
statik analiz gecisi, en-kotu-durum gecikme olcumu, ARM uzerinde
dogrulama.
