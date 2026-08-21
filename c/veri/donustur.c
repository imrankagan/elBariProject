/* =====================================================================
 * ALFA / ArduPilot DataFlash logunu olcum fikstuurune cevirir
 * ---------------------------------------------------------------------
 * Calistirma:
 *   donustur <log.bin>            fikstuurleri uretir
 *   donustur <log.bin> --dok      logun icinde ne oldugunu listeler
 *
 * URETILEN BICIM (testverisi/gercek_gps.bin ile AYNI):
 *   [int32 kanal_sayisi][int32 eleman_sayisi][int32 x eleman_sayisi]
 *   ic ice gecmis kayitlar: k0,k1,k2, k0,k1,k2, ...
 *
 * Boylece cikti dosyalari dogrudan kiyas.exe'ye verilebilir.
 *
 * ---------------------------------------------------------------------
 * KUANTALAMA - neden ve hangi olcekle
 *
 *   Cekirdek motor tamsayi uzerinde calisir. Log'daki ondalikli
 *   degerler, MAVLink'in KENDI gosterimiyle AYNI hassasiyete
 *   kuantalanir; boylece olculen oran, gercek bir telemetri akisinda
 *   elde edilecek oranla karsilastirilabilir olur.
 *
 *     yonelim  : derece -> milirad   (MAVLink ATTITUDE radyan tasir)
 *     jiroskop : rad/sn -> mrad/sn   (SCALED_IMU gibi)
 *     ivme     : m/sn^2 -> mg        (SCALED_IMU gibi)
 *     enlem/boylam: ham int32 1e-7 derece (MAVLink ile AYNI, donusum yok)
 *
 *   Kendi kafamiza gore bir olcek secmek sonuclari yaniltirdi: cok kaba
 *   olcek orani sisirir, cok ince olcek dusurur. MAVLink'in gosterimine
 *   baglanmak bu keyfiligi ortadan kaldirir.
 * ===================================================================== */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dataflash.h"

#define PI_SAYISI (3.14159265358979323846)

/* derece -> milirad */
#define DER_MRAD  ((PI_SAYISI / 180.0) * 1000.0)
/* m/sn^2 -> mg (yercekimi ivmesine gore) */
#define MS2_MG    (1000.0 / 9.80665)

#define MAKS_KANAL (12)

typedef struct
{
    const char *log_ad;                 /* log kayit adi, orn "ATT"   */
    const char *cikti_ad;               /* uretilecek dosya           */
    const char *aciklama;
    const char *alanlar[MAKS_KANAL];    /* etiketler, NULL ile biter  */
    double      olcek[MAKS_KANAL];      /* her alan icin carpan       */
    int32_t     tamsayi_mi[MAKS_KANAL]; /* 1 = ham tamsayi oku        */
} cikarim;

static const cikarim CIKARIMLAR[] =
{
    { "ATT", "alfa_att.bin", "yonelim (roll/pitch/yaw), milirad",
      { "Roll", "Pitch", "Yaw", NULL },
      { DER_MRAD, DER_MRAD, DER_MRAD },
      { 0, 0, 0 } },

    { "IMU", "alfa_imu.bin", "jiroskop (mrad/sn) + ivme (mg)",
      { "GyrX", "GyrY", "GyrZ", "AccX", "AccY", "AccZ", NULL },
      { 1000.0, 1000.0, 1000.0, MS2_MG, MS2_MG, MS2_MG },
      { 0, 0, 0, 0, 0, 0 } },

    { "GPS", "alfa_gps.bin", "enlem/boylam (1e-7 derece) + irtifa (mm)",
      { "Lat", "Lng", "Alt", NULL },
      { 1.0, 1.0, 1000.0 },
      { 1, 1, 0 } },

    { "RCOU", "alfa_rcou.bin", "servo cikislari (PWM us)",
      { "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", NULL },
      { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
      { 1, 1, 1, 1, 1, 1, 1, 1 } },

    { "RCIN", "alfa_rcin.bin", "kumanda girisleri (PWM us)",
      { "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", NULL },
      { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },
      { 1, 1, 1, 1, 1, 1, 1, 1 } },

    { "BAT", "alfa_bat.bin", "batarya (mV, cA, mAh)",
      { "Volt", "Curr", "CurrTot", NULL },
      { 1000.0, 100.0, 1.0 },
      { 0, 0, 0 } },

    { "VIBE", "alfa_vibe.bin", "titresim (x1000)",
      { "VibeX", "VibeY", "VibeZ", NULL },
      { 1000.0, 1000.0, 1000.0 },
      { 0, 0, 0 } }
};

#define CIKARIM_ADEDI ((int32_t)(sizeof(CIKARIMLAR) / sizeof(CIKARIMLAR[0])))

/* ---------------------------------------------------------------------
 * DOSYA
 * ------------------------------------------------------------------- */

static uint8_t *dosya_oku(const char *yol, int64_t *boyut_cikti)
{
    FILE *f = fopen(yol, "rb");
    uint8_t *tampon;
    long boyut;
    size_t okunan;

    if (f == NULL) { return NULL; }
    (void)fseek(f, 0, SEEK_END);
    boyut = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    if (boyut <= 0) { (void)fclose(f); return NULL; }

    tampon = (uint8_t *)malloc((size_t)boyut);
    if (tampon == NULL) { (void)fclose(f); return NULL; }

    okunan = fread(tampon, 1, (size_t)boyut, f);
    (void)fclose(f);
    if (okunan != (size_t)boyut) { free(tampon); return NULL; }

    *boyut_cikti = (int64_t)boyut;
    return tampon;
}

static int32_t fikstur_yaz(const char *yol, int32_t kanal,
                           const int32_t *veri, int32_t eleman)
{
    FILE *f = fopen(yol, "wb");

    if (f == NULL) { return -1; }
    (void)fwrite(&kanal, 4, 1, f);
    (void)fwrite(&eleman, 4, 1, f);
    (void)fwrite(veri, 4, (size_t)eleman, f);
    (void)fclose(f);
    return 0;
}

/* ---------------------------------------------------------------------
 * DOKUM: logun icinde ne var
 * ------------------------------------------------------------------- */

static void dokum(const uint8_t *ham, int64_t boyut)
{
    df_okuyucu o;
    const df_tanim *t;
    const uint8_t *g;
    long sayac[256];
    int32_t i;

    (void)memset(sayac, 0, sizeof(sayac));
    df_kur(&o, ham, boyut);
    while (df_sonraki(&o, &t, &g) != 0) { sayac[t->tip]++; }

    printf("--- Logdaki kayit tipleri ---\n");
    printf("  %-8s %10s  %s\n", "ad", "adet", "alanlar");
    printf("  ---------------------------------------------------------------\n");

    for (i = 0; i < 256; i++)
    {
        if ((o.tanimli[i] != 0) && (sayac[i] > 0))
        {
            const df_tanim *d = &o.tanimlar[i];
            int32_t j;

            printf("  %-8s %10ld  ", d->ad, sayac[i]);
            for (j = 0; j < d->alan_sayisi; j++)
            {
                printf("%s%s", d->alan_adi[j],
                       (j < (d->alan_sayisi - 1)) ? "," : "");
            }
            printf("\n");
        }
    }
    if (o.atlanan_bayt > 0)
    {
        printf("\n  NOT: %lld bayt atlandi (senkron kaybi ya da tanimsiz tip).\n",
               (long long)o.atlanan_bayt);
    }
}

/* ---------------------------------------------------------------------
 * CIKARIM
 * ------------------------------------------------------------------- */

static void cikar(const uint8_t *ham, int64_t boyut, const cikarim *c)
{
    df_okuyucu o;
    const df_tanim *t;
    const uint8_t *g;
    const df_tanim *hedef;
    int32_t alan_indeksi[MAKS_KANAL];
    int32_t kanal = 0;
    long    kayit = 0;
    int32_t *tampon;
    int32_t  kapasite;
    int32_t  eleman = 0;
    int32_t  i;

    /* 1) Tanimi bul ve alanlari esle */
    df_kur(&o, ham, boyut);
    while (df_sonraki(&o, &t, &g) != 0) { /* tanimlarin dolmasi icin tara */ }

    hedef = df_tanim_bul(&o, c->log_ad);
    if (hedef == NULL)
    {
        printf("  %-14s ATLANDI  (logda '%s' kaydi yok)\n",
               c->cikti_ad, c->log_ad);
        return;
    }

    while ((kanal < MAKS_KANAL) && (c->alanlar[kanal] != NULL))
    {
        alan_indeksi[kanal] = df_alan_bul(hedef, c->alanlar[kanal]);
        if (alan_indeksi[kanal] < 0)
        {
            printf("  %-14s ATLANDI  ('%s' kaydinda '%s' alani yok)\n",
                   c->cikti_ad, c->log_ad, c->alanlar[kanal]);
            return;
        }
        kanal++;
    }

    /* 2) Kayit sayisini say */
    df_kur(&o, ham, boyut);
    while (df_sonraki(&o, &t, &g) != 0)
    {
        if (t->tip == hedef->tip) { kayit++; }
    }
    if (kayit == 0)
    {
        printf("  %-14s ATLANDI  ('%s' kaydi hic gecmiyor)\n",
               c->cikti_ad, c->log_ad);
        return;
    }

    kapasite = (int32_t)(kayit * (long)kanal);
    tampon = (int32_t *)malloc((size_t)kapasite * sizeof(int32_t));
    if (tampon == NULL) { return; }

    /* 3) Degerleri cikar ve kuantala */
    df_kur(&o, ham, boyut);
    while (df_sonraki(&o, &t, &g) != 0)
    {
        if (t->tip != hedef->tip) { continue; }
        if ((eleman + kanal) > kapasite) { break; }

        for (i = 0; i < kanal; i++)
        {
            double d;

            if (c->tamsayi_mi[i] != 0)
            {
                d = (double)df_tamsayi(t, g, alan_indeksi[i]) * c->olcek[i];
            }
            else
            {
                d = df_ondalik(t, g, alan_indeksi[i]) * c->olcek[i];
            }

            /* Sifirdan uzaga yuvarlama - ElBari float katmaniyla ayni kural */
            if (d >= 2147483647.0)       { tampon[eleman] = 2147483647; }
            else if (d <= -2147483648.0) { tampon[eleman] = (-2147483647 - 1); }
            else { tampon[eleman] = (d >= 0.0) ? (int32_t)(d + 0.5)
                                               : (int32_t)(d - 0.5); }
            eleman++;
        }
    }

    if (fikstur_yaz(c->cikti_ad, kanal, tampon, eleman) == 0)
    {
        printf("  %-14s %8d kayit x %d kanal = %8d bayt   %s\n",
               c->cikti_ad, (int)(eleman / kanal), (int)kanal,
               (int)(eleman * 4 + 8), c->aciklama);
    }
    else
    {
        printf("  %-14s YAZILAMADI\n", c->cikti_ad);
    }

    free(tampon);
}

/* =====================================================================
 * ANA
 * ===================================================================== */

int main(int argc, char **argv)
{
    uint8_t *ham;
    int64_t  boyut = 0;
    int32_t  i;

    if (argc < 2)
    {
        (void)fprintf(stderr,
                      "Kullanim: donustur <log.bin> [--dok]\n"
                      "  --dok : logun icindeki kayit tiplerini listeler\n");
        return 2;
    }

    ham = dosya_oku(argv[1], &boyut);
    if (ham == NULL)
    {
        (void)fprintf(stderr, "HATA: log okunamadi: %s\n", argv[1]);
        return 2;
    }

    printf("=====================================================================\n");
    printf("  ArduPilot DataFlash -> olcum fikstuuru\n");
    printf("=====================================================================\n");
    printf("Log   : %s  (%lld bayt)\n\n", argv[1], (long long)boyut);

    if ((argc >= 3) && (strcmp(argv[2], "--dok") == 0))
    {
        dokum(ham, boyut);
        free(ham);
        return 0;
    }

    printf("--- Uretilen fikstuurler ---\n");
    for (i = 0; i < CIKARIM_ADEDI; i++)
    {
        cikar(ham, boyut, &CIKARIMLAR[i]);
    }

    printf("\nBu dosyalar dogrudan kiyas.exe'ye verilebilir:\n");
    printf("  kiyas.exe alfa_att.bin 200\n");
    printf("=====================================================================\n");

    free(ham);
    return 0;
}
