/* =====================================================================
 * KIYAS - AILE B: Sprintz-Delta  (Blalock, Madden & Guttag, 2018)
 * ---------------------------------------------------------------------
 * "Sprintz: Time Series Compression for the Internet of Things"
 * Proc. ACM IMWUT, 2(3), 2018.
 *
 * NEDEN BU KODEK ONEMLI:
 *   Sprintz, ElBari'nin problem tanimina en yakin literatur calismasidir:
 *   cok kanalli, gomulu hedefli, kayipsiz tamsayi zaman serisi sikistirma.
 *   Ayni yapi taslarini kullanir - kanal ayrimi, ongorucu (fark), zigzag,
 *   bit paketleme, sifir blok kisayolu. Bu yuzden "ElBari ne kadar ozgun"
 *   sorusunun gercek muhatabi zstd degil, Sprintz'tir.
 *
 * BU UYGULAMANIN KAPSAMI - DURUST SINIRLAR:
 *   1) Sprintz-Delta uygulanmistir; Sprintz-Delta-Huf DEGIL. Yazarlarin
 *      tam surumu bit paketlemeden sonra bir de Huffman katmani calistirir
 *      ve orani bir miktar daha artirir (hiz karsiliginda). Buradaki
 *      surum, gomulu hedefte anlamli olan hizli varyanttir.
 *   2) Sprintz'in FIRE ongorucusu yerine duz fark (delta) kullanilmistir.
 *      Makale her ikisini de sunar; FIRE duzgun sinyallerde biraz daha
 *      iyidir.
 *   3) Sprintz 8 ve 16 bitlik veri icin tasarlanmistir. Telemetride
 *      yaygin olan 32 bit tamsayiya UYARLANMISTIR: blok basi kanal
 *      genisligi 6 bit ile tutulur (0..32). Bu bir GENISLETMEDIR,
 *      yazarlarin bicimi degildir.
 *
 * BICIM:
 *   [ilk kayit: kanal x 32 bit ham]
 *   sonra 8 ornekli bloklar:
 *     1 bit  : 1 = sifir kosusu, 0 = normal blok
 *     kosu   : 8 bit blok sayisi (1..255) - artik tamamen sifir
 *     normal : kanal x 6 bit genislik, sonra kanal x 8 x genislik bit
 *   kuyruk (< 8 kayit): her deger 32 bit ham
 * ===================================================================== */

#include "kiyas.h"

#define SPR_BLOK (8)

/* ---------------------------------------------------------------------
 * Bir blogun tum artiklari sifir mi? (yani kayit hic degismemis mi)
 * kayit_no: blogun ilk kaydinin indeksi (>= 1 olmali)
 * ------------------------------------------------------------------- */
static int32_t spr_sifir_blok(const int32_t *kayitlar, int32_t kanal_sayisi,
                              int32_t kayit_no)
{
    int32_t i;
    int32_t c;

    for (i = 0; i < SPR_BLOK; i++)
    {
        const int32_t *simdi  = &kayitlar[(kayit_no + i) * kanal_sayisi];
        const int32_t *onceki = &kayitlar[((kayit_no + i) - 1) * kanal_sayisi];

        for (c = 0; c < kanal_sayisi; c++)
        {
            if (simdi[c] != onceki[c]) { return 0; }
        }
    }
    return 1;
}

int32_t kiyas_sprintz_kodla(const int32_t *kayitlar, int32_t eleman_sayisi,
                            int32_t kanal_sayisi, uint8_t *cikti,
                            int32_t kapasite)
{
    int32_t kayit_sayisi;
    kiyas_bit_yazici y;
    int32_t r;
    int32_t c;

    if ((kanal_sayisi <= 0) || (kanal_sayisi > 255)) { return -1; }
    if ((eleman_sayisi % kanal_sayisi) != 0) { return -1; }

    kayit_sayisi = eleman_sayisi / kanal_sayisi;
    if (kayit_sayisi <= 0) { return 0; }

    kiyas_yazici_kur(&y, cikti, kapasite);

    /* Ilk kayit ham: ongorucunun mutlak baslangic noktasi. */
    for (c = 0; c < kanal_sayisi; c++)
    {
        kiyas_bit_yaz(&y, (uint32_t)kayitlar[c], 32);
    }

    r = 1;
    while ((r + SPR_BLOK) <= kayit_sayisi)
    {
        int32_t genislikler[256];
        int32_t i;

        if (spr_sifir_blok(kayitlar, kanal_sayisi, r) != 0)
        {
            int32_t kosu = 1;

            while ((kosu < 255) &&
                   ((r + ((kosu + 1) * SPR_BLOK)) <= kayit_sayisi) &&
                   (spr_sifir_blok(kayitlar, kanal_sayisi,
                                   r + (kosu * SPR_BLOK)) != 0))
            {
                kosu++;
            }

            kiyas_bit_yaz(&y, 1u, 1);
            kiyas_bit_yaz(&y, (uint32_t)kosu, 8);
            r += kosu * SPR_BLOK;
            continue;
        }

        kiyas_bit_yaz(&y, 0u, 1);

        for (c = 0; c < kanal_sayisi; c++)
        {
            int32_t b = 0;
            for (i = 0; i < SPR_BLOK; i++)
            {
                const int32_t *simdi  = &kayitlar[(r + i) * kanal_sayisi];
                const int32_t *onceki = &kayitlar[((r + i) - 1) * kanal_sayisi];
                uint32_t fark = (uint32_t)simdi[c] - (uint32_t)onceki[c];
                int32_t  w    = kiyas_bit_genisligi(kiyas_zigzag((int32_t)fark));
                if (w > b) { b = w; }
            }
            genislikler[c] = b;
            kiyas_bit_yaz(&y, (uint32_t)b, 6);
        }

        for (c = 0; c < kanal_sayisi; c++)
        {
            for (i = 0; i < SPR_BLOK; i++)
            {
                const int32_t *simdi  = &kayitlar[(r + i) * kanal_sayisi];
                const int32_t *onceki = &kayitlar[((r + i) - 1) * kanal_sayisi];
                uint32_t fark = (uint32_t)simdi[c] - (uint32_t)onceki[c];
                kiyas_bit_yaz(&y, kiyas_zigzag((int32_t)fark), genislikler[c]);
            }
        }

        r += SPR_BLOK;
    }

    /* Kuyruk: 8'e tamamlanmayan artik kayitlar ham yazilir. */
    for (; r < kayit_sayisi; r++)
    {
        for (c = 0; c < kanal_sayisi; c++)
        {
            kiyas_bit_yaz(&y, (uint32_t)kayitlar[(r * kanal_sayisi) + c], 32);
        }
    }

    return kiyas_yazici_bitir(&y);
}

int32_t kiyas_sprintz_coz(const uint8_t *girdi, int32_t girdi_boyutu,
                          int32_t eleman_sayisi, int32_t kanal_sayisi,
                          int32_t *cikti)
{
    int32_t kayit_sayisi;
    kiyas_bit_okuyucu o;
    int32_t r;
    int32_t c;

    if ((kanal_sayisi <= 0) || (kanal_sayisi > 255)) { return -1; }
    if ((eleman_sayisi % kanal_sayisi) != 0) { return -1; }

    kayit_sayisi = eleman_sayisi / kanal_sayisi;
    if (kayit_sayisi <= 0) { return 0; }

    kiyas_okuyucu_kur(&o, girdi, girdi_boyutu);

    for (c = 0; c < kanal_sayisi; c++)
    {
        cikti[c] = (int32_t)kiyas_bit_oku(&o, 32);
    }

    r = 1;
    while ((r + SPR_BLOK) <= kayit_sayisi)
    {
        int32_t genislikler[256];
        int32_t i;

        if (kiyas_bit_oku(&o, 1) == 1u)
        {
            int32_t kosu = (int32_t)kiyas_bit_oku(&o, 8);
            int32_t adet = kosu * SPR_BLOK;

            if ((kosu <= 0) || ((r + adet) > kayit_sayisi)) { return -1; }

            for (i = 0; i < adet; i++)
            {
                int32_t *simdi        = &cikti[(r + i) * kanal_sayisi];
                const int32_t *onceki = &cikti[((r + i) - 1) * kanal_sayisi];
                for (c = 0; c < kanal_sayisi; c++)
                {
                    simdi[c] = onceki[c];
                }
            }
            r += adet;
            continue;
        }

        for (c = 0; c < kanal_sayisi; c++)
        {
            genislikler[c] = (int32_t)kiyas_bit_oku(&o, 6);
            if (genislikler[c] > 32) { return -1; }
        }

        for (c = 0; c < kanal_sayisi; c++)
        {
            for (i = 0; i < SPR_BLOK; i++)
            {
                uint32_t zz   = kiyas_bit_oku(&o, genislikler[c]);
                uint32_t fark = (uint32_t)kiyas_zigzag_ters(zz);
                int32_t *simdi        = &cikti[(r + i) * kanal_sayisi];
                const int32_t *onceki = &cikti[((r + i) - 1) * kanal_sayisi];
                simdi[c] = (int32_t)((uint32_t)onceki[c] + fark);
            }
        }

        r += SPR_BLOK;
    }

    for (; r < kayit_sayisi; r++)
    {
        for (c = 0; c < kanal_sayisi; c++)
        {
            cikti[(r * kanal_sayisi) + c] = (int32_t)kiyas_bit_oku(&o, 32);
        }
    }

    if (kiyas_okuyucu_bayt(&o) > (girdi_boyutu + 8)) { return -1; }
    return 0;
}
