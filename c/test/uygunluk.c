/* =====================================================================
 * ELBARI - Uygunluk (conformance) testi
 * ---------------------------------------------------------------------
 * AMAC:
 *   TestVectors/vektorler.txt icindeki DONMUS referans vektorlerini
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
            (void)snprintf(ad, sizeof(ad), "%s", deger);
            kanal = 1;
            sirano = 0u;
            girdi_adet = 0;
            beklenen_adet = 0;
            continue;
        }

        deger = alan(satir, "KATMAN");
        if (deger != NULL) { (void)snprintf(katman, sizeof(katman), "%s", deger); continue; }

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

        deger = alan(satir, "CIKTI");
        if (deger != NULL)
        {
            beklenen_adet = (int32_t)hex_coz(deger, beklenen, MAKS_BAYT_T);
            continue;
        }

        if (strcmp(satir, "SON") == 0)
        {
            if ((girdi_adet > 0) && (beklenen_adet > 0))
            {
                vektor_sayisi++;
                vektoru_dogrula(ad, katman, kanal, sirano,
                                girdi, girdi_adet, beklenen, beklenen_adet);
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
