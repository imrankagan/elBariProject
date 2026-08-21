/* =====================================================================
 * KIYAS - rakip kodek uygulamalarinin oz testi
 * ---------------------------------------------------------------------
 * NEDEN GEREKLI:
 *   Kiyas tablosundaki ORAN sutunu ancak uygulamalar dogruysa anlamlidir.
 *   Tek bir veri seti uzerinde tam turun gecmesi yeterli kanit degildir:
 *   kuyruk yollari (blok boyutuna bolunmeyen adetler), sinir degerler
 *   (0, 2^31, 2^32-1), tamamen sifir bloklar ve tek elemanli girdiler
 *   gercek veride hic tetiklenmeyebilir.
 *
 *   Bu test, her kodegi 6 farkli deger dagilimi x 24 farkli uzunluk ile
 *   tam tur dogrulamasindan gecirir. Bir kodek burayi gecemiyorsa
 *   tablodaki orani da guvenilmez demektir.
 *
 * Rastgelelik deterministiktir (sabit tohumlu LCG): test tekrarlanabilir.
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kiyas.h"

/* Sabit tohumlu dogrusal eslesik uretec - zamana bagli degildir. */
static uint32_t s_durum = 0x1234567u;

static uint32_t rastgele(void)
{
    s_durum = (s_durum * 1664525u) + 1013904223u;
    return s_durum;
}

/* ---------------------------------------------------------------------
 * DEGER DAGILIMLARI
 * ------------------------------------------------------------------- */

typedef enum
{
    DAG_SIFIR = 0,      /* tamami sifir - sifir blok yollari          */
    DAG_KUCUK,          /* 0..3 - dar bit genisligi                    */
    DAG_ORTA,           /* 0..65535                                    */
    DAG_AYKIRILI,       /* cogunluk kucuk, %2 buyuk - istisna yollari  */
    DAG_TAM32,          /* tum 32 bit araligi                          */
    DAG_SINIR,          /* 0, 1, 2^31, 2^32-1 gibi sinir degerler      */
    DAG_ADEDI
} dagilim;

static const char *DAGILIM_ADI[DAG_ADEDI] =
{
    "sifir", "kucuk", "orta", "aykirili", "tam32", "sinir"
};

static void uret(dagilim d, uint32_t *cikti, int32_t adet)
{
    static const uint32_t SINIR_DEGERLER[8] =
    {
        0u, 1u, 2u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0xFFFFu, 0x10000u
    };
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        switch (d)
        {
        case DAG_SIFIR:    cikti[i] = 0u;                              break;
        case DAG_KUCUK:    cikti[i] = rastgele() & 3u;                 break;
        case DAG_ORTA:     cikti[i] = rastgele() & 0xFFFFu;            break;
        case DAG_AYKIRILI:
            cikti[i] = ((rastgele() % 100u) < 2u) ? rastgele()
                                                  : (rastgele() & 0x3Fu);
            break;
        case DAG_TAM32:    cikti[i] = rastgele();                      break;
        case DAG_SINIR:
        default:
            cikti[i] = SINIR_DEGERLER[rastgele() & 7u];
            break;
        }
    }
}

/* ---------------------------------------------------------------------
 * AILE A TESTI
 * ------------------------------------------------------------------- */

typedef struct
{
    const char    *ad;
    kiyas_kodla_fn kodla;
    kiyas_coz_fn   coz;
} test_kodek;

static const test_kodek TEST_KODEKLER[] =
{
    { "VByte",       kiyas_vbyte_kodla,       kiyas_vbyte_coz },
    { "StreamVByte", kiyas_streamvbyte_kodla, kiyas_streamvbyte_coz },
    { "Simple8b",    kiyas_simple8b_kodla,    kiyas_simple8b_coz },
    { "BP128",       kiyas_bp128_kodla,       kiyas_bp128_coz },
    { "OptPFD",      kiyas_optpfd_kodla,      kiyas_optpfd_coz }
};

#define TEST_KODEK_ADEDI ((int32_t)(sizeof(TEST_KODEKLER) / sizeof(TEST_KODEKLER[0])))

/* Blok sinirlarini (8, 128, 240) ve kuyruk yollarini kasten zorlayan
 * uzunluklar. */
static const int32_t UZUNLUKLAR[] =
{
    0, 1, 2, 3, 7, 8, 9, 15, 16, 60, 119, 120, 121, 127, 128, 129,
    239, 240, 241, 255, 256, 1000, 4097, 10000
};

#define UZUNLUK_ADEDI ((int32_t)(sizeof(UZUNLUKLAR) / sizeof(UZUNLUKLAR[0])))

static int32_t aile_a_testi(void)
{
    int32_t en_uzun = UZUNLUKLAR[UZUNLUK_ADEDI - 1];
    uint32_t *girdi = (uint32_t *)malloc((size_t)en_uzun * sizeof(uint32_t));
    uint32_t *geri  = (uint32_t *)malloc((size_t)en_uzun * sizeof(uint32_t));
    uint8_t  *tampon;
    int32_t   tampon_kap = (en_uzun * 9) + 4096;
    int32_t   hata = 0;
    int32_t   k;

    tampon = (uint8_t *)malloc((size_t)tampon_kap);
    if ((girdi == NULL) || (geri == NULL) || (tampon == NULL)) { return 1; }

    for (k = 0; k < TEST_KODEK_ADEDI; k++)
    {
        int32_t bu_hata = 0;
        int32_t d;

        for (d = 0; d < (int32_t)DAG_ADEDI; d++)
        {
            int32_t u;

            for (u = 0; u < UZUNLUK_ADEDI; u++)
            {
                int32_t adet = UZUNLUKLAR[u];
                int32_t n;

                uret((dagilim)d, girdi, adet);
                (void)memset(geri, 0xAB, (size_t)en_uzun * sizeof(uint32_t));

                n = TEST_KODEKLER[k].kodla(girdi, adet, tampon, tampon_kap);
                if (n < 0)
                {
                    printf("  [HATA] %-12s %-9s adet=%-6d kodlama basarisiz\n",
                           TEST_KODEKLER[k].ad, DAGILIM_ADI[d], (int)adet);
                    bu_hata++;
                    continue;
                }

                if (TEST_KODEKLER[k].coz(tampon, n, geri, adet) < 0)
                {
                    printf("  [HATA] %-12s %-9s adet=%-6d cozme basarisiz\n",
                           TEST_KODEKLER[k].ad, DAGILIM_ADI[d], (int)adet);
                    bu_hata++;
                    continue;
                }

                if ((adet > 0) &&
                    (memcmp(girdi, geri, (size_t)adet * sizeof(uint32_t)) != 0))
                {
                    printf("  [HATA] %-12s %-9s adet=%-6d veri bozuldu\n",
                           TEST_KODEKLER[k].ad, DAGILIM_ADI[d], (int)adet);
                    bu_hata++;
                }
            }
        }

        printf("  %-12s %s  (%d dagilim x %d uzunluk = %d durum)\n",
               TEST_KODEKLER[k].ad,
               (bu_hata == 0) ? "GECTI " : "KALDI ",
               (int)DAG_ADEDI, (int)UZUNLUK_ADEDI,
               (int)((int32_t)DAG_ADEDI * UZUNLUK_ADEDI));
        hata += bu_hata;
    }

    free(girdi);
    free(geri);
    free(tampon);
    return hata;
}

/* ---------------------------------------------------------------------
 * SPRINTZ TESTI (cok kanalli)
 * ------------------------------------------------------------------- */

static const int32_t KANAL_SAYILARI[] = { 1, 2, 3, 6, 9, 16 };
#define KANAL_ADEDI ((int32_t)(sizeof(KANAL_SAYILARI) / sizeof(KANAL_SAYILARI[0])))

static const int32_t KAYIT_SAYILARI[] = { 1, 2, 7, 8, 9, 16, 17, 100, 1000, 5000 };
#define KAYIT_ADEDI ((int32_t)(sizeof(KAYIT_SAYILARI) / sizeof(KAYIT_SAYILARI[0])))

static int32_t sprintz_testi(void)
{
    int32_t en_buyuk = 5000 * 16;
    int32_t *girdi = (int32_t *)malloc((size_t)en_buyuk * sizeof(int32_t));
    int32_t *geri  = (int32_t *)malloc((size_t)en_buyuk * sizeof(int32_t));
    uint8_t *tampon;
    int32_t  tampon_kap = (en_buyuk * 9) + 4096;
    int32_t  hata = 0;
    int32_t  d;

    tampon = (uint8_t *)malloc((size_t)tampon_kap);
    if ((girdi == NULL) || (geri == NULL) || (tampon == NULL)) { return 1; }

    for (d = 0; d < (int32_t)DAG_ADEDI; d++)
    {
        int32_t kn;

        for (kn = 0; kn < KANAL_ADEDI; kn++)
        {
            int32_t ky;

            for (ky = 0; ky < KAYIT_ADEDI; ky++)
            {
                int32_t kanal = KANAL_SAYILARI[kn];
                int32_t kayit = KAYIT_SAYILARI[ky];
                int32_t eleman = kanal * kayit;
                int32_t n;
                int32_t i;

                uret((dagilim)d, (uint32_t *)girdi, eleman);
                /* Zaman serisi benzeri kilmak icin kumulatif topla:
                 * fark kodlamanin gercekci bir girdiye uygulanmasi. */
                for (i = kanal; i < eleman; i++)
                {
                    girdi[i] = (int32_t)((uint32_t)girdi[i - kanal]
                                         + (uint32_t)girdi[i]);
                }
                (void)memset(geri, 0xAB, (size_t)eleman * sizeof(int32_t));

                n = kiyas_sprintz_kodla(girdi, eleman, kanal, tampon, tampon_kap);
                if (n < 0)
                {
                    printf("  [HATA] Sprintz %-9s kanal=%-3d kayit=%-5d kodlama\n",
                           DAGILIM_ADI[d], (int)kanal, (int)kayit);
                    hata++;
                    continue;
                }

                if (kiyas_sprintz_coz(tampon, n, eleman, kanal, geri) < 0)
                {
                    printf("  [HATA] Sprintz %-9s kanal=%-3d kayit=%-5d cozme\n",
                           DAGILIM_ADI[d], (int)kanal, (int)kayit);
                    hata++;
                    continue;
                }

                if (memcmp(girdi, geri, (size_t)eleman * sizeof(int32_t)) != 0)
                {
                    printf("  [HATA] Sprintz %-9s kanal=%-3d kayit=%-5d bozuldu\n",
                           DAGILIM_ADI[d], (int)kanal, (int)kayit);
                    hata++;
                }
            }
        }
    }

    printf("  %-12s %s  (%d dagilim x %d kanal x %d uzunluk = %d durum)\n",
           "Sprintz", (hata == 0) ? "GECTI " : "KALDI ",
           (int)DAG_ADEDI, (int)KANAL_ADEDI, (int)KAYIT_ADEDI,
           (int)((int32_t)DAG_ADEDI * KANAL_ADEDI * KAYIT_ADEDI));

    free(girdi);
    free(geri);
    free(tampon);
    return hata;
}

/* =====================================================================
 * ANA
 * ===================================================================== */

int main(void)
{
    int32_t hata;

    printf("=====================================================================\n");
    printf("  KIYAS - rakip kodek uygulamalarinin oz testi\n");
    printf("=====================================================================\n");
    printf("Amac: kiyas tablosundaki ORAN sutununun dayandigi uygulamalarin\n");
    printf("      dogrulugunu, gercek veri setinin tetiklemedigi yollarda da\n");
    printf("      (kuyruklar, sinir degerler, sifir bloklar) kanitlamak.\n\n");

    hata = aile_a_testi();
    hata += sprintz_testi();

    printf("\n");
    if (hata == 0)
    {
        printf("SONUC: tum kodekler tum durumlarda kayipsiz.\n");
    }
    else
    {
        printf("SONUC: %d BASARISIZ DURUM - kiyas tablosu guvenilmez!\n", (int)hata);
    }
    printf("=====================================================================\n");

    return (hata == 0) ? 0 : 1;
}
