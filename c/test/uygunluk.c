/* =====================================================================
 * ELBARI - Uygunluk (conformance) testi
 * ---------------------------------------------------------------------
 * AMAC:
 *   testverisi/vektorler.txt icindeki DONMUS referans vektorlerini
 *   okuyup her biri icin iki yonu de dogrular:
 *
 *     1) KODLAMA : verilen girdiden beklenen bayt dizisi uretiliyor mu?
 *     2) COZME   : beklenen bayt dizisinden orijinal girdi geri geliyor mu?
 *
 * NEDEN ONEMLI:
 *   Bu dosya bicimin sozlesmesidir. Bagimsiz bir implementasyon (baska
 *   bir dil, baska bir ekip, baska bir donanim) ayni vektorleri
 *   uretebiliyorsa bicime uygundur. Savunma/havacilik tedarikinde
 *   istenen "arayuz kontrol dokumani + uygunluk kaniti" tam olarak budur.
 *
 * KULLANIM:
 *   uygunluk <vektor_dosyasi>
 * ===================================================================== */

/* MSVC fopen uyarisi: test kodu, tasinabilirlik icin standart fopen kullanilir */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/elbari.h"

#define MAKS_SATIR    (8192)
#define MAKS_ELEMAN_T (256)
#define MAKS_BAYT_T   (2048)

static int g_gecen = 0;
static int g_kalan = 0;

/* --------------------------------------------------------------------- */

/** Tek bir hex karakteri sayiya cevirir; gecersizse -1. */
static int hex_basamak(char c)
{
    if ((c >= '0') && (c <= '9')) { return c - '0'; }
    if ((c >= 'a') && (c <= 'f')) { return (c - 'a') + 10; }
    if ((c >= 'A') && (c <= 'F')) { return (c - 'A') + 10; }
    return -1;
}

/** Hex metnini bayt dizisine cevirir. Donus: bayt sayisi, hata: -1 */
static int hex_coz(const char *metin, unsigned char *hedef, int hedef_kap)
{
    int adet = 0;

    while ((*metin != '\0') && (*metin != '\n') && (*metin != '\r'))
    {
        int yuksek;
        int dusuk;

        if (*metin == ' ') { metin++; continue; }

        yuksek = hex_basamak(*metin);
        if (yuksek < 0) { return -1; }
        metin++;

        dusuk = hex_basamak(*metin);
        if (dusuk < 0) { return -1; }
        metin++;

        if (adet >= hedef_kap) { return -1; }
        hedef[adet] = (unsigned char)((yuksek * 16) + dusuk);
        adet++;
    }
    return adet;
}

/**
 * Bosluk ayrilmis 8 haneli hex bloklarini float dizisine cevirir.
 *
 * NEDEN BIT DESENI: Float'in metin gosterimi (ondalik) belirsizdir ve
 * ayristirici farkliliklari yuzunden iki implementasyon ayni metinden
 * farkli degerler uretebilir. NaN ve negatif sifir gibi degerler de
 * metinle guvenilir tasinmaz. Bit deseni bu belirsizligi tamamen kaldirir.
 *
 * Donus: adet, hata: -1
 */
static int hexfloat_coz(const char *metin, float *hedef, int hedef_kap)
{
    int adet = 0;

    while (*metin != '\0')
    {
        uint32_t deger = 0u;
        int basamak;

        while ((*metin == ' ') || (*metin == '\t')) { metin++; }
        if ((*metin == '\0') || (*metin == '\n') || (*metin == '\r')) { break; }

        for (basamak = 0; basamak < 8; basamak++)
        {
            int h = hex_basamak(*metin);
            if (h < 0) { return -1; }
            deger = (deger << 4) | (uint32_t)h;
            metin++;
        }

        if (adet >= hedef_kap) { return -1; }
        (void)memcpy(&hedef[adet], &deger, sizeof(float));
        adet++;
    }
    return adet;
}

/** Bosluk ayrilmis tamsayilari ayristirir. Donus: adet */
static int sayilari_coz(const char *metin, int32_t *hedef, int hedef_kap)
{
    int adet = 0;
    char *son;

    while (*metin != '\0')
    {
        long deger;

        while ((*metin == ' ') || (*metin == '\t')) { metin++; }
        if ((*metin == '\0') || (*metin == '\n') || (*metin == '\r')) { break; }

        deger = strtol(metin, &son, 10);
        if (son == metin) { break; }

        if (adet >= hedef_kap) { return -1; }
        hedef[adet] = (int32_t)deger;
        adet++;
        metin = son;
    }
    return adet;
}

/** "ANAHTAR " onekini kontrol edip degeri dondurur; yoksa NULL. */
static const char *alan(const char *satir, const char *anahtar)
{
    size_t n = strlen(anahtar);

    if (strncmp(satir, anahtar, n) != 0) { return NULL; }
    if (satir[n] != ' ') { return NULL; }
    return &satir[n + 1];
}

/** Satir sonundaki bosluk/yenisatir karakterlerini kirpar. */
static void kirp(char *s)
{
    size_t n = strlen(s);

    while (n > 0u)
    {
        char c = s[n - 1u];
        if ((c == '\n') || (c == '\r') || (c == ' ') || (c == '\t'))
        {
            s[n - 1u] = '\0';
            n--;
        }
        else
        {
            break;
        }
    }
}

static void sonuc(const char *ad, const char *asama, int basarili, const char *not_)
{
    if (basarili != 0)
    {
        g_gecen++;
    }
    else
    {
        g_kalan++;
    }
    printf("  [%s] %-24s %-8s %s\n",
           (basarili != 0) ? "GECTI" : "KALDI", ad, asama,
           (not_ != NULL) ? not_ : "");
}

/**
 * Iki bayt dizisini karsilastirir; ilk farkin konumunu mesaja yazar.
 * @return 1 ayni, 0 farkli
 */
static int tampon_karsilastir(const unsigned char *a, int32_t a_boy,
                              const unsigned char *b, int32_t b_boy,
                              char *mesaj, size_t mesaj_boyutu)
{
    int32_t i;

    if (a_boy != b_boy)
    {
        (void)snprintf(mesaj, mesaj_boyutu, "boyut farkli: C=%d, beklenen=%d",
                       (int)a_boy, (int)b_boy);
        return 0;
    }

    for (i = 0; i < a_boy; i++)
    {
        if (a[i] != b[i])
        {
            (void)snprintf(mesaj, mesaj_boyutu, "bayt %d: C=0x%02X, beklenen=0x%02X",
                           (int)i, (unsigned)a[i], (unsigned)b[i]);
            return 0;
        }
    }

    (void)snprintf(mesaj, mesaj_boyutu, "%d bayt birebir", (int)a_boy);
    return 1;
}

/* ---------------------------------------------------------------------
 * FLOAT VEKTORLERI
 * ---------------------------------------------------------------------
 * Kayan nokta iki dilde kolayca ayrisir; bu yuzden hem kodlama hem
 * cozme birebir dogrulanir. Karsilastirmalar BIT DESENI uzerinden
 * yapilir: NaN kendisine esit olmadigi icin '==' ile dogrulama yaniltir.
 * ------------------------------------------------------------------- */
static int floatlar_ayni_mi(const float *a, const float *b, int32_t adet, int32_t *ilk_fark)
{
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        uint32_t ba;
        uint32_t bb;
        (void)memcpy(&ba, &a[i], sizeof(ba));
        (void)memcpy(&bb, &b[i], sizeof(bb));
        if (ba != bb)
        {
            *ilk_fark = i;
            return 0;
        }
    }
    return 1;
}

static void float_vektoru_dogrula(const char  *ad,
                                  const char  *katman,
                                  int32_t      kanal,
                                  const float *girdif,
                                  int32_t      girdif_adet,
                                  const float *olcekler,
                                  int32_t      olcek_adet,
                                  const unsigned char *beklenen,
                                  int32_t      beklenen_adet)
{
    static int32_t tam[MAKS_ELEMAN_T];
    static float   geri[MAKS_ELEMAN_T];
    static float   calisma[MAKS_ELEMAN_T];
    unsigned char  uretilen[MAKS_BAYT_T];
    char    mesaj[128];
    int32_t n = -1;
    int32_t durum;
    int     esit;
    int32_t i;
    int32_t ilk_fark = -1;

    if (strcmp(katman, "float_kuantala") == 0)
    {
        if (olcek_adet < kanal)
        {
            sonuc(ad, "kodlama", 0, "olcek dizisi eksik");
            return;
        }

        durum = elbari_float_kuantala_kanalli(girdif, girdif_adet, kanal, olcekler, tam);
        if (durum != ELBARI_TAMAM)
        {
            sonuc(ad, "kodlama", 0, "kuantalama hata dondu");
            return;
        }

        /* Beklenen cikti: int32 dizisinin little-endian baytlari */
        n = girdif_adet * 4;
        if (n > (int32_t)sizeof(uretilen))
        {
            sonuc(ad, "kodlama", 0, "vektor cok buyuk");
            return;
        }
        for (i = 0; i < girdif_adet; i++)
        {
            uretilen[(i * 4) + 0] = (unsigned char)((uint32_t)tam[i] & 0xFFu);
            uretilen[(i * 4) + 1] = (unsigned char)(((uint32_t)tam[i] >> 8) & 0xFFu);
            uretilen[(i * 4) + 2] = (unsigned char)(((uint32_t)tam[i] >> 16) & 0xFFu);
            uretilen[(i * 4) + 3] = (unsigned char)(((uint32_t)tam[i] >> 24) & 0xFFu);
        }

        esit = tampon_karsilastir(uretilen, n, beklenen, beklenen_adet, mesaj, sizeof(mesaj));
        sonuc(ad, "kodlama", esit, mesaj);

        /* Cozme: kuantalanmis degerlerden geri don, hata sinirini kontrol et */
        durum = elbari_float_coz_kanalli(tam, girdif_adet, kanal, olcekler, geri);
        if (durum != ELBARI_TAMAM)
        {
            sonuc(ad, "cozme", 0, "coz hata dondu");
            return;
        }
        {
            float maks = elbari_float_maks_hata(girdif, geri, girdif_adet);
            float en_kaba = 0.0f;

            for (i = 0; i < kanal; i++)
            {
                float adim = 1.0f / olcekler[i];
                if (adim > en_kaba) { en_kaba = adim; }
            }
            (void)snprintf(mesaj, sizeof(mesaj), "maks hata %.3e <= %.3e",
                           (double)maks, (double)(en_kaba * 0.5f * 1.001f));
            sonuc(ad, "cozme", (maks <= (en_kaba * 0.5f * 1.001f)) ? 1 : 0, mesaj);
        }
        return;
    }

    /* ---- float_xor ve float_xor_kanal ---- */
    if (strcmp(katman, "float_xor") == 0)
    {
        n = elbari_float_xor_kabid(girdif, girdif_adet, uretilen, (int32_t)sizeof(uretilen));
    }
    else if (strcmp(katman, "float_xor_kanal") == 0)
    {
        n = elbari_float_xor_kanal_kabid(girdif, girdif_adet, kanal,
                                         calisma, (int32_t)MAKS_ELEMAN_T,
                                         uretilen, (int32_t)sizeof(uretilen));
    }
    else
    {
        sonuc(ad, "kodlama", 0, "bilinmeyen float katmani");
        return;
    }

    if (n < 0)
    {
        sonuc(ad, "kodlama", 0, "kodlama hata dondu");
        return;
    }

    esit = tampon_karsilastir(uretilen, n, beklenen, beklenen_adet, mesaj, sizeof(mesaj));
    sonuc(ad, "kodlama", esit, mesaj);

    /* Cozme: referans baytlardan geri don, BIT BIT ayni olmali (kayipsiz) */
    if (strcmp(katman, "float_xor") == 0)
    {
        durum = elbari_float_xor_basit(beklenen, beklenen_adet, geri, girdif_adet);
    }
    else
    {
        durum = elbari_float_xor_kanal_basit(beklenen, beklenen_adet,
                                             calisma, (int32_t)MAKS_ELEMAN_T,
                                             geri, girdif_adet);
    }

    if (durum != ELBARI_TAMAM)
    {
        (void)snprintf(mesaj, sizeof(mesaj), "cozme hata kodu: %d", (int)durum);
        sonuc(ad, "cozme", 0, mesaj);
        return;
    }

    if (floatlar_ayni_mi(girdif, geri, girdif_adet, &ilk_fark) != 0)
    {
        (void)snprintf(mesaj, sizeof(mesaj), "%d deger BIT BIT ayni", (int)girdif_adet);
        sonuc(ad, "cozme", 1, mesaj);
    }
    else
    {
        (void)snprintf(mesaj, sizeof(mesaj), "eleman %d bit deseni farkli", (int)ilk_fark);
        sonuc(ad, "cozme", 0, mesaj);
    }
}

/* ---------------------------------------------------------------------
 * TEK BIR VEKTORU DOGRULA
 * ------------------------------------------------------------------- */
static void vektoru_dogrula(const char *ad,
                            const char *katman,
                            int32_t     kanal,
                            uint32_t    sirano,
                            const int32_t *girdi,
                            int32_t     girdi_adet,
                            const unsigned char *beklenen,
                            int32_t     beklenen_adet)
{
    unsigned char uretilen[MAKS_BAYT_T];
    int32_t geri[MAKS_ELEMAN_T];
    int32_t calisma[MAKS_ELEMAN_T];
    int32_t n = -1;
    int32_t durum;
    char mesaj[128];
    int i;
    int esit;

    /* ---- 1) KODLAMA ---- */
    if (strcmp(katman, "cekirdek") == 0)
    {
        n = elbari_kabid(girdi, girdi_adet, uretilen, (int32_t)sizeof(uretilen));
    }
    else if (strcmp(katman, "kanal") == 0)
    {
        n = elbari_kanal_kabid(girdi, girdi_adet, kanal,
                               calisma, (int32_t)MAKS_ELEMAN_T,
                               uretilen, (int32_t)sizeof(uretilen));
    }
    else if (strcmp(katman, "cerceve") == 0)
    {
        n = elbari_cerceve_yaz(girdi, girdi_adet, kanal, sirano,
                               calisma, (int32_t)MAKS_ELEMAN_T,
                               uretilen, (int32_t)sizeof(uretilen));
    }
    else
    {
        sonuc(ad, "kodlama", 0, "bilinmeyen katman");
        return;
    }

    if (n != beklenen_adet)
    {
        (void)snprintf(mesaj, sizeof(mesaj), "boyut farkli: C=%d, beklenen=%d",
                       (int)n, (int)beklenen_adet);
        sonuc(ad, "kodlama", 0, mesaj);
    }
    else
    {
        esit = 1;
        for (i = 0; i < n; i++)
        {
            if (uretilen[i] != beklenen[i])
            {
                (void)snprintf(mesaj, sizeof(mesaj),
                               "bayt %d: C=0x%02X, beklenen=0x%02X",
                               i, (unsigned)uretilen[i], (unsigned)beklenen[i]);
                esit = 0;
                break;
            }
        }
        if (esit != 0)
        {
            (void)snprintf(mesaj, sizeof(mesaj), "%d bayt birebir", (int)n);
        }
        sonuc(ad, "kodlama", esit, mesaj);
    }

    /* ---- 2) COZME (referans baytlardan) ---- */
    for (i = 0; i < MAKS_ELEMAN_T; i++) { geri[i] = 0; }

    if (strcmp(katman, "cekirdek") == 0)
    {
        durum = elbari_basit(beklenen, beklenen_adet, geri, girdi_adet);
    }
    else if (strcmp(katman, "kanal") == 0)
    {
        durum = elbari_kanal_basit(beklenen, beklenen_adet,
                                   calisma, (int32_t)MAKS_ELEMAN_T,
                                   geri, girdi_adet);
    }
    else
    {
        uint32_t okunan_sira = 0u;
        int32_t okunan_adet = 0;

        durum = elbari_cerceve_oku(beklenen, beklenen_adet, kanal,
                                   calisma, (int32_t)MAKS_ELEMAN_T,
                                   geri, girdi_adet,
                                   &okunan_sira, &okunan_adet);

        if ((durum == ELBARI_TAMAM) && (okunan_sira != sirano))
        {
            (void)snprintf(mesaj, sizeof(mesaj), "sira no farkli: %u != %u",
                           (unsigned)okunan_sira, (unsigned)sirano);
            sonuc(ad, "cozme", 0, mesaj);
            return;
        }
    }

    if (durum != ELBARI_TAMAM)
    {
        (void)snprintf(mesaj, sizeof(mesaj), "cozme hata kodu: %d", (int)durum);
        sonuc(ad, "cozme", 0, mesaj);
        return;
    }

    esit = 1;
    for (i = 0; i < girdi_adet; i++)
    {
        if (geri[i] != girdi[i])
        {
            (void)snprintf(mesaj, sizeof(mesaj), "eleman %d: %d != %d",
                           i, (int)geri[i], (int)girdi[i]);
            esit = 0;
            break;
        }
    }
    if (esit != 0)
    {
        (void)snprintf(mesaj, sizeof(mesaj), "%d eleman geri geldi", (int)girdi_adet);
    }
    sonuc(ad, "cozme", esit, mesaj);
}

/* =====================================================================
 * ANA
 * ===================================================================== */
int main(int argc, char **argv)
{
    FILE *f;
    char satir[MAKS_SATIR];
    char ad[128];
    char katman[64];
    int32_t kanal = 1;
    uint32_t sirano = 0u;
    int32_t girdi[MAKS_ELEMAN_T];
    int32_t girdi_adet = 0;
    static float girdif[MAKS_ELEMAN_T];
    int32_t girdif_adet = 0;
    static float olcekler[ELBARI_MAKS_KANAL];
    int32_t olcek_adet = 0;
    unsigned char beklenen[MAKS_BAYT_T];
    int32_t beklenen_adet = 0;
    int vektor_sayisi = 0;
    const char *deger;

    if (argc < 2)
    {
        (void)fprintf(stderr, "Kullanim: uygunluk <vektor_dosyasi>\n");
        return 2;
    }

    f = fopen(argv[1], "r");
    if (f == NULL)
    {
        (void)fprintf(stderr, "HATA: vektor dosyasi acilamadi: %s\n", argv[1]);
        return 2;
    }

    printf("=====================================================================\n");
    printf("  ELBARI - Uygunluk (conformance) testi\n");
    printf("  Vektor dosyasi: %s\n", argv[1]);
    printf("=====================================================================\n\n");

    ad[0] = '\0';
    katman[0] = '\0';

    while (fgets(satir, (int)sizeof(satir), f) != NULL)
    {
        kirp(satir);

        if ((satir[0] == '#') || (satir[0] == '\0')) { continue; }

        deger = alan(satir, "VEKTOR");
        if (deger != NULL)
        {
            /* Kirpma kasitlidir: kaynak satir cok uzun olabilir. Genislik
             * belirteci ile bunu derleyiciye acikca bildiriyoruz. */
            (void)snprintf(ad, sizeof(ad), "%.*s", (int)sizeof(ad) - 1, deger);
            kanal = 1;
            sirano = 0u;
            girdi_adet = 0;
            girdif_adet = 0;
            olcek_adet = 0;
            beklenen_adet = 0;
            continue;
        }

        deger = alan(satir, "KATMAN");
        if (deger != NULL) { (void)snprintf(katman, sizeof(katman), "%.*s",
                                       (int)sizeof(katman) - 1, deger);
            continue; }

        deger = alan(satir, "KANAL");
        if (deger != NULL) { kanal = (int32_t)atoi(deger); continue; }

        deger = alan(satir, "SIRANO");
        if (deger != NULL) { sirano = (uint32_t)strtoul(deger, NULL, 10); continue; }

        deger = alan(satir, "GIRDI");
        if (deger != NULL)
        {
            girdi_adet = (int32_t)sayilari_coz(deger, girdi, MAKS_ELEMAN_T);
            continue;
        }

        deger = alan(satir, "GIRDIF");
        if (deger != NULL)
        {
            girdif_adet = (int32_t)hexfloat_coz(deger, girdif, MAKS_ELEMAN_T);
            continue;
        }

        deger = alan(satir, "OLCEKLER");
        if (deger != NULL)
        {
            olcek_adet = (int32_t)hexfloat_coz(deger, olcekler, ELBARI_MAKS_KANAL);
            continue;
        }

        deger = alan(satir, "CIKTI");
        if (deger != NULL)
        {
            beklenen_adet = (int32_t)hex_coz(deger, beklenen, MAKS_BAYT_T);
            continue;
        }

        if (strcmp(satir, "SON") == 0)
        {
            if ((girdif_adet > 0) && (beklenen_adet > 0))
            {
                vektor_sayisi++;
                float_vektoru_dogrula(ad, katman, kanal,
                                      girdif, girdif_adet,
                                      olcekler, olcek_adet,
                                      beklenen, beklenen_adet);
            }
            else if ((girdi_adet > 0) && (beklenen_adet > 0))
            {
                vektor_sayisi++;
                vektoru_dogrula(ad, katman, kanal, sirano,
                                girdi, girdi_adet, beklenen, beklenen_adet);
            }
            else
            {
                /* eksik vektor: yok sayilir */
            }
            continue;
        }

        /* ACIKLAMA ve bilinmeyen alanlar yok sayilir */
    }

    (void)fclose(f);

    printf("\n=====================================================================\n");
    printf("  %d vektor dogrulandi: %d gecti, %d kaldi\n",
           vektor_sayisi, g_gecen, g_kalan);
    printf("=====================================================================\n");

    return (g_kalan == 0) ? 0 : 1;
}
