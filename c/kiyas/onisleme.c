/* =====================================================================
 * KIYAS - ortak on isleme ve bit akisi
 * ---------------------------------------------------------------------
 * Bu dosyadaki her sey RAKIPLERIN LEHINEDIR: kanal ayrimi, fark alma ve
 * zigzag esleme rakip kodeklere bedava verilir. Boylece "ElBari kanal
 * ayrimini biliyor, rakipler bilmiyor" itirazi ortadan kalkar.
 * ===================================================================== */

#include "kiyas.h"

/* ---------------------------------------------------------------------
 * ZIGZAG
 * ------------------------------------------------------------------- */

uint32_t kiyas_zigzag(int32_t v)
{
    uint32_t u = (uint32_t)v;
    /* Isaretli kaydirma yerine maske: tanimsiz/uygulamaya bagli davranis yok. */
    uint32_t maske = (uint32_t)0u - (u >> 31);
    return (uint32_t)((u << 1) ^ maske);
}

int32_t kiyas_zigzag_ters(uint32_t u)
{
    uint32_t maske = (uint32_t)0u - (u & 1u);
    return (int32_t)((u >> 1) ^ maske);
}

/* ---------------------------------------------------------------------
 * KANAL AYRIMI
 * ------------------------------------------------------------------- */

int32_t kiyas_kanal_cek(const int32_t *ic_ice, int32_t eleman_sayisi,
                        int32_t kanal_sayisi, int32_t kanal_no,
                        int32_t *cikti)
{
    int32_t adet = 0;
    int32_t i;

    for (i = kanal_no; i < eleman_sayisi; i += kanal_sayisi)
    {
        cikti[adet] = ic_ice[i];
        adet++;
    }
    return adet;
}

void kiyas_kanal_koy(const int32_t *kanal_verisi, int32_t adet,
                     int32_t kanal_sayisi, int32_t kanal_no,
                     int32_t *ic_ice)
{
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        ic_ice[(i * kanal_sayisi) + kanal_no] = kanal_verisi[i];
    }
}

/* ---------------------------------------------------------------------
 * FARK + ZIGZAG
 * ------------------------------------------------------------------- */

void kiyas_delta_zigzag(const int32_t *veri, int32_t adet, uint32_t *cikti)
{
    int32_t i;

    if (adet <= 0) { return; }

    cikti[0] = kiyas_zigzag(veri[0]);
    for (i = 1; i < adet; i++)
    {
        /* Isaretsiz aritmetik: isaretli tasma tanimsiz davranisi olusmaz. */
        uint32_t fark = (uint32_t)veri[i] - (uint32_t)veri[i - 1];
        cikti[i] = kiyas_zigzag((int32_t)fark);
    }
}

void kiyas_delta_zigzag_ters(const uint32_t *veri, int32_t adet, int32_t *cikti)
{
    int32_t i;

    if (adet <= 0) { return; }

    cikti[0] = kiyas_zigzag_ters(veri[0]);
    for (i = 1; i < adet; i++)
    {
        uint32_t fark = (uint32_t)kiyas_zigzag_ters(veri[i]);
        cikti[i] = (int32_t)((uint32_t)cikti[i - 1] + fark);
    }
}

/* ---------------------------------------------------------------------
 * BIT GENISLIGI
 * ------------------------------------------------------------------- */

int32_t kiyas_bit_genisligi(uint32_t v)
{
    int32_t n = 0;

    while (v != 0u)
    {
        n++;
        v >>= 1;
    }
    return n;
}

/* ---------------------------------------------------------------------
 * BIT YAZICI
 * ------------------------------------------------------------------- */

static uint64_t alt_maske(int32_t genislik)
{
    if (genislik >= 64) { return ~(uint64_t)0u; }
    return (((uint64_t)1u << genislik) - (uint64_t)1u);
}

void kiyas_yazici_kur(kiyas_bit_yazici *y, uint8_t *tampon, int32_t kapasite)
{
    y->tampon      = tampon;
    y->kapasite    = kapasite;
    y->bayt_konum  = 0;
    y->birikim     = 0u;
    y->birikim_bit = 0;
    y->tasti       = 0;
}

void kiyas_bit_yaz(kiyas_bit_yazici *y, uint32_t deger, int32_t genislik)
{
    if (genislik <= 0) { return; }

    /* birikim_bit her cagri oncesi < 8, genislik <= 32 => en fazla 39 bit. */
    y->birikim |= ((uint64_t)deger & alt_maske(genislik)) << y->birikim_bit;
    y->birikim_bit += genislik;

    while (y->birikim_bit >= 8)
    {
        if (y->bayt_konum >= y->kapasite)
        {
            y->tasti = 1;
            return;
        }
        y->tampon[y->bayt_konum] = (uint8_t)(y->birikim & 0xFFu);
        y->bayt_konum++;
        y->birikim >>= 8;
        y->birikim_bit -= 8;
    }
}

int32_t kiyas_yazici_bitir(kiyas_bit_yazici *y)
{
    if (y->birikim_bit > 0)
    {
        if (y->bayt_konum >= y->kapasite)
        {
            y->tasti = 1;
        }
        else
        {
            y->tampon[y->bayt_konum] = (uint8_t)(y->birikim & 0xFFu);
            y->bayt_konum++;
            y->birikim     = 0u;
            y->birikim_bit = 0;
        }
    }
    if (y->tasti != 0) { return -1; }
    return y->bayt_konum;
}

/* ---------------------------------------------------------------------
 * BIT OKUYUCU
 * ------------------------------------------------------------------- */

void kiyas_okuyucu_kur(kiyas_bit_okuyucu *o, const uint8_t *tampon, int32_t boyut)
{
    o->tampon      = tampon;
    o->boyut       = boyut;
    o->bayt_konum  = 0;
    o->birikim     = 0u;
    o->birikim_bit = 0;
}

uint32_t kiyas_bit_oku(kiyas_bit_okuyucu *o, int32_t genislik)
{
    uint32_t sonuc;

    if (genislik <= 0) { return 0u; }

    while (o->birikim_bit < genislik)
    {
        uint64_t b = 0u;
        if (o->bayt_konum < o->boyut)
        {
            b = (uint64_t)o->tampon[o->bayt_konum];
        }
        o->bayt_konum++;
        o->birikim |= b << o->birikim_bit;
        o->birikim_bit += 8;
    }

    sonuc = (uint32_t)(o->birikim & alt_maske(genislik));
    o->birikim >>= genislik;
    o->birikim_bit -= genislik;
    return sonuc;
}

int32_t kiyas_okuyucu_bayt(const kiyas_bit_okuyucu *o)
{
    return o->bayt_konum;
}
