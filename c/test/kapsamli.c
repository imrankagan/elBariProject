/* =====================================================================
 * ELBARI - Kapsamli olcum (C surumu)
 * ---------------------------------------------------------------------
 * C# tarafinin urettigi AYNI veri setlerini okuyup AYNI olculeri alir.
 * Boylece iki surum yan yana karsilastirilabilir.
 *
 * KULLANIM:
 *   kapsamli <veri_dizini>
 *
 * Dizinde her veri seti su bicimde beklenir:
 *   [int32 kanal][int32 eleman][int32 x N]
 * ===================================================================== */

/* MSVC fopen uyarisi: test kodu, tasinabilirlik icin standart fopen kullanilir */
#define _CRT_SECURE_NO_WARNINGS

/* POSIX ozellik makrosu - herhangi bir #include'dan ONCE tanimlanmali.
 * clock_gettime ve CLOCK_MONOTONIC ISO C'nin parcasi degildir. */
#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#endif

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

#define ISINMA (15)
#define TUR    (120)

/* ---------------------------------------------------------------------
 * ZAMANLAYICI
 * ------------------------------------------------------------------- */
static double frekans(void)
{
#if defined(_WIN32)
    LARGE_INTEGER f;
    (void)QueryPerformanceFrequency(&f);
    return (double)f.QuadPart;
#else
    return 1000000000.0;
#endif
}

static long long simdi(void)
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
 * VERI SETI
 * ------------------------------------------------------------------- */
typedef struct
{
    char     ad[48];
    char     dosya[64];
    int32_t *veri;
    int32_t  eleman;
    int32_t  kanal;
} veri_seti;

static int veri_yukle(const char *dizin, const char *dosya, const char *ad, veri_seti *vs)
{
    char yol[1024];
    FILE *f;
    int32_t basliklar[2];

    (void)snprintf(yol, sizeof(yol), "%s/%s", dizin, dosya);
    f = fopen(yol, "rb");
    if (f == NULL) { return 0; }

    if (fread(basliklar, sizeof(int32_t), 2, f) != 2u) { (void)fclose(f); return 0; }
    vs->kanal = basliklar[0];
    vs->eleman = basliklar[1];

    vs->veri = (int32_t *)malloc((size_t)vs->eleman * sizeof(int32_t));
    if (vs->veri == NULL) { (void)fclose(f); return 0; }

    if (fread(vs->veri, sizeof(int32_t), (size_t)vs->eleman, f) != (size_t)vs->eleman)
    {
        free(vs->veri); (void)fclose(f); return 0;
    }
    (void)fclose(f);

    (void)snprintf(vs->ad, sizeof(vs->ad), "%.*s", (int)sizeof(vs->ad) - 1, ad);
    (void)snprintf(vs->dosya, sizeof(vs->dosya), "%.*s", (int)sizeof(vs->dosya) - 1, dosya);
    return 1;
}

/* ---------------------------------------------------------------------
 * OLCUMLER
 * ------------------------------------------------------------------- */
static void oran_satiri(const veri_seti *vs)
{
    int32_t ham = vs->eleman * 4;
    int32_t cekirdek_kap = elbari_cekirdek_en_kotu_durum_boyutu(vs->eleman);
    int32_t kanal_kap = elbari_kanal_en_kotu_durum_boyutu(vs->eleman, vs->kanal);
    int32_t cal_kap = elbari_kanal_gerekli_calisma_alani(vs->eleman, vs->kanal);
    uint8_t *c1 = (uint8_t *)malloc((size_t)cekirdek_kap);
    uint8_t *c2 = (uint8_t *)malloc((size_t)kanal_kap);
    int32_t *cal = (int32_t *)malloc((size_t)cal_kap * sizeof(int32_t));
    int32_t n_cek;
    int32_t n_kan;
    int32_t n_cer = 0;
    char cek_metin[16];

    if ((c1 == NULL) || (c2 == NULL) || (cal == NULL)) { return; }

    n_cek = elbari_kabid(vs->veri, vs->eleman, c1, cekirdek_kap);
    if (n_cek > 0)
    {
        (void)snprintf(cek_metin, sizeof(cek_metin), "%8.2fx", (double)ham / (double)n_cek);
    }
    else
    {
        (void)snprintf(cek_metin, sizeof(cek_metin), "%8s", "RED");
    }

    n_kan = elbari_kanal_kabid(vs->veri, vs->eleman, vs->kanal, cal, cal_kap, c2, kanal_kap);

    /* Cerceve toplami (100 kayit/cerceve) */
    {
        const int32_t KPC = 100;
        int32_t kayit = vs->eleman / vs->kanal;
        int32_t ccal_kap = elbari_cerceve_gerekli_calisma_alani(KPC, vs->kanal);
        int32_t paket_kap = elbari_cerceve_en_kotu_durum_boyutu(KPC, vs->kanal);
        int32_t *ccal = (int32_t *)malloc((size_t)ccal_kap * sizeof(int32_t));
        uint8_t *paket = (uint8_t *)malloc((size_t)paket_kap);
        int32_t k;
        uint32_t sira = 0u;

        if ((ccal != NULL) && (paket != NULL))
        {
            for (k = 0; k < kayit; k += KPC)
            {
                int32_t a = ((kayit - k) < KPC) ? (kayit - k) : KPC;
                int32_t n = elbari_cerceve_yaz(&vs->veri[k * vs->kanal], a * vs->kanal,
                                               vs->kanal, sira, ccal, ccal_kap,
                                               paket, paket_kap);
                sira++;
                if (n > 0) { n_cer += n; }
            }
        }
        free(ccal);
        free(paket);
    }

    printf("%-22s|%3d|%10d|%10s|%9.2fx|%9.2fx\n",
           vs->ad, (int)vs->kanal, (int)ham, cek_metin,
           (double)ham / (double)n_kan,
           (n_cer > 0) ? ((double)ham / (double)n_cer) : 0.0);

    free(c1); free(c2); free(cal);
}

static void hiz_satiri(const veri_seti *vs)
{
    int32_t ham = vs->eleman * 4;
    int32_t kayit = vs->eleman / vs->kanal;
    int32_t kanal_kap = elbari_kanal_en_kotu_durum_boyutu(vs->eleman, vs->kanal);
    int32_t cal_kap = elbari_kanal_gerekli_calisma_alani(vs->eleman, vs->kanal);
    uint8_t *cikti = (uint8_t *)malloc((size_t)kanal_kap);
    int32_t *cal = (int32_t *)malloc((size_t)cal_kap * sizeof(int32_t));
    int32_t *cr = (int32_t *)malloc((size_t)cal_kap * sizeof(int32_t));
    int32_t *geri = (int32_t *)malloc((size_t)vs->eleman * sizeof(int32_t));
    int32_t n = 0;
    int t;
    long long bas;
    double enc;
    double dec;
    double fr = frekans();

    if ((cikti == NULL) || (cal == NULL) || (cr == NULL) || (geri == NULL)) { return; }

    for (t = 0; t < ISINMA; t++)
    {
        n = elbari_kanal_kabid(vs->veri, vs->eleman, vs->kanal, cal, cal_kap, cikti, kanal_kap);
        (void)elbari_kanal_basit(cikti, n, cr, cal_kap, geri, vs->eleman);
    }

    bas = simdi();
    for (t = 0; t < TUR; t++)
    {
        n = elbari_kanal_kabid(vs->veri, vs->eleman, vs->kanal, cal, cal_kap, cikti, kanal_kap);
    }
    enc = ((double)(simdi() - bas) / fr) / (double)TUR;

    bas = simdi();
    for (t = 0; t < TUR; t++)
    {
        (void)elbari_kanal_basit(cikti, n, cr, cal_kap, geri, vs->eleman);
    }
    dec = ((double)(simdi() - bas) / fr) / (double)TUR;

    printf("%-22s|%13.0f |%13.0f |%17.0f \n",
           vs->ad,
           ((double)ham / enc) / (1024.0 * 1024.0),
           ((double)ham / dec) / (1024.0 * 1024.0),
           (double)kayit / enc);

    free(cikti); free(cal); free(cr); free(geri);
}

/* =====================================================================
 * ANA
 * ===================================================================== */
int main(int argc, char **argv)
{
    static const char *dosyalar[] = {
        "gercek_gps.bin", "iha_telemetri.bin", "float_kuantalanmis.bin",
        "sirali_sayac.bin", "sabit_deger.bin", "sinus_sensor.bin", "rastgele.bin"
    };
    static const char *adlar[] = {
        "Gercek GPS", "IHA telemetri", "Float kuantalanmis",
        "Sirali sayac", "Sabit deger", "Sinus sensor", "Rastgele"
    };
    const int ADET = (int)(sizeof(dosyalar) / sizeof(dosyalar[0]));
    veri_seti setler[16];
    int yuklenen = 0;
    int i;

    if (argc < 2)
    {
        (void)fprintf(stderr, "Kullanim: kapsamli <veri_dizini>\n");
        return 2;
    }

    for (i = 0; i < ADET; i++)
    {
        if (veri_yukle(argv[1], dosyalar[i], adlar[i], &setler[yuklenen]) != 0)
        {
            yuklenen++;
        }
        else
        {
            (void)fprintf(stderr, "UYARI: yuklenemedi: %s\n", dosyalar[i]);
        }
    }

    if (yuklenen == 0)
    {
        (void)fprintf(stderr, "HATA: hicbir veri seti yuklenemedi\n");
        return 2;
    }

    printf("=================================================================================\n");
    printf("  ELBARI KAPSAMLI OLCUM - C (saf skaler, SIMD yok)\n");
    printf("  Isinma %d tur, olcum %d tur, tek is parcacigi\n", ISINMA, TUR);
    printf("=================================================================================\n\n");

    printf("BOLUM 1 - SIKISTIRMA ORANI (katmana gore)\n");
    printf("---------------------------------------------------------------------------------\n");
    printf("%-22s|%3s|%10s|%10s|%10s|%10s\n", "veri seti", "K", "ham B", "cekirdek", "kanal", "cerceve");
    printf("---------------------------------------------------------------------------------\n");
    for (i = 0; i < yuklenen; i++) { oran_satiri(&setler[i]); }
    printf("\n");

    printf("BOLUM 2 - HIZ (kanal katmani, MB/sn)\n");
    printf("---------------------------------------------------------------------------------\n");
    printf("%-22s|%14s|%14s|%18s\n", "veri seti", "encode MB/sn", "decode MB/sn", "encode kayit/sn");
    printf("-------------------------------------------------------------------------\n");
    for (i = 0; i < yuklenen; i++) { hiz_satiri(&setler[i]); }
    printf("\n");

    for (i = 0; i < yuklenen; i++) { free(setler[i].veri); }
    return 0;
}
