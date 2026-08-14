/* =====================================================================
 * ELBARI - C surumu performans ve gecikme dagilimi olcumu
 * ---------------------------------------------------------------------
 * AMAC:
 *   1) C surumunun verimi (MB/sn) - README'deki bosluk
 *   2) CERCEVE BASINA GECIKME DAGILIMI - "deterministik" iddiasinin sinavi
 *
 * NEDEN DAGILIM, NEDEN ORTALAMA DEGIL:
 *   Gercek zamanli bir sistemde ortalama gecikme neredeyse anlamsizdir.
 *   Onemli olan "en kotu ihtimalle ne kadar surer" sorusudur. Bu yuzden
 *   asagida medyan, p95, p99, p99.9 ve en buyuk deger ayri ayri raporlanir.
 *
 * DURUSTLUK NOTU:
 *   Bu olcum genel amacli bir isletim sistemi uzerinde yapilmaktadir.
 *   En yuksek yuzdelikler buyuk olcude ISLETIM SISTEMI GURULTUSUNU
 *   (zamanlayici kesintileri, sayfa hatalari, frekans olcekleme)
 *   yansitir; algoritmanin kendisini degil. Gercek en-kotu-durum
 *   (WCET) analizi ancak bir RTOS uzerinde ve statik analizle yapilir.
 *
 *   Buna karsilik ALGORITMIK degiskenlik olculebilir: ayni boyuttaki
 *   cerceveler farkli VERI uzerinde farkli surelerde islenirse bu
 *   algoritmadan kaynaklanir. Asagida bu ikisi ayri raporlanir.
 * ===================================================================== */

/* MSVC fopen uyarisi: test kodu, tasinabilirlik icin standart fopen kullanilir */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/elbari.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <time.h>
#endif

/* ---------------------------------------------------------------------
 * YUKSEK COZUNURLUKLU ZAMANLAYICI
 * ------------------------------------------------------------------- */

static double zamanlayici_frekansi(void)
{
#if defined(_WIN32)
    LARGE_INTEGER f;
    (void)QueryPerformanceFrequency(&f);
    return (double)f.QuadPart;
#else
    return 1000000000.0;
#endif
}

static long long zaman_oku(void)
{
#if defined(_WIN32)
    LARGE_INTEGER t;
    (void)QueryPerformanceCounter(&t);
    return (long long)t.QuadPart;
#else
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((long long)ts.tv_sec * 1000000000LL) + (long long)ts.tv_nsec;
#endif
}

/* ---------------------------------------------------------------------
 * YUZDELIK HESABI
 * ------------------------------------------------------------------- */

static int cift_karsilastir(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) { return -1; }
    if (da > db) { return 1; }
    return 0;
}

static double yuzdelik(const double *sirali, int adet, double oran)
{
    int indeks = (int)((double)(adet - 1) * oran);
    if (indeks < 0) { indeks = 0; }
    if (indeks >= adet) { indeks = adet - 1; }
    return sirali[indeks];
}

/* ---------------------------------------------------------------------
 * DOSYA OKUMA
 * ------------------------------------------------------------------- */

static unsigned char *dosya_oku(const char *yol, long *boyut_cikti)
{
    FILE *f = fopen(yol, "rb");
    unsigned char *tampon;
    long boyut;
    size_t okunan;

    if (f == NULL) { return NULL; }

    (void)fseek(f, 0, SEEK_END);
    boyut = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    if (boyut <= 0) { (void)fclose(f); return NULL; }

    tampon = (unsigned char *)malloc((size_t)boyut);
    if (tampon == NULL) { (void)fclose(f); return NULL; }

    okunan = fread(tampon, 1, (size_t)boyut, f);
    (void)fclose(f);
    if (okunan != (size_t)boyut) { free(tampon); return NULL; }

    *boyut_cikti = boyut;
    return tampon;
}

/* =====================================================================
 * ANA
 * ===================================================================== */

int main(int argc, char **argv)
{
    char yol[1024];
    unsigned char *girdi_ham;
    long girdi_boy = 0;
    int32_t kanal_sayisi;
    int32_t eleman_sayisi;
    int32_t *veri;
    int32_t kayit_sayisi;
    double frekans;

    if (argc < 2)
    {
        (void)fprintf(stderr, "Kullanim: olcum <referans_dizini>\n");
        return 2;
    }

    (void)snprintf(yol, sizeof(yol), "%s/girdi.bin", argv[1]);
    girdi_ham = dosya_oku(yol, &girdi_boy);
    if (girdi_ham == NULL)
    {
        (void)fprintf(stderr, "HATA: girdi.bin okunamadi: %s\n", yol);
        return 2;
    }

    (void)memcpy(&kanal_sayisi, &girdi_ham[0], 4);
    (void)memcpy(&eleman_sayisi, &girdi_ham[4], 4);

    veri = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
    if (veri == NULL) { return 2; }
    (void)memcpy(veri, &girdi_ham[8], (size_t)eleman_sayisi * sizeof(int32_t));

    kayit_sayisi = eleman_sayisi / kanal_sayisi;
    frekans = zamanlayici_frekansi();

    printf("=====================================================================\n");
    printf("  ELBARI C surumu - verim ve gecikme dagilimi\n");
    printf("=====================================================================\n");
    printf("Veri  : %d kayit x %d kanal = %d eleman (%d bayt ham)\n",
           (int)kayit_sayisi, (int)kanal_sayisi, (int)eleman_sayisi,
           (int)(eleman_sayisi * 4));
    printf("Derleme: saf skaler C (SIMD yok)\n\n");

    /* =================================================================
     * BOLUM 1 - VERIM (tum akis, kanal katmani)
     * ================================================================= */
    {
        int32_t calisma_kap = elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi);
        int32_t cikti_kap = elbari_kanal_en_kotu_durum_boyutu(eleman_sayisi, kanal_sayisi);
        int32_t *calisma = (int32_t *)malloc((size_t)calisma_kap * sizeof(int32_t));
        int32_t *geri = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
        uint8_t *cikti = (uint8_t *)malloc((size_t)cikti_kap);
        const int TUR = 200;
        int t;
        long long bas;
        long long son;
        double enc_sn;
        double dec_sn;
        int32_t boyut = 0;

        if ((calisma == NULL) || (geri == NULL) || (cikti == NULL)) { return 2; }

        /* Isinma */
        for (t = 0; t < 20; t++)
        {
            boyut = elbari_kanal_kabid(veri, eleman_sayisi, kanal_sayisi,
                                       calisma, calisma_kap, cikti, cikti_kap);
            (void)elbari_kanal_basit(cikti, boyut, calisma, calisma_kap,
                                     geri, eleman_sayisi);
        }

        bas = zaman_oku();
        for (t = 0; t < TUR; t++)
        {
            boyut = elbari_kanal_kabid(veri, eleman_sayisi, kanal_sayisi,
                                       calisma, calisma_kap, cikti, cikti_kap);
        }
        son = zaman_oku();
        enc_sn = ((double)(son - bas) / frekans) / (double)TUR;

        bas = zaman_oku();
        for (t = 0; t < TUR; t++)
        {
            (void)elbari_kanal_basit(cikti, boyut, calisma, calisma_kap,
                                     geri, eleman_sayisi);
        }
        son = zaman_oku();
        dec_sn = ((double)(son - bas) / frekans) / (double)TUR;

        printf("--- BOLUM 1: Verim (kanal katmani, tum akis) ---\n");
        printf("  sikistirma : %d -> %d bayt (%.2fx)\n",
               (int)(eleman_sayisi * 4), (int)boyut,
               (double)(eleman_sayisi * 4) / (double)boyut);
        printf("  encode     : %9.0f ns   %8.0f MB/sn   %11.0f kayit/sn\n",
               enc_sn * 1e9,
               ((double)(eleman_sayisi * 4) / enc_sn) / (1024.0 * 1024.0),
               (double)kayit_sayisi / enc_sn);
        printf("  decode     : %9.0f ns   %8.0f MB/sn   %11.0f kayit/sn\n\n",
               dec_sn * 1e9,
               ((double)(eleman_sayisi * 4) / dec_sn) / (1024.0 * 1024.0),
               (double)kayit_sayisi / dec_sn);

        free(calisma);
        free(geri);
        free(cikti);
    }

    /* =================================================================
     * BOLUM 2 - CERCEVE BASINA GECIKME DAGILIMI
     * =================================================================
     * Gercek zamanli sistemde onemli olan birim budur: bir cerceveyi
     * hazirlamak ne kadar surer, ve bu sure ne kadar oynar?
     * ================================================================= */
    {
        const int32_t KPC = 100;   /* cerceve basina kayit */
        const int TEKRAR = 200;    /* her cerceve icin olcum tekrari */
        int32_t calisma_kap = elbari_cerceve_gerekli_calisma_alani(KPC, kanal_sayisi);
        int32_t paket_kap = elbari_cerceve_en_kotu_durum_boyutu(KPC, kanal_sayisi);
        int32_t *calisma = (int32_t *)malloc((size_t)calisma_kap * sizeof(int32_t));
        uint8_t *paket = (uint8_t *)malloc((size_t)paket_kap);
        int32_t *cerceve_cikti = (int32_t *)malloc((size_t)(KPC * kanal_sayisi) * sizeof(int32_t));
        int32_t tam_cerceve_sayisi = kayit_sayisi / KPC;
        int toplam_olcum = tam_cerceve_sayisi * TEKRAR;
        double *enc_ornekler = (double *)malloc((size_t)toplam_olcum * sizeof(double));
        double *dec_ornekler = (double *)malloc((size_t)toplam_olcum * sizeof(double));
        double *veri_ortalamalari = (double *)malloc((size_t)tam_cerceve_sayisi * sizeof(double));
        int enc_adet = 0;
        int dec_adet = 0;
        int32_t k;
        int r;

        if ((calisma == NULL) || (paket == NULL) || (cerceve_cikti == NULL) ||
            (enc_ornekler == NULL) || (dec_ornekler == NULL) || (veri_ortalamalari == NULL))
        {
            return 2;
        }

        /* Isinma */
        for (r = 0; r < 50; r++)
        {
            (void)elbari_cerceve_yaz(veri, KPC * kanal_sayisi, kanal_sayisi, 0u,
                                     calisma, calisma_kap, paket, paket_kap);
        }

        /* Her cerceve, TEKRAR kez olculur. Boylece hem isletim sistemi
         * gurultusu (ayni veri, farkli sureler) hem de veriye bagli
         * degiskenlik (farkli veri, farkli sureler) gorulebilir. */
        for (k = 0; k < tam_cerceve_sayisi; k++)
        {
            const int32_t *dilim = &veri[(k * KPC) * kanal_sayisi];
            double bu_cerceve_toplam = 0.0;
            int32_t yazilan = 0;

            for (r = 0; r < TEKRAR; r++)
            {
                long long bas = zaman_oku();
                yazilan = elbari_cerceve_yaz(dilim, KPC * kanal_sayisi, kanal_sayisi,
                                             (uint32_t)k, calisma, calisma_kap,
                                             paket, paket_kap);
                {
                    long long son = zaman_oku();
                    double sure_us = ((double)(son - bas) / frekans) * 1e6;
                    enc_ornekler[enc_adet] = sure_us;
                    enc_adet++;
                    bu_cerceve_toplam += sure_us;
                }
            }
            veri_ortalamalari[k] = bu_cerceve_toplam / (double)TEKRAR;

            for (r = 0; r < TEKRAR; r++)
            {
                long long bas = zaman_oku();
                (void)elbari_cerceve_oku(paket, yazilan, kanal_sayisi,
                                         calisma, calisma_kap,
                                         cerceve_cikti, KPC * kanal_sayisi,
                                         NULL, NULL);
                {
                    long long son = zaman_oku();
                    dec_ornekler[dec_adet] = ((double)(son - bas) / frekans) * 1e6;
                    dec_adet++;
                }
            }
        }

        qsort(enc_ornekler, (size_t)enc_adet, sizeof(double), cift_karsilastir);
        qsort(dec_ornekler, (size_t)dec_adet, sizeof(double), cift_karsilastir);

        printf("--- BOLUM 2: Cerceve basina gecikme (100 kayit, %d cerceve x %d tekrar) ---\n",
               (int)tam_cerceve_sayisi, TEKRAR);
        printf("  %-10s %10s %10s %10s %10s %10s %10s\n",
               "islem", "en kucuk", "medyan", "p95", "p99", "p99.9", "en buyuk");
        printf("  %-10s %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f\n", "encode us",
               enc_ornekler[0],
               yuzdelik(enc_ornekler, enc_adet, 0.50),
               yuzdelik(enc_ornekler, enc_adet, 0.95),
               yuzdelik(enc_ornekler, enc_adet, 0.99),
               yuzdelik(enc_ornekler, enc_adet, 0.999),
               enc_ornekler[enc_adet - 1]);
        printf("  %-10s %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f\n", "decode us",
               dec_ornekler[0],
               yuzdelik(dec_ornekler, dec_adet, 0.50),
               yuzdelik(dec_ornekler, dec_adet, 0.95),
               yuzdelik(dec_ornekler, dec_adet, 0.99),
               yuzdelik(dec_ornekler, dec_adet, 0.999),
               dec_ornekler[dec_adet - 1]);

        printf("\n  Oynama orani (p99 / medyan):  encode %.2fx   decode %.2fx\n",
               yuzdelik(enc_ornekler, enc_adet, 0.99) / yuzdelik(enc_ornekler, enc_adet, 0.50),
               yuzdelik(dec_ornekler, dec_adet, 0.99) / yuzdelik(dec_ornekler, dec_adet, 0.50));

        /* --- Veriye bagli degiskenlik: bu ALGORITMIKTIR, OS gurultusu degil --- */
        {
            double en_kucuk = veri_ortalamalari[0];
            double en_buyuk = veri_ortalamalari[0];
            int32_t j;

            for (j = 1; j < tam_cerceve_sayisi; j++)
            {
                if (veri_ortalamalari[j] < en_kucuk) { en_kucuk = veri_ortalamalari[j]; }
                if (veri_ortalamalari[j] > en_buyuk) { en_buyuk = veri_ortalamalari[j]; }
            }

            printf("\n--- BOLUM 3: Veriye bagli degiskenlik (algoritmik) ---\n");
            printf("  Her cerceve icin %d tekrarin ortalamasi alinarak isletim sistemi\n", TEKRAR);
            printf("  gurultusu bastirildi; kalan fark VERIDEN kaynaklanir.\n\n");
            printf("  en hizli cerceve : %8.2f us\n", en_kucuk);
            printf("  en yavas cerceve : %8.2f us\n", en_buyuk);
            printf("  algoritmik oran  : %8.2fx  (en yavas / en hizli)\n", en_buyuk / en_kucuk);
        }

        printf("\n  NOT: Ust yuzdelikler buyuk olcude isletim sistemi gurultusudur\n");
        printf("       (zamanlayici kesintisi, sayfa hatasi, frekans olcekleme).\n");
        printf("       Gercek en-kotu-durum analizi RTOS uzerinde yapilmalidir.\n");

        free(calisma);
        free(paket);
        free(cerceve_cikti);
        free(enc_ornekler);
        free(dec_ornekler);
        free(veri_ortalamalari);
    }

    printf("=====================================================================\n");

    free(veri);
    free(girdi_ham);
    return 0;
}
