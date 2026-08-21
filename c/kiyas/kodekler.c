/* =====================================================================
 * KIYAS - AILE A: tek akis tamsayi kodekleri
 * ---------------------------------------------------------------------
 * Hepsi yayinlanmis bicim tanimlarindan yeniden yazilmis SKALER C
 * uygulamalaridir. Yazarlarinin kutuphaneleri degildir (bkz. kiyas.h
 * icindeki metodolojik uyari).
 *
 *   VByte        : LEB128 / varint. Her bayt 7 bit yuk + 1 devam biti.
 *   StreamVByte  : Lemire & Kurz 2017. 4'lu gruplar, kontrol baytlari
 *                  veriden ayri akista tutulur (dallanma tahmini icin).
 *   Simple8b     : Anh & Moffat 2010. 64 bit kelime = 4 bit secici +
 *                  60 bit yuk. Secici, kac degerin kac bitle paketlendigini
 *                  soyler.
 *   BP128        : Lemire & Boytsov 2015 (FastPFor kutuphanesindeki
 *                  BinaryPacking). 128'lik blok, blok basina tek bit
 *                  genisligi, istisna YOK.
 *   OptPFD       : Zukowski 2006 (PFOR) / Yan, Ding, Suel 2009. 128'lik
 *                  blok, taban bit genisligi + istisna yamalama.
 * ===================================================================== */

#include "kiyas.h"

#define BLOK (128)

/* =====================================================================
 * 1) VBYTE (LEB128)
 * ===================================================================== */

int32_t kiyas_vbyte_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap)
{
    int32_t p = 0;
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        uint32_t v = g[i];
        while (v >= 0x80u)
        {
            if (p >= kap) { return -1; }
            c[p] = (uint8_t)(v | 0x80u);
            p++;
            v >>= 7;
        }
        if (p >= kap) { return -1; }
        c[p] = (uint8_t)v;
        p++;
    }
    return p;
}

int32_t kiyas_vbyte_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet)
{
    int32_t p = 0;
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        uint32_t v = 0u;
        int32_t  kaydir = 0;

        for (;;)
        {
            uint8_t b;
            if (p >= boyut) { return -1; }
            b = g[p];
            p++;
            v |= ((uint32_t)(b & 0x7Fu)) << kaydir;
            if ((b & 0x80u) == 0u) { break; }
            kaydir += 7;
            if (kaydir > 28) { return -1; }
        }
        c[i] = v;
    }
    return 0;
}

/* =====================================================================
 * 2) STREAMVBYTE  (Lemire & Kurz, 2017)
 * ---------------------------------------------------------------------
 * Bicim: [kontrol baytlari][veri baytlari]
 *   kontrol bayti = 4 degerin 2'ser bitlik uzunluk kodu (00->1 ... 11->4)
 * Kontrol akisinin veriden ayrilmasi, cozucude dallanmayi tahmin
 * edilebilir kilar; SIMD surumunun temeli budur.
 * ===================================================================== */

static int32_t svb_uzunluk(uint32_t v)
{
    if (v < 0x100u)      { return 1; }
    if (v < 0x10000u)    { return 2; }
    if (v < 0x1000000u)  { return 3; }
    return 4;
}

int32_t kiyas_streamvbyte_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap)
{
    int32_t kontrol_boyut = (adet + 3) / 4;
    int32_t vp = kontrol_boyut;
    int32_t i;

    if (kontrol_boyut > kap) { return -1; }
    for (i = 0; i < kontrol_boyut; i++) { c[i] = 0u; }

    for (i = 0; i < adet; i++)
    {
        uint32_t v   = g[i];
        int32_t  uzn = svb_uzunluk(v);
        int32_t  j;

        c[i >> 2] |= (uint8_t)((uint32_t)(uzn - 1) << ((i & 3) * 2));

        if ((vp + uzn) > kap) { return -1; }
        for (j = 0; j < uzn; j++)
        {
            c[vp] = (uint8_t)((v >> (j * 8)) & 0xFFu);
            vp++;
        }
    }
    return vp;
}

int32_t kiyas_streamvbyte_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet)
{
    int32_t kontrol_boyut = (adet + 3) / 4;
    int32_t vp = kontrol_boyut;
    int32_t i;

    if (kontrol_boyut > boyut) { return -1; }

    for (i = 0; i < adet; i++)
    {
        int32_t  uzn = (int32_t)((g[i >> 2] >> ((i & 3) * 2)) & 3u) + 1;
        uint32_t v   = 0u;
        int32_t  j;

        if ((vp + uzn) > boyut) { return -1; }
        for (j = 0; j < uzn; j++)
        {
            v |= ((uint32_t)g[vp]) << (j * 8);
            vp++;
        }
        c[i] = v;
    }
    return 0;
}

/* =====================================================================
 * 3) SIMPLE8B  (Anh & Moffat, 2010)
 * ---------------------------------------------------------------------
 * 64 bit kelime: ust 4 bit secici, alt 60 bit yuk.
 * Secici tablosu: kac deger x kac bit.
 * ===================================================================== */

static const int32_t S8B_ADET[16] = { 240, 120, 60, 30, 20, 15, 12, 10,
                                        8,   7,  6,  5,  4,  3,  2,  1 };
static const int32_t S8B_BIT[16]  = {   0,   0,  1,  2,  3,  4,  5,  6,
                                        7,   8, 10, 12, 15, 20, 30, 60 };

int32_t kiyas_simple8b_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap)
{
    int32_t p = 0;
    int32_t i = 0;

    while (i < adet)
    {
        int32_t  kalan = adet - i;
        int32_t  s;
        int32_t  secilen = -1;
        int32_t  kac = 0;
        uint64_t kelime;
        int32_t  j;

        for (s = 0; s < 16; s++)
        {
            int32_t  bit = S8B_BIT[s];
            int32_t  n   = S8B_ADET[s];
            int32_t  sinir;
            int32_t  uygun = 1;

            if (bit == 0)
            {
                /* Sifir seciciler: gercekten o kadar sifir bulunmali. */
                if (kalan < n) { continue; }
                for (j = 0; j < n; j++)
                {
                    if (g[i + j] != 0u) { uygun = 0; break; }
                }
                if (uygun == 0) { continue; }
                secilen = s;
                kac     = n;
                break;
            }

            sinir = (n < kalan) ? n : kalan;
            for (j = 0; j < sinir; j++)
            {
                if (kiyas_bit_genisligi(g[i + j]) > bit) { uygun = 0; break; }
            }
            if (uygun == 0) { continue; }

            secilen = s;
            kac     = sinir;
            break;
        }

        if (secilen < 0) { return -1; }   /* olamaz: secici 15 her zaman uyar */

        kelime = ((uint64_t)(uint32_t)secilen) << 60;
        if (S8B_BIT[secilen] > 0)
        {
            for (j = 0; j < kac; j++)
            {
                kelime |= ((uint64_t)g[i + j]) << (j * S8B_BIT[secilen]);
            }
        }

        if ((p + 8) > kap) { return -1; }
        for (j = 0; j < 8; j++)
        {
            c[p] = (uint8_t)((kelime >> (j * 8)) & 0xFFu);
            p++;
        }

        i += (S8B_BIT[secilen] == 0) ? S8B_ADET[secilen] : kac;
    }
    return p;
}

int32_t kiyas_simple8b_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet)
{
    int32_t p = 0;
    int32_t i = 0;

    while (i < adet)
    {
        uint64_t kelime = 0u;
        int32_t  s;
        int32_t  bit;
        int32_t  n;
        int32_t  j;

        if ((p + 8) > boyut) { return -1; }
        for (j = 0; j < 8; j++)
        {
            kelime |= ((uint64_t)g[p]) << (j * 8);
            p++;
        }

        s   = (int32_t)((kelime >> 60) & 0xFu);
        bit = S8B_BIT[s];
        n   = S8B_ADET[s];

        for (j = 0; (j < n) && (i < adet); j++)
        {
            if (bit == 0)
            {
                c[i] = 0u;
            }
            else
            {
                uint64_t maske = (((uint64_t)1u << bit) - (uint64_t)1u);
                c[i] = (uint32_t)((kelime >> (j * bit)) & maske);
            }
            i++;
        }
    }
    return 0;
}

/* =====================================================================
 * 4) BP128 - ikili paketleme (Lemire & Boytsov, 2015)
 * ---------------------------------------------------------------------
 * 128'lik bloklar. Blok basina: 1 bayt bit genisligi + 128*b bit yuk.
 * 128*b her zaman 8'in kati oldugu icin bloklar bayt hizalidir.
 * Artan kuyruk (< 128 deger) VByte ile yazilir.
 * ISTISNA YOKTUR: tek bir buyuk deger tum blogu genisletir. PFOR
 * ailesinin (ve ElBari'nin) yamalama fikri tam olarak bunu hedefler.
 * ===================================================================== */

int32_t kiyas_bp128_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap)
{
    int32_t tam    = adet / BLOK;
    int32_t kuyruk = adet % BLOK;
    kiyas_bit_yazici y;
    int32_t k;
    int32_t p;
    int32_t kt;

    kiyas_yazici_kur(&y, c, kap);

    for (k = 0; k < tam; k++)
    {
        const uint32_t *blok = &g[k * BLOK];
        int32_t b = 0;
        int32_t j;

        for (j = 0; j < BLOK; j++)
        {
            int32_t w = kiyas_bit_genisligi(blok[j]);
            if (w > b) { b = w; }
        }

        kiyas_bit_yaz(&y, (uint32_t)b, 8);
        for (j = 0; j < BLOK; j++)
        {
            kiyas_bit_yaz(&y, blok[j], b);
        }
    }

    p = kiyas_yazici_bitir(&y);
    if (p < 0) { return -1; }

    if (kuyruk > 0)
    {
        kt = kiyas_vbyte_kodla(&g[tam * BLOK], kuyruk, &c[p], kap - p);
        if (kt < 0) { return -1; }
        p += kt;
    }
    return p;
}

int32_t kiyas_bp128_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet)
{
    int32_t tam    = adet / BLOK;
    int32_t kuyruk = adet % BLOK;
    kiyas_bit_okuyucu o;
    int32_t k;
    int32_t p;

    kiyas_okuyucu_kur(&o, g, boyut);

    for (k = 0; k < tam; k++)
    {
        int32_t b = (int32_t)kiyas_bit_oku(&o, 8);
        int32_t j;

        if (b > 32) { return -1; }
        for (j = 0; j < BLOK; j++)
        {
            c[(k * BLOK) + j] = kiyas_bit_oku(&o, b);
        }
    }

    p = kiyas_okuyucu_bayt(&o);
    if (p > boyut) { return -1; }

    if (kuyruk > 0)
    {
        if (kiyas_vbyte_coz(&g[p], boyut - p, &c[tam * BLOK], kuyruk) < 0)
        {
            return -1;
        }
    }
    return 0;
}

/* =====================================================================
 * 5) OptPFD - istisna yamali PFOR
 *    (Zukowski ve ark. 2006; Yan, Ding & Suel 2009)
 * ---------------------------------------------------------------------
 * 128'lik blok. Taban genislik b secilir; b bitten genis degerler
 * ISTISNA sayilir. Her degerin alt b biti akisa gomulu yazilir; istisna
 * olanlarin konumu (1 bayt) ve UST bitleri (32-b bit) blok sonuna
 * eklenir.
 *
 * b secimi: toplam bit maliyetini en kucukleyen b. Bit genisligi
 * histogrami uzerinden 33 adayin hepsi denenir (tam arama).
 *
 * Blok bicimi:
 *   8 bit   : b
 *   8 bit   : istisna sayisi e
 *   128*b   : tum degerlerin alt b biti
 *   e*8     : istisna konumlari
 *   e*(32-b): istisnalarin ust bitleri
 * ===================================================================== */

int32_t kiyas_optpfd_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap)
{
    int32_t tam    = adet / BLOK;
    int32_t kuyruk = adet % BLOK;
    kiyas_bit_yazici y;
    int32_t k;
    int32_t p;
    int32_t kt;

    kiyas_yazici_kur(&y, c, kap);

    for (k = 0; k < tam; k++)
    {
        const uint32_t *blok = &g[k * BLOK];
        int32_t histogram[33];
        int32_t b;
        int32_t j;
        int32_t en_iyi_b   = 32;
        long    en_iyi_bit = 0;
        int32_t bulundu    = 0;
        int32_t e;

        for (j = 0; j <= 32; j++) { histogram[j] = 0; }
        for (j = 0; j < BLOK; j++)
        {
            histogram[kiyas_bit_genisligi(blok[j])]++;
        }

        /* Her b adayi icin maliyet: 16 + 128*b + e*(8 + (32-b)) */
        for (b = 0; b <= 32; b++)
        {
            int32_t istisna = 0;
            long    maliyet;

            for (j = b + 1; j <= 32; j++) { istisna += histogram[j]; }
            if (istisna > 127) { continue; }   /* konum 1 bayta sigmali */

            maliyet = 16L + ((long)BLOK * (long)b)
                      + ((long)istisna * (long)(8 + (32 - b)));

            if ((bulundu == 0) || (maliyet < en_iyi_bit))
            {
                en_iyi_bit = maliyet;
                en_iyi_b   = b;
                bulundu    = 1;
            }
        }

        b = en_iyi_b;
        e = 0;
        for (j = b + 1; j <= 32; j++) { e += histogram[j]; }

        kiyas_bit_yaz(&y, (uint32_t)b, 8);
        kiyas_bit_yaz(&y, (uint32_t)e, 8);

        for (j = 0; j < BLOK; j++)
        {
            kiyas_bit_yaz(&y, blok[j], b);
        }
        for (j = 0; j < BLOK; j++)
        {
            if (kiyas_bit_genisligi(blok[j]) > b)
            {
                kiyas_bit_yaz(&y, (uint32_t)j, 8);
            }
        }
        if ((32 - b) > 0)
        {
            for (j = 0; j < BLOK; j++)
            {
                if (kiyas_bit_genisligi(blok[j]) > b)
                {
                    kiyas_bit_yaz(&y, blok[j] >> b, 32 - b);
                }
            }
        }
    }

    p = kiyas_yazici_bitir(&y);
    if (p < 0) { return -1; }

    if (kuyruk > 0)
    {
        kt = kiyas_vbyte_kodla(&g[tam * BLOK], kuyruk, &c[p], kap - p);
        if (kt < 0) { return -1; }
        p += kt;
    }
    return p;
}

int32_t kiyas_optpfd_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet)
{
    int32_t tam    = adet / BLOK;
    int32_t kuyruk = adet % BLOK;
    kiyas_bit_okuyucu o;
    int32_t k;
    int32_t p;

    kiyas_okuyucu_kur(&o, g, boyut);

    for (k = 0; k < tam; k++)
    {
        int32_t b = (int32_t)kiyas_bit_oku(&o, 8);
        int32_t e = (int32_t)kiyas_bit_oku(&o, 8);
        int32_t konumlar[128];
        int32_t j;

        if ((b > 32) || (e > 128)) { return -1; }

        for (j = 0; j < BLOK; j++)
        {
            c[(k * BLOK) + j] = kiyas_bit_oku(&o, b);
        }
        for (j = 0; j < e; j++)
        {
            konumlar[j] = (int32_t)kiyas_bit_oku(&o, 8);
            if (konumlar[j] >= BLOK) { return -1; }
        }
        if ((32 - b) > 0)
        {
            for (j = 0; j < e; j++)
            {
                uint32_t ust = kiyas_bit_oku(&o, 32 - b);
                c[(k * BLOK) + konumlar[j]] |= (ust << b);
            }
        }
    }

    p = kiyas_okuyucu_bayt(&o);
    if (p > boyut) { return -1; }

    if (kuyruk > 0)
    {
        if (kiyas_vbyte_coz(&g[p], boyut - p, &c[tam * BLOK], kuyruk) < 0)
        {
            return -1;
        }
    }
    return 0;
}
