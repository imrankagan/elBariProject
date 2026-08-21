/* =====================================================================
 * ELBARI - Paket kaybi altinda kurtarma supurmesi
 * ---------------------------------------------------------------------
 * SORU:
 *   Cerceve boyutu kac olmali?
 *
 *   Bu soru simdiye kadar UC kisitla olculuyordu: oran, gecikme ve paket
 *   boyutu (bkz. kiyas.c senaryo 3). Ama projenin ayirt edici ozelligi
 *   paket kaybi dayanikliligidir ve DORDUNCU eksen - kayip altinda ne
 *   kadarinin kurtarildigi - olculmemisti. Elde tek bir isletim noktasi
 *   vardi (100 kayit/cerceve), egri yoktu.
 *
 *   Bu arac egriyi cikarir.
 *
 * CELISKI - optimumu bu belirler:
 *   BUYUK cerceve  -> daha iyi oran, ama MTU'ya sigmayip birden cok
 *                     pakete bolunur. Cerceve ancak TUM parcalari
 *                     ulasirsa cozulur; hayatta kalma olasiligi paket
 *                     sayisiyla USTEL azalir.
 *   KUCUK cerceve  -> her cerceve tek pakete siger ve bagimsiz hayatta
 *                     kalir, ama basligin payi buyudugu icin oran duser.
 *
 *   Yani oran ve dayaniklilik TERS yonde caliisir. Optimum ikisinin
 *   carpimindadir; asagidaki "etkin oran" sutunu tam olarak budur:
 *
 *       etkin oran = (ham bayt x kurtarma orani) / gonderilen bayt
 *
 *   Yani gonderilen her bayta karsilik ALICIYA ULASAN ham veri. Tek
 *   basina oran da, tek basina kurtarma da yanittir degildir; bu carpim
 *   yanittir.
 *
 * KAYIP MODELI:
 *   Gercek RF linkleri bagimsiz degil, PATLAMALI kaybeder (girisim,
 *   coklu yol sonmesi, anten yonelimi). Bu yuzden iki model var:
 *
 *   - Bernoulli (patlama uzunlugu 1): her paket bagimsiz duser.
 *   - Gilbert-Elliott: iki durumlu Markov zinciri. Iyi durumda kayip
 *     yok, Kotu durumda her paket duser. Hedef kayip orani L ve ortalama
 *     patlama uzunlugu B icin:
 *         r = 1/B            (Kotu'dan cikma olasiligi)
 *         p = L*r / (1-L)    (Kotu'ya girme olasiligi)
 *     Durgun durumda P(Kotu) = p/(p+r) = L olur.
 *
 * DURUSTLUK:
 *   Kurtarma aritmetikle TAHMIN EDILMEZ. Dusen paketler gercekten
 *   atilir, hayatta kalan cerceveler gercekten cozulur ve geri gelen
 *   kayitlar orijinalle BIT BIT karsilastirilir. Sayilan sey, dogru
 *   cozulmus kayit sayisidir.
 * ===================================================================== */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/elbari.h"

/* Tipik SiK / RFD900 sinifi telsizlerde kullanilabilir yuk. */
#define VARSAYILAN_MTU   (250)

/* Istatistiksel oynamayi bastirmak icin tur sayisi. */
#define TUR              (20)

/* Bellek makul kalsin diye kullanilan azami kayit. */
#define MAKS_KAYIT       (200000)

#define MAKS_CERCEVE     (MAKS_KAYIT + 8)

/* --------------------------------------------------------------------- */

static unsigned long long g_tohum = 0x243F6A8885A308D3ull;

static unsigned int rastgele(void)
{
    g_tohum ^= g_tohum << 13;
    g_tohum ^= g_tohum >> 7;
    g_tohum ^= g_tohum << 17;
    return (unsigned int)(g_tohum >> 32);
}

/** [0,1) araliginda kayan nokta. */
static double rastgele_kesir(void)
{
    return (double)rastgele() / 4294967296.0;
}

/* =====================================================================
 * DOSYA
 * ===================================================================== */

static int32_t *fikstur_oku(const char *yol, int32_t *kanal_cikti,
                            int32_t *kayit_cikti)
{
    FILE *f = fopen(yol, "rb");
    int32_t kanal = 0;
    int32_t eleman = 0;
    int32_t *veri;
    int32_t kayit;

    if (f == NULL) { return NULL; }
    if (fread(&kanal, 4, 1, f) != 1u) { (void)fclose(f); return NULL; }
    if (fread(&eleman, 4, 1, f) != 1u) { (void)fclose(f); return NULL; }
    if ((kanal < 1) || (kanal > ELBARI_MAKS_KANAL) || (eleman < kanal))
    {
        (void)fclose(f);
        return NULL;
    }

    kayit = eleman / kanal;
    if (kayit > MAKS_KAYIT) { kayit = MAKS_KAYIT; }
    eleman = kayit * kanal;

    veri = (int32_t *)malloc((size_t)eleman * sizeof(int32_t));
    if (veri == NULL) { (void)fclose(f); return NULL; }
    if (fread(veri, 4, (size_t)eleman, f) != (size_t)eleman)
    {
        free(veri);
        (void)fclose(f);
        return NULL;
    }
    (void)fclose(f);

    *kanal_cikti = kanal;
    *kayit_cikti = kayit;
    return veri;
}

/* =====================================================================
 * CERCEVELEME
 * ===================================================================== */

typedef struct
{
    int32_t  adet;                       /* uretilen cerceve sayisi     */
    long     toplam_bayt;                /* link'e verilen toplam bayt  */
    int32_t  ofset[MAKS_CERCEVE];        /* cikti icindeki yeri         */
    int32_t  boy[MAKS_CERCEVE];          /* cerceve bayt boyu           */
    int32_t  kayit[MAKS_CERCEVE];        /* icindeki kayit sayisi       */
    int32_t  paket[MAKS_CERCEVE];        /* MTU'ya bolununce kac parca  */
    long     toplam_paket;
} cerceve_plani;

/**
 * Veriyi kpc kayitlik cercevelere boler ve her cercevenin MTU'ya kac
 * pakete bolundugunu hesaplar.
 * @return 0 basarili, < 0 hata
 */
static int32_t cerceveleri_uret(const int32_t *veri, int32_t kayit_sayisi,
                                int32_t kanal, int32_t kpc, int32_t mtu,
                                uint8_t *cikti, int32_t cikti_kap,
                                int32_t *calisma, int32_t calisma_kap,
                                cerceve_plani *plan)
{
    int32_t r = 0;
    int32_t p = 0;
    uint32_t sira = 0u;

    plan->adet = 0;
    plan->toplam_bayt = 0;
    plan->toplam_paket = 0;

    while (r < kayit_sayisi)
    {
        int32_t kac = kayit_sayisi - r;
        int32_t n;

        if (kac > kpc) { kac = kpc; }
        if (plan->adet >= MAKS_CERCEVE) { return -1; }

        n = elbari_cerceve_yaz(&veri[r * kanal], kac * kanal, kanal, sira,
                               calisma, calisma_kap, &cikti[p], cikti_kap - p);
        if (n < 0) { return n; }

        plan->ofset[plan->adet] = p;
        plan->boy[plan->adet] = n;
        plan->kayit[plan->adet] = kac;
        /* MTU'ya bolme: tavana yuvarlanmis parca sayisi. */
        plan->paket[plan->adet] = ((n + mtu) - 1) / mtu;

        plan->toplam_bayt += (long)n;
        plan->toplam_paket += (long)plan->paket[plan->adet];
        plan->adet++;
        p += n;
        r += kac;
        sira++;
    }
    return 0;
}

/* =====================================================================
 * KAYIP MODELI
 * ---------------------------------------------------------------------
 * Gilbert-Elliott: Iyi durumda kayip yok, Kotu durumda her paket duser.
 * Patlama uzunlugu 1 verilirse Bernoulli'ye yaklasir.
 * ===================================================================== */

typedef struct
{
    double p;        /* Iyi -> Kotu */
    double r;        /* Kotu -> Iyi */
    int32_t kotu;    /* su anki durum */
} kanal_durumu;

static void kanal_kur(kanal_durumu *k, double kayip_orani,
                      double patlama_uzunlugu)
{
    if (patlama_uzunlugu < 1.0) { patlama_uzunlugu = 1.0; }
    k->r = 1.0 / patlama_uzunlugu;
    k->p = (kayip_orani >= 1.0)
           ? 1.0
           : ((kayip_orani * k->r) / (1.0 - kayip_orani));
    k->kotu = 0;
}

/** 1 = paket dustu, 0 = ulasti. */
static int32_t paket_dustu(kanal_durumu *k)
{
    int32_t dusuyor;

    if (k->kotu != 0)
    {
        dusuyor = 1;
        if (rastgele_kesir() < k->r) { k->kotu = 0; }
    }
    else
    {
        dusuyor = 0;
        if (rastgele_kesir() < k->p) { k->kotu = 1; }
    }
    return dusuyor;
}

/* =====================================================================
 * TEK TUR: paketleri dusur, hayatta kalanlari GERCEKTEN coz
 * ===================================================================== */

static long tek_tur(const int32_t *veri, int32_t kanal,
                    const cerceve_plani *plan, const uint8_t *cikti,
                    double kayip, double patlama,
                    int32_t *calisma, int32_t calisma_kap,
                    int32_t *geri, int32_t geri_kap)
{
    kanal_durumu kd;
    long kurtarilan = 0;
    int32_t c;
    int32_t r0 = 0;

    kanal_kur(&kd, kayip, patlama);

    for (c = 0; c < plan->adet; c++)
    {
        int32_t saglam = 1;
        int32_t i;

        /* Cerceve ancak TUM parcalari ulasirsa cozulebilir. */
        for (i = 0; i < plan->paket[c]; i++)
        {
            if (paket_dustu(&kd) != 0) { saglam = 0; }
        }

        if (saglam != 0)
        {
            int32_t eleman = plan->kayit[c] * kanal;

            if (eleman <= geri_kap)
            {
                int32_t sonuc = elbari_cerceve_oku(&cikti[plan->ofset[c]],
                                                   plan->boy[c], kanal,
                                                   calisma, calisma_kap,
                                                   geri, eleman, NULL, NULL);
                if (sonuc == ELBARI_TAMAM)
                {
                    /* Cozuldu demek yetmez: BIT BIT dogru mu? */
                    if (memcmp(geri, &veri[r0 * kanal],
                               (size_t)eleman * sizeof(int32_t)) == 0)
                    {
                        kurtarilan += (long)plan->kayit[c];
                    }
                }
            }
        }
        r0 += plan->kayit[c];
    }
    return kurtarilan;
}

/* =====================================================================
 * ANA
 * ===================================================================== */

static const int32_t SUPURME[] = { 5, 10, 25, 50, 100, 200, 500, 1000 };
#define SUPURME_ADEDI ((int32_t)(sizeof(SUPURME) / sizeof(SUPURME[0])))

static const double KAYIPLAR[] = { 0.01, 0.05, 0.10, 0.25 };
#define KAYIP_ADEDI ((int32_t)(sizeof(KAYIPLAR) / sizeof(KAYIPLAR[0])))

static const double PATLAMALAR[] = { 1.0, 3.0, 10.0 };
#define PATLAMA_ADEDI ((int32_t)(sizeof(PATLAMALAR) / sizeof(PATLAMALAR[0])))

int main(int argc, char **argv)
{
    const char *yol;
    int32_t mtu = VARSAYILAN_MTU;
    int32_t kanal = 0;
    int32_t kayit_sayisi = 0;
    int32_t *veri;
    int32_t *calisma;
    int32_t *geri;
    uint8_t *cikti;
    int32_t cikti_kap;
    int32_t calisma_kap;
    long ham_bayt;
    int32_t u;
    int32_t b;
    cerceve_plani *plan;

    if (argc < 2)
    {
        (void)fprintf(stderr,
                      "Kullanim: kayip <fikstur.bin> [mtu]\n"
                      "  fikstur: [int32 kanal][int32 eleman][int32 veri...]\n"
                      "  mtu    : telsiz paket yuku, varsayilan %d bayt\n",
                      (int)VARSAYILAN_MTU);
        return 2;
    }
    yol = argv[1];
    if (argc >= 3)
    {
        mtu = atoi(argv[2]);
        if (mtu < 32) { mtu = 32; }
    }

    veri = fikstur_oku(yol, &kanal, &kayit_sayisi);
    if (veri == NULL)
    {
        (void)fprintf(stderr, "HATA: fikstur okunamadi: %s\n", yol);
        return 2;
    }

    ham_bayt = (long)kayit_sayisi * (long)kanal * 4L;

    /* En kotu durum: cerceve basina 1 kayit. */
    cikti_kap = kayit_sayisi
                * elbari_cerceve_en_kotu_durum_boyutu(1, kanal) + 65536;
    calisma_kap = elbari_cerceve_gerekli_calisma_alani(SUPURME[SUPURME_ADEDI - 1],
                                                       kanal);
    {
        int32_t k2 = elbari_kanal_gerekli_calisma_alani(kayit_sayisi * kanal,
                                                        kanal);
        if (k2 > calisma_kap) { calisma_kap = k2; }
    }

    cikti   = (uint8_t *)malloc((size_t)cikti_kap);
    calisma = (int32_t *)malloc((size_t)calisma_kap * sizeof(int32_t));
    geri    = (int32_t *)malloc((size_t)kayit_sayisi * (size_t)kanal
                                * sizeof(int32_t));
    plan    = (cerceve_plani *)malloc(sizeof(cerceve_plani));

    if ((cikti == NULL) || (calisma == NULL) || (geri == NULL) || (plan == NULL))
    {
        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
        return 2;
    }

    printf("=====================================================================\n");
    printf("  ELBARI - Paket kaybi altinda kurtarma supurmesi\n");
    printf("=====================================================================\n");
    printf("Veri     : %s\n", yol);
    printf("Boyut    : %d kayit x %d kanal  (%ld bayt ham)\n",
           (int)kayit_sayisi, (int)kanal, ham_bayt);
    printf("Telsiz   : MTU %d bayt  |  Tur: %d (istatistiksel ortalama)\n",
           (int)mtu, (int)TUR);
    printf("Kayip    : Gilbert-Elliott; Iyi durumda kayip yok, Kotu durumda\n");
    printf("           her paket duser. Patlama uzunlugu = ortalama Kotu suresi.\n");
    printf("Kurtarma : tahmin DEGIL - hayatta kalan cerceveler gercekten\n");
    printf("           cozulur ve orijinalle BIT BIT karsilastirilir.\n\n");

    /* ---------------------------------------------------------------
     * TABAN CIZGI: cercevesiz tek blok
     * ------------------------------------------------------------- */
    {
        int32_t n = elbari_kanal_kabid(veri, kayit_sayisi * kanal, kanal,
                                       calisma, calisma_kap, cikti, cikti_kap);
        if (n > 0)
        {
            long paket = ((long)n + mtu - 1L) / (long)mtu;
            double hayatta = 1.0;
            long q;

            /* (1-L)^paket -- matematik kutuphanesi kullanmadan. */
            for (q = 0; q < paket; q++) { hayatta *= 0.99; }

            printf("--- Taban cizgi: CERCEVESIZ tek blok ---\n");
            printf("  Boyut: %d bayt (%.2fx), %ld pakete bolunuyor.\n",
                   (int)n, (double)ham_bayt / (double)n, paket);
            printf("  Cerceve YOK: tek paket duserse akisin tamami gider.\n");
            printf("  %%1 kayipta hayatta kalma olasiligi: %.3g\n\n", hayatta);
        }
    }

    /* ---------------------------------------------------------------
     * SUPURME
     * ------------------------------------------------------------- */
    for (b = 0; b < PATLAMA_ADEDI; b++)
    {
        printf("--- Patlama uzunlugu %.0f paket %s ---\n", PATLAMALAR[b],
               (PATLAMALAR[b] <= 1.0) ? "(bagimsiz / Bernoulli)" : "(patlamali)");
        printf("  %6s %8s %7s %6s", "kayit", "gonderilen", "oran", "pkt/crc");
        {
            int32_t k;
            for (k = 0; k < KAYIP_ADEDI; k++)
            {
                printf("   %%%-2.0f kurt/etkin", KAYIPLAR[k] * 100.0);
            }
        }
        printf("\n  ");
        {
            int32_t d;
            for (d = 0; d < (30 + (KAYIP_ADEDI * 17)); d++) { printf("-"); }
        }
        printf("\n");

        for (u = 0; u < SUPURME_ADEDI; u++)
        {
            int32_t kpc = SUPURME[u];
            double oran;
            int32_t k;

            if (cerceveleri_uret(veri, kayit_sayisi, kanal, kpc, mtu,
                                 cikti, cikti_kap, calisma, calisma_kap,
                                 plan) != 0)
            {
                printf("  %6d  <-- cerceveleme hatasi\n", (int)kpc);
                continue;
            }

            oran = (double)ham_bayt / (double)plan->toplam_bayt;

            printf("  %6d %8ld %6.2fx %6.2f",
                   (int)kpc, plan->toplam_bayt, oran,
                   (double)plan->toplam_paket / (double)plan->adet);

            for (k = 0; k < KAYIP_ADEDI; k++)
            {
                long toplam = 0;
                int32_t t;
                double kurtarma;
                double etkin;

                for (t = 0; t < TUR; t++)
                {
                    toplam += tek_tur(veri, kanal, plan, cikti,
                                      KAYIPLAR[k], PATLAMALAR[b],
                                      calisma, calisma_kap,
                                      geri, kayit_sayisi * kanal);
                }
                kurtarma = (double)toplam
                           / ((double)TUR * (double)kayit_sayisi);
                etkin = ((double)ham_bayt * kurtarma)
                        / (double)plan->toplam_bayt;

                printf("   %5.1f%% %6.2fx", kurtarma * 100.0, etkin);
            }
            printf("\n");
        }
        printf("\n");
    }

    printf("---------------------------------------------------------------------\n");
    printf("  kurt   = kurtarilan kayit orani (bit bit dogrulandi)\n");
    printf("  etkin  = (ham bayt x kurtarma) / gonderilen bayt\n");
    printf("           yani GONDERILEN HER BAYTA KARSILIK aliciya ulasan ham veri.\n");
    printf("           Cerceve boyutunun optimumu bu sutunun tepesindedir:\n");
    printf("           tek basina oran da, tek basina kurtarma da yanit degildir.\n");
    printf("  pkt/crc= cerceve basina paket. 1'in ustune ciktigi anda cerceve\n");
    printf("           hayatta kalma olasiligi USTEL duser - tablodaki kirilma\n");
    printf("           noktasi tam olarak oradadir.\n");
    printf("=====================================================================\n");

    free(veri);
    free(cikti);
    free(calisma);
    free(geri);
    free(plan);
    return 0;
}
