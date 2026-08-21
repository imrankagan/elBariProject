# Kıyas — ElBâri'yi kendi ailesiyle ölçmek

## Neden bu klasör var

ElBâri, [README](../../README.md#-algoritma-detayları) ve
[biçim spesifikasyonunda](../../belgeler/BICIM_SPESIFIKASYONU.md) kendini açıkça
**PFOR-Delta ailesinin bir uygulaması** olarak tanımlıyor. Buna rağmen bugüne kadar
yalnızca **genel amaçlı bayt sıkıştırıcılarıyla** (zstd, LZ4, Brotli, Deflate)
karşılaştırılmıştı.

Bir kodeği kendi ailesiyle kıyaslamamak, karşılaştırmanın en zayıf noktasıdır:
"zstd'yi üç kat geçtik" cümlesi, doğru rakip zstd olmadığı için katkıyı ölçmez.
Bu klasör o boşluğu kapatır.

## Ölçülen kodekler

| Kodek | Kaynak | Neden listede |
| --- | --- | --- |
| VByte (LEB128) | varint temel çizgisi | Her tamsayı kodek çalışmasının alt sınırı |
| StreamVByte | Lemire & Kurz, 2017 | Kontrol baytlarını veriden ayıran modern varint |
| Simple8b | Anh & Moffat, 2010 | 64-bit kelimeye seçici + paketleme; kelime hizalı ailenin temsilcisi |
| BP128 | Lemire & Boytsov, 2015 | FastPFor kütüphanesindeki `BinaryPacking`; istisnasız ikili paketleme |
| OptPFD | Zukowski 2006 / Yan, Ding & Suel 2009 | **ElBâri'nin doğrudan atası**: taban bit genişliği + istisna yamalama |
| Sprintz-Delta | Blalock, Madden & Guttag, 2018 | **En yakın literatür komşusu**: çok kanallı, gömülü hedefli, kayıpsız zaman serisi |
| ElBâri (kanal) | bu çalışma | — |
| ElBâri (çerçeve, 100 kayıt) | bu çalışma | Paket kaybı dayanıklılığının oran maliyeti |

## ⚠️ Metodolojik uyarı — sayıları kullanmadan önce okuyun

**Buradaki rakip kodekler, yazarlarının kütüphaneleri değildir.** Yayınlanmış biçim
tanımlarından yeniden yazılmış skaler C uygulamalarıdır. Bunun iki ayrı sonucu vardır
ve ikisini karıştırmamak gerekir:

**1. Oran taşınabilirdir — teze girebilir.**
Bir biçimin ürettiği bayt sayısı biçim tanımından gelir, uygulamanın kalitesinden
değil. Simple8b'yi doğru yazan herkes aynı boyutu üretir. Oran sütunu literatürle
karşılaştırılabilir.

**2. Hız taşınabilir değildir — alt sınırdır.**
FastPFor ve StreamVByte'ın SIMD sürümleri buradaki skaler sürümlerden kat kat
hızlıdır. Hız sütunu rakipler için **bir alt sınırdır**, gerçek performansları değil.

Buna karşılık **karşılaştırma sınıfı adildir**: ElBâri'nin C sürümü de skalerdir
(SIMD yok), aynı derleyici, aynı bayraklar, aynı makine, aynı veri, aynı tur sayısı.
Yani "skaler C uygulamaları arasında" geçerli bir sıralamadır.

**Bir sonraki adım:** yazarların kütüphanelerini (FastPFor, streamvbyte, Sprintz)
bağlayıp SIMD'li sürümlerle tekrar ölçmek. Bu yapılmadan hız sütunu üzerinden
"ElBâri daha hızlı" denemez.

## Adil karşılaştırma için yapılanlar

- **Ön işleme rakiplere bedava verildi.** Kanal ayrımı + fark alma + zigzag eşleme,
  Aile A kodekleri için dışarıda yapılır ve **bu süre onların encode süresine
  dahildir**. ElBâri bunları kendi içinde yapar. Böylece "ElBâri veri yapısını biliyor,
  rakipler bilmiyor" itirazı ortadan kalkar.
- **Başlık yükü sayılır.** Her kodeğin kendi çerçeveleme/başlık maliyeti sıkıştırılmış
  boyuta dahildir.
- **Her kodek tam tur doğrulamasından geçer.** Doğrulamayı geçemeyen kodeğin oranı
  anlamsızdır; tabloda `HAYIR` olarak işaretlenir. (Bu kontrol geliştirme sırasında
  çok kanallı çözme yolunda gerçek bir tampon çakışması hatası yakaladı.)
- **Ölçüm tekrarlanabilir.** Boyutlar deterministiktir; üç ayrı koşuda bayt bayt aynı
  çıktı, hız değerlerinde ~%3 oynama gözlendi.

## Rakip uygulamaların doğrulanması

Oran sütununun anlamlı olması, rakip uygulamaların **doğru** olmasına bağlıdır. Tek bir
veri setinde tam turun geçmesi yeterli kanıt değildir: kuyruk yolları (blok boyutuna
bölünmeyen adetler), sınır değerler (0, 2³¹, 2³²−1), tamamen sıfır bloklar ve tek
elemanlı girdiler gerçek GPS verisinde hiç tetiklenmez.

[`oztest.c`](oztest.c) her kodeği **6 değer dağılımı × 24 uzunluk** (Sprintz için
ayrıca 6 farklı kanal sayısı) ile tam tur doğrulamasından geçirir. Uzunluklar blok
sınırlarını (8, 120, 128, 240) ve kuyruklarını kasten zorlar.

    oztest.exe          # Windows
    make -C .. kiyas-oztest && ../kiyas-oztest

Sonuç:

    VByte        GECTI   (144 durum)
    StreamVByte  GECTI   (144 durum)
    Simple8b     GECTI   (144 durum)
    BP128        GECTI   (144 durum)
    OptPFD       GECTI   (144 durum)
    Sprintz      GECTI   (360 durum)
    SONUC: tum kodekler tum durumlarda kayipsiz.

Test dış veri istemez ve `make test` içinde çalışır.

## Bilinen sınırlar

1. **Sprintz-Delta uygulandı, Sprintz-Delta-Huf değil.** Yazarların tam sürümü bit
   paketlemeden sonra bir de Huffman katmanı çalıştırır; oranı bir miktar artırır, hızı
   düşürür. Buradaki sürüm gömülü hedefte anlamlı olan hızlı varyanttır.
2. **Sprintz'in FIRE öngörücüsü yerine düz fark kullanıldı.** Makale her ikisini de
   sunar; FIRE düzgün sinyallerde biraz daha iyidir.
3. **Sprintz 8/16 bit veri için tasarlandı; 32 bite uyarlandı** (blok başı kanal
   genişliği 6 bit). Bu bir genişletmedir, yazarların biçimi değildir.
4. **OptPFD, FastPFor'un kendisi değildir.** Aynı ailenin daha eski üyesidir. FastPFor,
   istisna sayfalarını bit genişliğine göre gruplayarak birkaç yüzde daha iyi oran verir.
5. **Tek veri seti.** Yalnızca gerçek GPS izleri (3 kanal). Yönelim/IMU kanalları içeren
   gerçek uçuş logları ile tekrar edilmelidir.

## Derleme ve çalıştırma

Windows (MSVC):

    derle.bat
    kiyas.exe ..\..\testverisi\gercek_gps.bin 200

Linux / macOS:

    make -C .. kiyas
    ../kiyas ../../testverisi/gercek_gps.bin 200

İkinci argüman tur sayısıdır (varsayılan 100). Çalıştırma, çalışılan dizine
`kiyas_sonuclari.csv` dosyasını yazar.

## Kapsam notu

Bu klasör **ölçüm kodudur, kütüphane değildir.** MISRA C:2012 uyum iddiası
[`c/src/`](../src/) için geçerlidir; burası (`c/test/` gibi) kapsam dışıdır.

## Sonuçlar

Tam rapor ve Pareto analizi:
**[belgeler/KIYAS_TAMSAYI_KODEKLER.md](../../belgeler/KIYAS_TAMSAYI_KODEKLER.md)**
