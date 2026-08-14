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
_Static_assert(sizeof(int32_t) == 4, "int32_t 4 bayt olmali");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t 4 bayt olmali");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t 8 bayt olmali");
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

static ELBARI_SATIRICI int32_t elbari_ic_i32_oku(const uint8_t *kaynak)
{
    return elbari_ic_isaretliye_cevir(elbari_ic_u32_oku(kaynak));
}

#endif /* ELBARI_IC_H */
