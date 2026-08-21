/* =====================================================================
 * KIYAS - ElBari'yi KENDI AILESIYLE olcen kosucu
 * ---------------------------------------------------------------------
 * Calistirma:
 *   kiyas <veri.bin> [tur_sayisi]
 *
 * Veri bicimi (little-endian), testverisi/gercek_gps.bin ile ayni:
 *   [int32 kanal_sayisi][int32 eleman_sayisi][int32 x eleman_sayisi]
 *
 * OLCUM KURALLARI:
 *   - Tum kodekler ayni veriyi, ayni makinede, ayni derleyici ve
 *     bayraklarla, ayni tur sayisiyla isler.
 *   - Rakiplere kanal ayrimi + fark + zigzag on islemesi BEDAVA verilir.
 *     ElBari bunlari kendi icinde yapar; rakipler icin disaridan yapilir.
 *     Boylece "ElBari veri yapisini biliyor, rakipler bilmiyor" itirazi
 *     ortadan kalkar.
 *   - Her kodek TAM TUR (round-trip) dogrulamasindan gecer. Dogrulamayi
 *     gecemeyen kodegin orani anlamsizdir; tabloda BOZUK olarak isaretlenir.
 *   - Sikistirilmis boyuta kodegin KENDI baslik yuku dahildir.
 * ===================================================================== */

#define _CRT_SECURE_NO_WARNINGS

#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kiyas.h"
#include "../src/elbari.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <time.h>
#endif

/* ---------------------------------------------------------------------
 * ZAMANLAYICI
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

/* =====================================================================
 * ORTAK CALISMA ALANI
 * ===================================================================== */

typedef struct
{
    int32_t  *kanal_i32;      /* tek kanalin isaretli degerleri      */
    uint32_t *kanal_u32;      /* fark + zigzag sonucu                */
    uint32_t *geri_u32;       /* cozumden donen                      */
    int32_t  *geri_i32;
    uint8_t  *cikti;
    int32_t   cikti_kap;
    int32_t  *elbari_calisma;
    int32_t   elbari_calisma_kap;
    int32_t  *cerceve_boylari; /* cerceve katmani icin paket uzunluklari */
    int32_t   cerceve_adedi;
    int32_t   kpc;             /* cerceve basina kayit (supurme icin degisir) */
} is_alani;

/* =====================================================================
 * AILE A SARMALAYICISI
 * ---------------------------------------------------------------------
 * Bicim: [1 bayt kanal sayisi][kanal x int32 uzunluk][kanal yukleri]
 * Baslik yuku sikistirilmis boyuta DAHILDIR.
 * ===================================================================== */

static int32_t aile_a_kodla(kiyas_kodla_fn kodla,
                            const int32_t *veri, int32_t eleman_sayisi,
                            int32_t kanal_sayisi, is_alani *is)
{
    int32_t baslik = 1 + (4 * kanal_sayisi);
    int32_t p = baslik;
    int32_t c;

    if (baslik > is->cikti_kap) { return -1; }
    is->cikti[0] = (uint8_t)kanal_sayisi;

    for (c = 0; c < kanal_sayisi; c++)
    {
        int32_t adet = kiyas_kanal_cek(veri, eleman_sayisi, kanal_sayisi, c,
                                       is->kanal_i32);
        int32_t n;

        kiyas_delta_zigzag(is->kanal_i32, adet, is->kanal_u32);

        n = kodla(is->kanal_u32, adet, &is->cikti[p], is->cikti_kap - p);
        if (n < 0) { return -1; }

        is->cikti[1 + (4 * c) + 0] = (uint8_t)((uint32_t)n & 0xFFu);
        is->cikti[1 + (4 * c) + 1] = (uint8_t)(((uint32_t)n >> 8) & 0xFFu);
        is->cikti[1 + (4 * c) + 2] = (uint8_t)(((uint32_t)n >> 16) & 0xFFu);
        is->cikti[1 + (4 * c) + 3] = (uint8_t)(((uint32_t)n >> 24) & 0xFFu);
        p += n;
    }
    return p;
}

static int32_t aile_a_coz(kiyas_coz_fn coz,
                          const uint8_t *girdi, int32_t boyut,
                          int32_t eleman_sayisi, int32_t kanal_sayisi,
                          int32_t *cikti, is_alani *is)
{
    int32_t baslik = 1 + (4 * kanal_sayisi);
    int32_t p = baslik;
    int32_t c;

    if (boyut < baslik) { return -1; }
    if ((int32_t)girdi[0] != kanal_sayisi) { return -1; }

    for (c = 0; c < kanal_sayisi; c++)
    {
        int32_t adet = (eleman_sayisi / kanal_sayisi)
                       + (((eleman_sayisi % kanal_sayisi) > c) ? 1 : 0);
        uint32_t n = (uint32_t)girdi[1 + (4 * c) + 0]
                   | ((uint32_t)girdi[1 + (4 * c) + 1] << 8)
                   | ((uint32_t)girdi[1 + (4 * c) + 2] << 16)
                   | ((uint32_t)girdi[1 + (4 * c) + 3] << 24);

        if (coz(&girdi[p], boyut - p, is->geri_u32, adet) < 0) { return -1; }
        /* DIKKAT: cikti, cagiran tarafta is->geri_i32 olabilir. Ara tampon
         * olarak kanal_i32 kullanilir; aksi halde kanal_koy kendi kaynagini
         * uzerine yazar (kanal_sayisi > 1 iken bozulma). */
        kiyas_delta_zigzag_ters(is->geri_u32, adet, is->kanal_i32);
        kiyas_kanal_koy(is->kanal_i32, adet, kanal_sayisi, c, cikti);
        p += (int32_t)n;
    }
    return 0;
}

/* =====================================================================
 * ELBARI SARMALAYICILARI
 * ===================================================================== */

static int32_t elbari_kanal_kodla(const int32_t *veri, int32_t eleman_sayisi,
                                  int32_t kanal_sayisi, is_alani *is)
{
    return elbari_kanal_kabid(veri, eleman_sayisi, kanal_sayisi,
                              is->elbari_calisma, is->elbari_calisma_kap,
                              is->cikti, is->cikti_kap);
}

static int32_t elbari_kanal_coz(const uint8_t *girdi, int32_t boyut,
                                int32_t eleman_sayisi, int32_t *cikti,
                                is_alani *is)
{
    return elbari_kanal_basit(girdi, boyut,
                              is->elbari_calisma, is->elbari_calisma_kap,
                              cikti, eleman_sayisi);
}

#define CERCEVE_KAYIT (100)

/* Cerceve boyutu supurmesinde denenen en buyuk cerceve (kayit). */
#define SUPURME_EN_BUYUK (1000)

static int32_t elbari_cerceve_kodla(const int32_t *veri, int32_t eleman_sayisi,
                                    int32_t kanal_sayisi, is_alani *is)
{
    int32_t kayit_sayisi = eleman_sayisi / kanal_sayisi;
    int32_t p = 0;
    int32_t r = 0;
    uint32_t sira = 0u;

    is->cerceve_adedi = 0;

    while (r < kayit_sayisi)
    {
        int32_t kac = kayit_sayisi - r;
        int32_t n;

        if (kac > is->kpc) { kac = is->kpc; }

        n = elbari_cerceve_yaz(&veri[r * kanal_sayisi], kac * kanal_sayisi,
                               kanal_sayisi, sira,
                               is->elbari_calisma, is->elbari_calisma_kap,
                               &is->cikti[p], is->cikti_kap - p);
        if (n < 0) { return -1; }

        is->cerceve_boylari[is->cerceve_adedi] = n;
        is->cerceve_adedi++;
        p += n;
        r += kac;
        sira++;
    }
    return p;
}

static int32_t elbari_cerceve_coz(int32_t eleman_sayisi, int32_t kanal_sayisi,
                                  int32_t *cikti, is_alani *is)
{
    int32_t p = 0;
    int32_t r = 0;
    int32_t k;
    int32_t kayit_sayisi = eleman_sayisi / kanal_sayisi;

    for (k = 0; k < is->cerceve_adedi; k++)
    {
        int32_t kac = kayit_sayisi - r;
        int32_t sonuc;

        if (kac > is->kpc) { kac = is->kpc; }

        sonuc = elbari_cerceve_oku(&is->cikti[p], is->cerceve_boylari[k],
                                   kanal_sayisi,
                                   is->elbari_calisma, is->elbari_calisma_kap,
                                   &cikti[r * kanal_sayisi], kac * kanal_sayisi,
                                   NULL, NULL);
        if (sonuc != ELBARI_TAMAM) { return -1; }

        p += is->cerceve_boylari[k];
        r += kac;
    }
    return 0;
}

/* =====================================================================
 * KODEK TABLOSU
 * ===================================================================== */

typedef enum
{
    TUR_AILE_A = 0,
    TUR_SPRINTZ,
    TUR_ELBARI_KANAL,
    TUR_ELBARI_CERCEVE
} kodek_turu;

typedef struct
{
    const char    *ad;
    const char    *referans;
    kodek_turu     tur;
    kiyas_kodla_fn kodla;
    kiyas_coz_fn   coz;
} kodek_tanimi;

static const kodek_tanimi KODEKLER[] =
{
    { "VByte (LEB128)",     "varint temel cizgisi",        TUR_AILE_A,
      kiyas_vbyte_kodla,        kiyas_vbyte_coz },
    { "StreamVByte",        "Lemire & Kurz 2017",          TUR_AILE_A,
      kiyas_streamvbyte_kodla,  kiyas_streamvbyte_coz },
    { "Simple8b",           "Anh & Moffat 2010",           TUR_AILE_A,
      kiyas_simple8b_kodla,     kiyas_simple8b_coz },
    { "BP128",              "Lemire & Boytsov 2015",       TUR_AILE_A,
      kiyas_bp128_kodla,        kiyas_bp128_coz },
    { "OptPFD (PFOR+yama)", "Zukowski 2006 / Yan 2009",    TUR_AILE_A,
      kiyas_optpfd_kodla,       kiyas_optpfd_coz },
    { "Sprintz-Delta",      "Blalock ve ark. 2018",        TUR_SPRINTZ,
      NULL, NULL },
    { "ElBari (kanal)",     "bu calisma",                  TUR_ELBARI_KANAL,
      NULL, NULL },
    { "ElBari (cerceve100)","bu calisma - kayip dayanikli", TUR_ELBARI_CERCEVE,
      NULL, NULL }
};

#define KODEK_ADEDI ((int32_t)(sizeof(KODEKLER) / sizeof(KODEKLER[0])))

/* =====================================================================
 * SONUC KAYDI
 * ===================================================================== */

typedef struct
{
    const char *ad;
    const char *referans;
    int32_t     boyut;
    double      oran;
    double      enc_mbsn;
    double      dec_mbsn;
    int32_t     dogru;      /* 1 = tam tur dogrulamasi gecti */
    int32_t     pareto_enc;
    int32_t     pareto_dec;
} sonuc;

/* ---------------------------------------------------------------------
 * TEK BIR KODEGI OLC
 * ------------------------------------------------------------------- */

static void olc(const kodek_tanimi *k, const int32_t *veri,
                int32_t eleman_sayisi, int32_t kanal_sayisi,
                int32_t tur, double frekans, is_alani *is, sonuc *s)
{
    int32_t t;
    long long bas;
    long long son;
    double enc_sn;
    double dec_sn;
    int32_t boyut = -1;
    double ham_bayt = (double)eleman_sayisi * 4.0;

    s->ad       = k->ad;
    s->referans = k->referans;
    s->dogru    = 0;
    s->boyut    = -1;
    s->oran     = 0.0;
    s->enc_mbsn = 0.0;
    s->dec_mbsn = 0.0;
    s->pareto_enc = 0;
    s->pareto_dec = 0;

    /* --- Isinma + boyut --- */
    for (t = 0; t < 5; t++)
    {
        switch (k->tur)
        {
        case TUR_AILE_A:
            boyut = aile_a_kodla(k->kodla, veri, eleman_sayisi, kanal_sayisi, is);
            break;
        case TUR_SPRINTZ:
            boyut = kiyas_sprintz_kodla(veri, eleman_sayisi, kanal_sayisi,
                                        is->cikti, is->cikti_kap);
            break;
        case TUR_ELBARI_KANAL:
            boyut = elbari_kanal_kodla(veri, eleman_sayisi, kanal_sayisi, is);
            break;
        case TUR_ELBARI_CERCEVE:
        default:
            boyut = elbari_cerceve_kodla(veri, eleman_sayisi, kanal_sayisi, is);
            break;
        }
    }

    if (boyut <= 0)
    {
        s->boyut = boyut;
        return;
    }

    /* --- Tam tur dogrulamasi --- */
    {
        int32_t sonuc_kod = -1;

        (void)memset(is->geri_i32, 0, (size_t)eleman_sayisi * sizeof(int32_t));

        switch (k->tur)
        {
        case TUR_AILE_A:
            sonuc_kod = aile_a_coz(k->coz, is->cikti, boyut, eleman_sayisi,
                                   kanal_sayisi, is->geri_i32, is);
            break;
        case TUR_SPRINTZ:
            sonuc_kod = kiyas_sprintz_coz(is->cikti, boyut, eleman_sayisi,
                                          kanal_sayisi, is->geri_i32);
            break;
        case TUR_ELBARI_KANAL:
            sonuc_kod = elbari_kanal_coz(is->cikti, boyut, eleman_sayisi,
                                         is->geri_i32, is);
            break;
        case TUR_ELBARI_CERCEVE:
        default:
            sonuc_kod = elbari_cerceve_coz(eleman_sayisi, kanal_sayisi,
                                           is->geri_i32, is);
            break;
        }

        if ((sonuc_kod == 0) &&
            (memcmp(veri, is->geri_i32,
                    (size_t)eleman_sayisi * sizeof(int32_t)) == 0))
        {
            s->dogru = 1;
        }
    }

    /* --- Kodlama suresi --- */
    bas = zaman_oku();
    for (t = 0; t < tur; t++)
    {
        switch (k->tur)
        {
        case TUR_AILE_A:
            (void)aile_a_kodla(k->kodla, veri, eleman_sayisi, kanal_sayisi, is);
            break;
        case TUR_SPRINTZ:
            (void)kiyas_sprintz_kodla(veri, eleman_sayisi, kanal_sayisi,
                                      is->cikti, is->cikti_kap);
            break;
        case TUR_ELBARI_KANAL:
            (void)elbari_kanal_kodla(veri, eleman_sayisi, kanal_sayisi, is);
            break;
        case TUR_ELBARI_CERCEVE:
        default:
            (void)elbari_cerceve_kodla(veri, eleman_sayisi, kanal_sayisi, is);
            break;
        }
    }
    son = zaman_oku();
    enc_sn = ((double)(son - bas) / frekans) / (double)tur;

    /* --- Cozme suresi --- */
    bas = zaman_oku();
    for (t = 0; t < tur; t++)
    {
        switch (k->tur)
        {
        case TUR_AILE_A:
            (void)aile_a_coz(k->coz, is->cikti, boyut, eleman_sayisi,
                             kanal_sayisi, is->geri_i32, is);
            break;
        case TUR_SPRINTZ:
            (void)kiyas_sprintz_coz(is->cikti, boyut, eleman_sayisi,
                                    kanal_sayisi, is->geri_i32);
            break;
        case TUR_ELBARI_KANAL:
            (void)elbari_kanal_coz(is->cikti, boyut, eleman_sayisi,
                                   is->geri_i32, is);
            break;
        case TUR_ELBARI_CERCEVE:
        default:
            (void)elbari_cerceve_coz(eleman_sayisi, kanal_sayisi,
                                     is->geri_i32, is);
            break;
        }
    }
    son = zaman_oku();
    dec_sn = ((double)(son - bas) / frekans) / (double)tur;

    s->boyut    = boyut;
    s->oran     = ham_bayt / (double)boyut;
    s->enc_mbsn = (ham_bayt / enc_sn) / (1024.0 * 1024.0);
    s->dec_mbsn = (ham_bayt / dec_sn) / (1024.0 * 1024.0);
}

/* ---------------------------------------------------------------------
 * PARETO SINIRI
 * ---------------------------------------------------------------------
 * Bir nokta, baska bir nokta HEM daha iyi oran HEM daha iyi hiz veriyorsa
 * BASKILANMIS sayilir. Baskilanmayanlar Pareto sinirini olusturur.
 * Yalnizca dogrulamayi gecen kodekler degerlendirilir.
 * ------------------------------------------------------------------- */

static void pareto_hesapla(sonuc *s, int32_t adet)
{
    int32_t i;
    int32_t j;

    for (i = 0; i < adet; i++)
    {
        if (s[i].dogru == 0) { continue; }
        s[i].pareto_enc = 1;
        s[i].pareto_dec = 1;

        for (j = 0; j < adet; j++)
        {
            if ((j == i) || (s[j].dogru == 0)) { continue; }

            if ((s[j].oran >= s[i].oran) && (s[j].enc_mbsn >= s[i].enc_mbsn) &&
                ((s[j].oran > s[i].oran) || (s[j].enc_mbsn > s[i].enc_mbsn)))
            {
                s[i].pareto_enc = 0;
            }
            if ((s[j].oran >= s[i].oran) && (s[j].dec_mbsn >= s[i].dec_mbsn) &&
                ((s[j].oran > s[i].oran) || (s[j].dec_mbsn > s[i].dec_mbsn)))
            {
                s[i].pareto_dec = 0;
            }
        }
    }
}

/* ---------------------------------------------------------------------
 * TABLO YAZDIR
 * ------------------------------------------------------------------- */

static void tablo_yaz(const sonuc *s, int32_t adet, int32_t ham_bayt)
{
    int32_t i;

    printf("  %-22s %10s %8s %11s %11s %7s %s\n",
           "kodek", "bayt", "oran", "enc MB/sn", "dec MB/sn", "dogru", "Pareto");
    printf("  ---------------------------------------------------------------"
           "---------------------\n");

    for (i = 0; i < adet; i++)
    {
        char pareto[8];
        int32_t p = 0;

        if (s[i].boyut <= 0)
        {
            printf("  %-22s %10s %8s %11s %11s %7s\n",
                   s[i].ad, "-", "-", "-", "-", "RED");
            continue;
        }

        pareto[0] = '\0';
        if (s[i].pareto_enc != 0) { pareto[p] = 'E'; p++; }
        if (s[i].pareto_dec != 0) { pareto[p] = 'D'; p++; }
        pareto[p] = '\0';

        printf("  %-22s %10d %7.2fx %11.0f %11.0f %7s %s\n",
               s[i].ad, (int)s[i].boyut, s[i].oran,
               s[i].enc_mbsn, s[i].dec_mbsn,
               (s[i].dogru != 0) ? "EVET" : "HAYIR",
               pareto);
    }
    printf("  ---------------------------------------------------------------"
           "---------------------\n");
    printf("  ham veri: %d bayt   |   Pareto: E = oran/encode sinirinda, "
           "D = oran/decode sinirinda\n", (int)ham_bayt);
}

/* ---------------------------------------------------------------------
 * CSV DOKUMU (Pareto grafigi icin)
 * ------------------------------------------------------------------- */

static void csv_yaz(FILE *f, const char *senaryo, const sonuc *s, int32_t adet)
{
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        if (s[i].boyut <= 0) { continue; }
        (void)fprintf(f, "%s,%s,%s,%d,%.4f,%.1f,%.1f,%d,%d,%d\n",
                      senaryo, s[i].ad, s[i].referans, (int)s[i].boyut,
                      s[i].oran, s[i].enc_mbsn, s[i].dec_mbsn,
                      (int)s[i].dogru, (int)s[i].pareto_enc,
                      (int)s[i].pareto_dec);
    }
}

/* =====================================================================
 * CERCEVE BOYUTU SUPURMESI
 * ---------------------------------------------------------------------
 * NEDEN GEREKLI:
 *   Cerceve boyutu bedava secilen bir parametre degildir. Uc kisit ayni
 *   anda baglar:
 *
 *   1) ORAN   : cerceve kucculdukce 16 baytlik baslik + CRC yuku ve
 *               kaybolan fark baglami orani dusurur.
 *   2) GECIKME: bir cerceve, dolana kadar GONDERILEMEZ. 10 Hz telemetride
 *               100 kayitlik cerceve = 10 saniye tamponlama demektir.
 *               Canli telemetride bu kabul edilemez.
 *   3) PAKET  : cerceve tek bir radyo paketine sigmalidir. SiK/RFD900
 *               siniflarinda kullanilabilir yuk tipik olarak ~200-250
 *               bayttir.
 *
 *   Bu supurme ucunu birden tek tabloda gosterir; boylece "hangi cerceve
 *   boyutu" sorusu fikir degil, olcum meselesi olur.
 * ===================================================================== */

static const int32_t SUPURME_BOYUTLARI[] =
{
    1, 2, 5, 10, 20, 25, 50, 100, 200, 500, 1000
};

#define SUPURME_ADEDI ((int32_t)(sizeof(SUPURME_BOYUTLARI) \
                                 / sizeof(SUPURME_BOYUTLARI[0])))

static void cerceve_supurmesi(const int32_t *veri, int32_t eleman_sayisi,
                              int32_t kanal_sayisi, is_alani *is, FILE *csv)
{
    double ham_bayt = (double)eleman_sayisi * 4.0;
    int32_t cercevesiz;
    int32_t u;

    /* Referans: cerceveleme yok (kanal katmani tek blok). */
    cercevesiz = elbari_kanal_kodla(veri, eleman_sayisi, kanal_sayisi, is);

    printf("\n--- SENARYO 3: cerceve boyutu supurmesi (yalnizca ElBari) ---\n");
    printf("Cerceve boyutu oran, gecikme ve paket boyutunu AYNI ANDA baglar.\n");
    printf("Gecikme = cerceve dolana kadar beklenen sure (telemetri hizina bagli).\n\n");

    printf("  %6s %8s %10s %8s %9s %9s %9s %9s\n",
           "kayit", "cerceve", "toplam B", "oran", "ort B/crc",
           "enb B/crc", "gec@10Hz", "gec@50Hz");
    printf("  --------------------------------------------------------------"
           "-------------------\n");

    printf("  %6s %8s %10d %7.2fx %9s %9s %9s %9s\n",
           "-", "yok", (int)cercevesiz, ham_bayt / (double)cercevesiz,
           "-", "-", "akis", "akis");

    for (u = 0; u < SUPURME_ADEDI; u++)
    {
        int32_t kpc = SUPURME_BOYUTLARI[u];
        int32_t boyut;
        int32_t k;
        int32_t en_buyuk = 0;
        double  ortalama;
        int32_t dogru = 0;

        is->kpc = kpc;

        boyut = elbari_cerceve_kodla(veri, eleman_sayisi, kanal_sayisi, is);
        if (boyut <= 0)
        {
            /* Sessizce atlamak yaniltici olurdu: hangi boyutun neden
             * calismadigi tabloda gorunmelidir. */
            printf("  %6d %8s %10s %8s %9s %9s %9s %9s  <-- kodlama hatasi (%d)\n",
                   (int)kpc, "-", "-", "-", "-", "-", "-", "-", (int)boyut);
            continue;
        }

        for (k = 0; k < is->cerceve_adedi; k++)
        {
            if (is->cerceve_boylari[k] > en_buyuk)
            {
                en_buyuk = is->cerceve_boylari[k];
            }
        }
        ortalama = (double)boyut / (double)is->cerceve_adedi;

        (void)memset(is->geri_i32, 0, (size_t)eleman_sayisi * sizeof(int32_t));
        if ((elbari_cerceve_coz(eleman_sayisi, kanal_sayisi, is->geri_i32, is) == 0) &&
            (memcmp(veri, is->geri_i32,
                    (size_t)eleman_sayisi * sizeof(int32_t)) == 0))
        {
            dogru = 1;
        }

        printf("  %6d %8d %10d %7.2fx %9.0f %9d %8.2fs %8.2fs%s\n",
               (int)kpc, (int)is->cerceve_adedi, (int)boyut,
               ham_bayt / (double)boyut, ortalama, (int)en_buyuk,
               (double)kpc / 10.0, (double)kpc / 50.0,
               (dogru != 0) ? "" : "  <-- DOGRULAMA BASARISIZ");

        if (csv != NULL)
        {
            (void)fprintf(csv, "cerceve-supurme,ElBari kpc=%d,bu calisma,"
                               "%d,%.4f,0.0,0.0,%d,0,0\n",
                          (int)kpc, (int)boyut, ham_bayt / (double)boyut,
                          (int)dogru);
        }
    }

    printf("  --------------------------------------------------------------"
           "-------------------\n");
    printf("  ort B/crc = cerceve basina ortalama bayt (radyo paket boyutu)\n");
    printf("  gec@N Hz  = telemetri N Hz ise cerceve dolana kadar gecen sure\n");

    is->kpc = CERCEVE_KAYIT;
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
    unsigned char *ham;
    long ham_boy = 0;
    int32_t kanal_sayisi;
    int32_t eleman_sayisi;
    int32_t *veri;
    int32_t tur = 100;
    double frekans;
    is_alani is;
    sonuc sonuclar[KODEK_ADEDI];
    int32_t i;
    FILE *csv;

    if (argc < 2)
    {
        (void)fprintf(stderr, "Kullanim: kiyas <veri.bin> [tur_sayisi]\n");
        return 2;
    }
    if (argc >= 3)
    {
        tur = atoi(argv[2]);
        if (tur < 1) { tur = 1; }
    }

    ham = dosya_oku(argv[1], &ham_boy);
    if (ham == NULL)
    {
        (void)fprintf(stderr, "HATA: veri okunamadi: %s\n", argv[1]);
        return 2;
    }

    (void)memcpy(&kanal_sayisi, &ham[0], 4);
    (void)memcpy(&eleman_sayisi, &ham[4], 4);

    veri = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
    if (veri == NULL) { return 2; }
    (void)memcpy(veri, &ham[8], (size_t)eleman_sayisi * sizeof(int32_t));

    frekans = zamanlayici_frekansi();

    /* --- Calisma alanlari (olcum dongusunun disinda ayrilir) --- */
    is.cikti_kap = (eleman_sayisi * 8) + 65536;
    is.cikti     = (uint8_t *)malloc((size_t)is.cikti_kap);
    is.kanal_i32 = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
    is.kanal_u32 = (uint32_t *)malloc((size_t)eleman_sayisi * sizeof(uint32_t));
    is.geri_u32  = (uint32_t *)malloc((size_t)eleman_sayisi * sizeof(uint32_t));
    is.geri_i32  = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
    /* Iki senaryo da (kanal_sayisi ve 1) ayni tamponlari kullanir; her
     * ikisinin de en kotu ihtiyaci karsilanmalidir. */
    {
        int32_t a = elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi);
        int32_t b = elbari_kanal_gerekli_calisma_alani(eleman_sayisi, 1);
        /* Supurmede en buyuk cerceve SUPURME_EN_BUYUK kayittir. */
        int32_t c = elbari_cerceve_gerekli_calisma_alani(SUPURME_EN_BUYUK, kanal_sayisi);
        int32_t d = elbari_cerceve_gerekli_calisma_alani(SUPURME_EN_BUYUK, 1);

        is.elbari_calisma_kap = a;
        if (b > is.elbari_calisma_kap) { is.elbari_calisma_kap = b; }
        if (c > is.elbari_calisma_kap) { is.elbari_calisma_kap = c; }
        if (d > is.elbari_calisma_kap) { is.elbari_calisma_kap = d; }
    }
    is.elbari_calisma = (int32_t *)malloc((size_t)is.elbari_calisma_kap
                                          * sizeof(int32_t));
    /* En kotu durum: cerceve basina 1 kayit, kanal_sayisi = 1.
     * O zaman cerceve sayisi eleman_sayisi kadar olur. */
    is.cerceve_boylari = (int32_t *)malloc((size_t)(eleman_sayisi + 8)
                                          * sizeof(int32_t));
    is.cerceve_adedi = 0;
    is.kpc = CERCEVE_KAYIT;

    if ((is.cikti == NULL) || (is.kanal_i32 == NULL) || (is.kanal_u32 == NULL) ||
        (is.geri_u32 == NULL) || (is.geri_i32 == NULL) ||
        (is.elbari_calisma == NULL) || (is.cerceve_boylari == NULL))
    {
        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
        return 2;
    }

    printf("=====================================================================\n");
    printf("  KIYAS - ElBari, ait oldugu tamsayi kodek ailesiyle olculuyor\n");
    printf("=====================================================================\n");
    printf("Veri     : %s\n", argv[1]);
    printf("Boyut    : %d kayit x %d kanal = %d eleman (%d bayt ham)\n",
           (int)(eleman_sayisi / kanal_sayisi), (int)kanal_sayisi,
           (int)eleman_sayisi, (int)(eleman_sayisi * 4));
    printf("Tur      : %d (her kodek icin)\n", (int)tur);
    printf("Derleme  : skaler C, SIMD yok - hem ElBari hem rakipler\n\n");
    printf("UYARI: Rakip kodekler yayinlanmis bicimlerden yeniden yazilmistir;\n");
    printf("       yazarlarinin SIMD'li kutuphaneleri DEGILDIR. ORAN sutunu\n");
    printf("       bicimden gelir ve tasinabilirdir; HIZ sutunu rakipler icin\n");
    printf("       bir ALT SINIRDIR. Ayrinti: c/kiyas/BENIOKU.md\n\n");

    csv = fopen("kiyas_sonuclari.csv", "w");
    if (csv != NULL)
    {
        (void)fprintf(csv, "senaryo,kodek,referans,bayt,oran,enc_mbsn,"
                           "dec_mbsn,dogru,pareto_enc,pareto_dec\n");
    }

    /* =================================================================
     * SENARYO 1 - kanal ayrimi ile (adil kiyas)
     * ================================================================= */
    printf("--- SENARYO 1: kanal ayrimi ile (%d kanal) ---\n", (int)kanal_sayisi);
    printf("Rakiplere kanal ayrimi + fark + zigzag BEDAVA verildi.\n\n");

    for (i = 0; i < KODEK_ADEDI; i++)
    {
        olc(&KODEKLER[i], veri, eleman_sayisi, kanal_sayisi, tur, frekans,
            &is, &sonuclar[i]);
    }
    pareto_hesapla(sonuclar, KODEK_ADEDI);
    tablo_yaz(sonuclar, KODEK_ADEDI, eleman_sayisi * 4);
    if (csv != NULL) { csv_yaz(csv, "kanal-ayrimli", sonuclar, KODEK_ADEDI); }

    /* =================================================================
     * SENARYO 2 - kanal ayrimi YOK (naif entegrasyon)
     * ================================================================= */
    printf("\n--- SENARYO 2: kanal ayrimi YOK (tek akis, naif entegrasyon) ---\n");
    printf("Ic ice gecmis kayit akisi tek bir seri gibi islenir.\n\n");

    for (i = 0; i < KODEK_ADEDI; i++)
    {
        olc(&KODEKLER[i], veri, eleman_sayisi, 1, tur, frekans,
            &is, &sonuclar[i]);
    }
    pareto_hesapla(sonuclar, KODEK_ADEDI);
    tablo_yaz(sonuclar, KODEK_ADEDI, eleman_sayisi * 4);
    if (csv != NULL) { csv_yaz(csv, "kanal-ayrimsiz", sonuclar, KODEK_ADEDI); }

    /* =================================================================
     * SENARYO 3 - cerceve boyutu supurmesi
     * ================================================================= */
    cerceve_supurmesi(veri, eleman_sayisi, kanal_sayisi, &is, csv);

    if (csv != NULL)
    {
        (void)fclose(csv);
        printf("\nCSV yazildi: kiyas_sonuclari.csv\n");
    }

    printf("=====================================================================\n");

    free(is.cikti);
    free(is.kanal_i32);
    free(is.kanal_u32);
    free(is.geri_u32);
    free(is.geri_i32);
    free(is.elbari_calisma);
    free(is.cerceve_boylari);
    free(veri);
    free(ham);
    return 0;
}
