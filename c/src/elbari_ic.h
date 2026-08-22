/* =====================================================================
 * ELBARI - Ic yardimcilar (kutuphane disina acilmaz)
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * ---------------------------------------------------------------------
 * Buradaki islevler iki amaca hizmet eder:
 *
 *  1) TANIMSIZ DAVRANISTAN KACINMA
 *     C'de isaretli tamsayi tasmasi tanimsiz davranistir. .NET tarafi
 *     ise varsayilan olarak "unchecked" calisir ve sessizce sarar.
 *     Ikili uyumlulugu korumak icin tum fark hesaplari isaretsiz
 *     aritmetik uzerinden yapilir ve sonuc guvenli sekilde geri
 *     cevrilir.
 *
 *  2) BAYT DUZENI BAGIMSIZLIGI
 *     Cok baytli alanlar acikca little-endian yazilir/okunur. Boylece
 *     big-endian bir islemcide de .NET ciktisiyla ayni bicim uretilir.
 * ===================================================================== */

#ifndef ELBARI_IC_H
#define ELBARI_IC_H

#include <stdint.h>
#include <string.h>

/* Satirici (inline) anahtar kelimesi.
 * C99 ve sonrasi "inline" tanir. Daha eski derleyicilerde (C90 modu,
 * bazi gomulu araç zincirleri) "__inline" ya da hicbir sey kullanilir.
 * Boylece kod her yerde derlenir, yalnizca eniyileme ipucu degisir. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  define ELBARI_SATIRICI inline
#elif defined(_MSC_VER)
#  define ELBARI_SATIRICI __inline
#else
#  define ELBARI_SATIRICI /* desteklenmiyor */
#endif

/* C11 ve sonrasinda derleme aninda varsayim denetimi yapilir.
 * Daha eski araç zincirlerinde bu blok sessizce atlanir; kod yine derlenir. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(int32_t) == 4u, "int32_t 4 bayt olmali");
_Static_assert(sizeof(uint32_t) == 4u, "uint32_t 4 bayt olmali");
_Static_assert(sizeof(uint64_t) == 8u, "uint64_t 8 bayt olmali");
#endif

/* ---------------------------------------------------------------------
 * TIP DONUSUMLERI
 * ------------------------------------------------------------------- */

/**
 * Isaretsiz 32-bit degeri isaretli 32-bit degere cevirir.
 * Dogrudan atama, deger INT32_MAX'i astiginda uygulamaya bagli
 * davranistir; memcpy ile bit deseni birebir tasinir ve davranis
 * her platformda ayni olur.
 */
static ELBARI_SATIRICI int32_t elbari_ic_isaretliye_cevir(uint32_t deger)
{
    int32_t sonuc;
    (void)memcpy(&sonuc, &deger, sizeof(sonuc));
    return sonuc;
}

/** Isaretli 32-bit degeri isaretsiz 32-bit degere cevirir. */
static ELBARI_SATIRICI uint32_t elbari_ic_isaretsize_cevir(int32_t deger)
{
    uint32_t sonuc;
    (void)memcpy(&sonuc, &deger, sizeof(sonuc));
    return sonuc;
}

/**
 * Cozucunun tuketmeden birakabilecegi en fazla bayt sayisi.
 *
 * Gecerli bir akista bu deger 0'dir; kodlayici ne yazdiysa cozucu onu
 * okur. Kucuk bir tolerans, ileride bicime hizalama/dolgu eklenirse
 * kirilma olmamasi icin birakilmistir.
 *
 * Hem cekirdek (tek akis API'si) hem kanal katmani (toplu kontrol)
 * kullandigi icin ortak ic baslikta durur.
 */
#define ELBARI_ARTIK_TOLERANSI          (0)

/* ---------------------------------------------------------------------
 * ARITMETIK
 * ------------------------------------------------------------------- */

/**
 * Iki degerin farki (saran aritmetik).
 * Isaretsiz uzerinden hesaplanir; boylece tasma tanimsiz davranis
 * olmaz ve .NET'in "unchecked" davranisiyla birebir ayni sonuc uretilir.
 */
static ELBARI_SATIRICI int32_t elbari_ic_fark(int32_t simdiki, int32_t onceki)
{
    uint32_t f = elbari_ic_isaretsize_cevir(simdiki) - elbari_ic_isaretsize_cevir(onceki);
    return elbari_ic_isaretliye_cevir(f);
}

/**
 * Iki degerin toplami (saran aritmetik). Onek toplam icin kullanilir.
 */
static ELBARI_SATIRICI int32_t elbari_ic_topla(int32_t a, int32_t b)
{
    uint32_t t = elbari_ic_isaretsize_cevir(a) + elbari_ic_isaretsize_cevir(b);
    return elbari_ic_isaretliye_cevir(t);
}

/**
 * Tasma-guvenli mutlak deger.
 *
 * DIKKAT: INT32_MIN icin sonuc yine INT32_MIN'dir (negatif kalir).
 * Bu bir hata degil, .NET surumuyle bilincli olarak ayni tutulan bir
 * davranistir: orada da bit hilesi ayni sekilde sarar. Cagiran kod bu
 * durumu "aykiri deger degil" olarak degerlendirir; kayipsizlik
 * bozulmaz cunku fark yine 32 bit olarak kodlanir.
 */
static ELBARI_SATIRICI int32_t elbari_ic_mutlak_deger(int32_t deger)
{
    int32_t sonuc;
    if (deger < 0)
    {
        sonuc = elbari_ic_isaretliye_cevir(0u - elbari_ic_isaretsize_cevir(deger));
    }
    else
    {
        sonuc = deger;
    }
    return sonuc;
}

/**
 * 64 bite genisleterek mutlak deger alir.
 *
 * elbari_ic_mutlak_deger'den farki: sonuc her zaman dogru ve pozitiftir,
 * INT32_MIN dahil. Buyukluk karsilastirmasi yapilan yerlerde (ornegin
 * ikinci derece fark karari) bu kullanilir; bit akisini etkileyen
 * yerlerde ise .NET ile ayni davranisi korumak icin 32 bitlik surum
 * kullanilir.
 */
static ELBARI_SATIRICI int64_t elbari_ic_mutlak64(int32_t deger)
{
    int64_t g = (int64_t)deger;
    return (g < 0) ? -g : g;
}

/**
 * Dusuk 'adet' biti secen maske (adet: 0..32; aralik disi degerler kelepcelenir).
 *
 * Kaydirma operatoru YERINE sabit tablo kullanilir. Sebep: kaydirma
 * miktarinin sinirli oldugu elle ispatlanabilir olsa da statik cozumleyici
 * bu bilgiyi fonksiyonlar arasinda tasiyamaz. Tabloda indeks ucuncu satirda
 * acikca 0..32'ye kelepcelenir; tanimsiz kaydirma davranisi hem gercekte
 * hem de arac acisindan imkansiz hale gelir (MISRA 12.2).
 *
 * Maliyet: 132 bayt salt-okunur veri. Kazanc: kaydirma yerine tek okuma.
 */
static ELBARI_SATIRICI uint32_t elbari_ic_alt_maske(int32_t adet)
{
    /* MISRA 8.9: tek islevde kullanildigi icin blok kapsaminda tanimlanir. */
    static const uint32_t elbari_ic_alt_maskeler[33] =
    {
        0x00000000u, 0x00000001u, 0x00000003u, 0x00000007u,
        0x0000000Fu, 0x0000001Fu, 0x0000003Fu, 0x0000007Fu,
        0x000000FFu, 0x000001FFu, 0x000003FFu, 0x000007FFu,
        0x00000FFFu, 0x00001FFFu, 0x00003FFFu, 0x00007FFFu,
        0x0000FFFFu, 0x0001FFFFu, 0x0003FFFFu, 0x0007FFFFu,
        0x000FFFFFu, 0x001FFFFFu, 0x003FFFFFu, 0x007FFFFFu,
        0x00FFFFFFu, 0x01FFFFFFu, 0x03FFFFFFu, 0x07FFFFFFu,
        0x0FFFFFFFu, 0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu,
        0xFFFFFFFFu
    };

    int32_t k = adet;

    if (k < 0)
    {
        k = 0;
    }
    if (k > 32)
    {
        k = 32;
    }
    return elbari_ic_alt_maskeler[(uint32_t)k];
}

/* ---------------------------------------------------------------------
 * BAYRAK BITLERI
 * ---------------------------------------------------------------------
 * Kanal basina 1 bitlik bayraklar bayt dizisinde tutulur. Indeks
 * hesabi ve maskeleme ISARETSIZ tip uzerinde yapilir; boylece bit
 * islemlerinde isaretli operand kullanilmaz (MISRA 10.1) ve bilesik
 * ifade donusumu olusmaz (MISRA 10.8).
 *
 * Maske kaydirma ile degil sabit tablodan alinir: indeks & 7u ile
 * 0..7 araligina sinirli oldugundan hem gercekte hem statik cozumleme
 * acisindan sinir disi erisim imkansizdir (MISRA 12.2).
 * ------------------------------------------------------------------- */

static const uint8_t elbari_ic_bit_maskesi[8] =
{
    0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u
};

/** indeks numarali bayragi kurar. */
static ELBARI_SATIRICI void elbari_ic_bayrak_kur(uint8_t *bayraklar, int32_t indeks)
{
    uint32_t i = (uint32_t)indeks;
    uint32_t bayt = i >> 3;
    uint32_t bit = i & 7u;
    uint8_t  maske = elbari_ic_bit_maskesi[bit];

    bayraklar[bayt] |= maske;
}

/** indeks numarali bayrak kurulu mu? 1 evet, 0 hayir. */
static ELBARI_SATIRICI int32_t elbari_ic_bayrak_var_mi(const uint8_t *bayraklar, int32_t indeks)
{
    uint32_t i = (uint32_t)indeks;
    uint32_t bayt = i >> 3;
    uint32_t bit = i & 7u;
    uint8_t  maske = elbari_ic_bit_maskesi[bit];
    int32_t  kurulu;

    /* MISRA 10.6: bilesik ifade yerine acik dallanma. */
    if ((bayraklar[bayt] & maske) != 0u)
    {
        kurulu = 1;
    }
    else
    {
        kurulu = 0;
    }
    return kurulu;
}

/* ---------------------------------------------------------------------
 * BAYT DUZENI (acikca little-endian)
 * ------------------------------------------------------------------- */

static ELBARI_SATIRICI void elbari_ic_u32_yaz(uint8_t *hedef, uint32_t deger)
{
    hedef[0] = (uint8_t)(deger & 0xFFu);
    hedef[1] = (uint8_t)((deger >> 8) & 0xFFu);
    hedef[2] = (uint8_t)((deger >> 16) & 0xFFu);
    hedef[3] = (uint8_t)((deger >> 24) & 0xFFu);
}

static ELBARI_SATIRICI uint32_t elbari_ic_u32_oku(const uint8_t *kaynak)
{
    return ((uint32_t)kaynak[0])
         | ((uint32_t)kaynak[1] << 8)
         | ((uint32_t)kaynak[2] << 16)
         | ((uint32_t)kaynak[3] << 24);
}

static ELBARI_SATIRICI void elbari_ic_i32_yaz(uint8_t *hedef, int32_t deger)
{
    elbari_ic_u32_yaz(hedef, elbari_ic_isaretsize_cevir(deger));
}

/** 16 bitlik isaretsiz deger, little-endian. */
static ELBARI_SATIRICI void elbari_ic_u16_yaz(uint8_t *hedef, uint16_t deger)
{
    hedef[0] = (uint8_t)(deger & 0xFFu);
    hedef[1] = (uint8_t)((deger >> 8) & 0xFFu);
}

static ELBARI_SATIRICI uint16_t elbari_ic_u16_oku(const uint8_t *kaynak)
{
    return (uint16_t)((uint16_t)kaynak[0] | ((uint16_t)kaynak[1] << 8));
}

static ELBARI_SATIRICI int32_t elbari_ic_i32_oku(const uint8_t *kaynak)
{
    return elbari_ic_isaretliye_cevir(elbari_ic_u32_oku(kaynak));
}

#endif /* ELBARI_IC_H */
