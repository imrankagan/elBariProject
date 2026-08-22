/* =====================================================================
 * ELBARI - Kodlayici deger fuzz'i (encode -> decode tam tur)
 * ---------------------------------------------------------------------
 * AMAC:
 *   Kodlayiciya DUSMANCA DEGER DAGILIMLARI verip ciktinin bit bit geri
 *   geldigini dogrular. Sorulan soru sudur:
 *
 *       "Kodlayicinin kabul ettigi HER girdi, kayipsiz geri geliyor mu?"
 *
 * NEDEN AYRI BIR ARAC:
 *   fuzz.c bir COZUCU saglamlik testidir: bozuk baytlari cozucuye verir
 *   ve cokmemesini bekler. Kodlayiciyi hic calistirmaz. Bu yuzden
 *   kodlayici tarafinda olusan sessiz veri kayiplarini goremez.
 *
 *   Bunun bedeli olculdu: tam olarak 2^31'lik bir ardisik fark (isareti
 *   degisip buyuklugu ayni kalan bir float bit deseni) blok bit
 *   genisligini yukseltmiyor, aykiri da sayilmiyor ve dar maskeyle
 *   paketlenip UST BITINI KAYBEDIYORDU. Ne 27 uygunluk vektoru ne de
 *   300.000 turluk cozucu fuzz'i bunu yakaladi; hata ancak gercek bir
 *   ArduPilot ucus logunun jiroskop kanali baglaninca ortaya cikti.
 *
 *   Bu arac o boslugu kapatir.
 *
 * YONTEM:
 *   - Uretecler kenar durumlari BILEREK hedefler: aykiri esigi (32767),
 *     tam 2^31'lik farklar, INT32_MIN/MAX, sifir kosulari, isaret
 *     degisimleri, float bit desenleri.
 *   - Cikti tamponlari kanarya ile cevrilir; sessiz tasma da yakalanir.
 *   - Uretec deterministiktir: bir hata bulunursa tohum yazdirilir ve
 *     durum birebir yeniden uretilebilir.
 *
 * KABUL / RED:
 *   Kodlayicinin ELBARI_SIKISTIRILAMAZ dondurmesi HATA DEGILDIR; cagiran
 *   veriyi ham gonderir. Sayilir ama basarisizlik olarak islenmez.
 *   Hata yalnizca sudur: kodlayici kabul etti, ama geri gelen veri
 *   girenle ayni degil.
 * ===================================================================== */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/elbari.h"

/* --------------------------------------------------------------------- */

#define KANARYA_BAYT     (64)
#define KANARYA_DESENI   (0x5Au)

#define MAKS_ELEMAN      (2048)
#define MAKS_KANAL_TEST  (8)

static unsigned long long g_tohum = 0ull;

/** Deterministik sozde-rastgele uretec (xorshift64). */
static unsigned int rastgele(void)
{
    g_tohum ^= g_tohum << 13;
    g_tohum ^= g_tohum >> 7;
    g_tohum ^= g_tohum << 17;
    return (unsigned int)(g_tohum >> 32);
}

static unsigned int rastgele_aralik(unsigned int ust)
{
    return (ust == 0u) ? 0u : (rastgele() % ust);
}

static int32_t rastgele_i32(void)
{
    return (int32_t)rastgele();
}

/**
 * Saran toplama.
 *
 * Uretecler durum degiskenini surekli ilerletir ve bu ISARETLI TASMAYA
 * yol acar - C'de tanimsiz davranistir. UBSan bunu CI'da yakaladi.
 * Kutuphanenin kendisi zaten isaretsiz aritmetik kullaniyor; burada
 * eksik olan test aracinin kendisiydi.
 */
static int32_t sar_topla(int32_t a, int32_t b)
{
    uint32_t t = (uint32_t)a + (uint32_t)b;
    int32_t r;

    (void)memcpy(&r, &t, sizeof(r));
    return r;
}

/* =====================================================================
 * DEGER URETECLERI
 * ---------------------------------------------------------------------
 * Her uretec, kodlayicinin farkli bir karar yolunu zorlar. Isimler
 * hata raporunda gorunur; bir hata bulundugunda hangi desenin kirdigi
 * dogrudan okunur.
 * ===================================================================== */

#define URETEC_ADEDI (12)

static const char *URETEC_ADI[URETEC_ADEDI] =
{
    "saf_rastgele",        /* tum 32 bit rastgele - cogu blok aykiri     */
    "duzgun_kucuk",        /* kucuk farklar - dar bit genisligi          */
    "sabit",               /* tum farklar sifir - SIFIR BLOK yolu        */
    "isaret_donusu_2p31",  /* fark TAM 2^31 - kayipsizlik hatasi buradan */
    "sinir_degerleri",     /* INT32_MIN/MAX, 0, +-32767, +-32768         */
    "aykiri_esiginde",     /* farklar tam esigin iki yaninda             */
    "float_bitleri",       /* gercekci float bit desenleri               */
    "seyrek_sicrama",      /* cogu sifir + nadir dev sicrama             */
    "sabit_hiz",           /* ikinci derece fark yolunu zorlar           */
    "sifir_kosusu",        /* uzun sifir bloklari + arada tek sicrama    */
    "artan_genislik",      /* her blok bir sonraki bit genisligine gecer */
    "karisik"              /* eleman basina rastgele uretec secimi       */
};

/** +0.001f / -0.001f gibi, yalnizca isaret biti farkli iki bit deseni. */
static void ikili_isaret_deseni(int32_t *a, int32_t *b)
{
    uint32_t taban = (rastgele() & 0x7FFFFFFFu);
    uint32_t p = taban;
    uint32_t n = taban | 0x80000000u;

    (void)memcpy(a, &p, sizeof(*a));
    (void)memcpy(b, &n, sizeof(*b));
}

static int32_t sinir_degeri(void)
{
    static const int32_t SINIRLAR[10] =
    {
        (-2147483647 - 1), 2147483647, 0, -1, 1,
        32767, -32767, 32768, -32768, 65535
    };
    return SINIRLAR[rastgele_aralik(10u)];
}

/** Tek bir eleman uretir (uretec 11 "karisik" bunu eleman basina cagirir). */
static int32_t tek_deger(int32_t uretec, int32_t i, int32_t *durum)
{
    int32_t d;

    switch (uretec)
    {
    case 0:
        return rastgele_i32();

    case 1:
        *durum = sar_topla(*durum, (int32_t)rastgele_aralik(9u) - 4);
        return *durum;

    case 2:
        return *durum;

    case 3:
    {
        int32_t a;
        int32_t b;
        ikili_isaret_deseni(&a, &b);
        return ((i % 2) == 0) ? a : b;
    }

    case 4:
        return sinir_degeri();

    case 5:
        /* Farklar esigin (32767) hemen iki yaninda gezinir. */
        d = (int32_t)rastgele_aralik(5u) - 2;
        *durum = sar_topla(*durum, (((i % 2) == 0) ? 32767 : -32766) + d);
        return *durum;

    case 6:
    {
        /* Gercekci bir yonelim degeri gibi: kucuk, isaret degistiren float */
        float f = ((float)((int32_t)rastgele_aralik(2000u) - 1000)) * 0.001f;
        int32_t bits;
        (void)memcpy(&bits, &f, sizeof(bits));
        return bits;
    }

    case 7:
        return (rastgele_aralik(32u) == 0u) ? rastgele_i32() : *durum;

    case 8:
        *durum = sar_topla(*durum, 1000);
        return *durum;

    case 9:
        /* Uzun sifir kosulari, arada tek sicrama: blok-ustu sifir yollari */
        if (rastgele_aralik(64u) == 0u)
        {
            *durum = sar_topla(*durum, rastgele_i32());
        }
        return *durum;

    case 10:
    default:
    {
        /* Blok numarasina gore genisleyen fark buyuklugu */
        int32_t blok = i / ELBARI_BLOK_BOYUTU;
        int32_t genislik = blok % 18;
        int32_t araligi = (genislik >= 31) ? 2147483647 : (1 << genislik);
        *durum = sar_topla(*durum,
                           (int32_t)rastgele_aralik((unsigned int)araligi)
                           - (araligi / 2));
        return *durum;
    }
    }
}

static void veri_uret(int32_t uretec, int32_t *hedef, int32_t adet)
{
    int32_t durum = rastgele_i32();
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        int32_t u = (uretec == 11) ? (int32_t)rastgele_aralik(11u) : uretec;
        hedef[i] = tek_deger(u, i, &durum);
    }
}

/* =====================================================================
 * KANARYA
 * ===================================================================== */

static void kanarya_doldur(uint8_t *tampon, int32_t toplam)
{
    (void)memset(tampon, (int)KANARYA_DESENI, (size_t)toplam);
}

static int32_t kanarya_saglam(const uint8_t *tampon, int32_t govde_boyutu)
{
    int32_t i;

    for (i = 0; i < KANARYA_BAYT; i++)
    {
        if (tampon[i] != KANARYA_DESENI) { return 0; }
        if (tampon[KANARYA_BAYT + govde_boyutu + i] != KANARYA_DESENI)
        {
            return 0;
        }
    }
    return 1;
}

/* =====================================================================
 * SAYAClAR
 * ===================================================================== */

typedef struct
{
    long kabul;
    long red;
    long hata;
    long tasma;
} sayac;

static sayac g_cekirdek;
static sayac g_kanal;
static sayac g_cerceve;
static sayac g_floatxor;

static void hata_yaz(const char *katman, const char *uretec_adi,
                     unsigned long long tohum, int32_t indeks,
                     int32_t giren, int32_t cikan)
{
    printf("\n  !!! KAYIP: %s / %s\n", katman, uretec_adi);
    printf("      tohum   : 0x%llX  (tekrar uretmek icin)\n", tohum);
    printf("      eleman  : %d\n", (int)indeks);
    printf("      giren   : %11d  (0x%08X)\n",
           (int)giren, (unsigned)giren);
    printf("      cikan   : %11d  (0x%08X)\n",
           (int)cikan, (unsigned)cikan);
    printf("      fark    : 0x%08X\n",
           (unsigned)((uint32_t)cikan ^ (uint32_t)giren));
}

/* =====================================================================
 * KATMAN TESTLERI
 * ===================================================================== */

static void cekirdek_turu(int32_t uretec, int32_t *veri, int32_t *geri,
                          uint8_t *cikti_ham)
{
    int32_t adet = (int32_t)rastgele_aralik(MAKS_ELEMAN) + 1;
    int32_t kap = elbari_cekirdek_en_kotu_durum_boyutu(adet);
    uint8_t *cikti = &cikti_ham[KANARYA_BAYT];
    unsigned long long tohum_once = g_tohum;
    int32_t n;
    int32_t i;

    veri_uret(uretec, veri, adet);
    kanarya_doldur(cikti_ham, (KANARYA_BAYT * 2) + kap);

    n = elbari_kabid(veri, adet, cikti, kap);
    if (n == ELBARI_SIKISTIRILAMAZ) { g_cekirdek.red++; return; }
    if (n < 0) { g_cekirdek.red++; return; }

    if (kanarya_saglam(cikti_ham, kap) == 0)
    {
        printf("\n  !!! TAMPON TASMASI: cekirdek / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_cekirdek.tasma++;
        return;
    }

    g_cekirdek.kabul++;

    if (elbari_basit(cikti, n, geri, adet) != ELBARI_TAMAM)
    {
        printf("\n  !!! COZULEMEDI: cekirdek / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_cekirdek.hata++;
        return;
    }

    for (i = 0; i < adet; i++)
    {
        if (veri[i] != geri[i])
        {
            hata_yaz("cekirdek", URETEC_ADI[uretec], tohum_once,
                     i, veri[i], geri[i]);
            g_cekirdek.hata++;
            return;
        }
    }
}

static void kanal_turu(int32_t uretec, int32_t *veri, int32_t *geri,
                       int32_t *calisma, uint8_t *cikti_ham)
{
    int32_t kanal = (int32_t)rastgele_aralik(MAKS_KANAL_TEST) + 1;
    int32_t kayit = (int32_t)rastgele_aralik(MAKS_ELEMAN / MAKS_KANAL_TEST) + 1;
    int32_t adet = kayit * kanal;
    int32_t kap = elbari_kanal_en_kotu_durum_boyutu(adet, kanal);
    int32_t calisma_kap = elbari_kanal_gerekli_calisma_alani(adet, kanal);
    uint8_t *cikti = &cikti_ham[KANARYA_BAYT];
    unsigned long long tohum_once = g_tohum;
    int32_t n;
    int32_t i;

    veri_uret(uretec, veri, adet);
    kanarya_doldur(cikti_ham, (KANARYA_BAYT * 2) + kap);

    n = elbari_kanal_kabid(veri, adet, kanal, calisma, calisma_kap, cikti, kap);
    if (n < 0) { g_kanal.red++; return; }

    if (kanarya_saglam(cikti_ham, kap) == 0)
    {
        printf("\n  !!! TAMPON TASMASI: kanal / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_kanal.tasma++;
        return;
    }

    g_kanal.kabul++;

    if (elbari_kanal_basit(cikti, n, calisma, calisma_kap, geri, adet)
        != ELBARI_TAMAM)
    {
        printf("\n  !!! COZULEMEDI: kanal / %s (tohum 0x%llX, K=%d)\n",
               URETEC_ADI[uretec], tohum_once, (int)kanal);
        g_kanal.hata++;
        return;
    }

    for (i = 0; i < adet; i++)
    {
        if (veri[i] != geri[i])
        {
            hata_yaz("kanal", URETEC_ADI[uretec], tohum_once,
                     i, veri[i], geri[i]);
            printf("      kanal sayisi: %d, kanal indeksi: %d\n",
                   (int)kanal, (int)(i % kanal));
            g_kanal.hata++;
            return;
        }
    }
}

static void cerceve_turu(int32_t uretec, int32_t *veri, int32_t *geri,
                         int32_t *calisma, uint8_t *cikti_ham)
{
    int32_t kanal = (int32_t)rastgele_aralik(MAKS_KANAL_TEST) + 1;
    int32_t kayit = (int32_t)rastgele_aralik(200u) + 1;
    int32_t adet = kayit * kanal;
    int32_t kap = elbari_cerceve_en_kotu_durum_boyutu(kayit, kanal);
    int32_t calisma_kap = elbari_cerceve_gerekli_calisma_alani(kayit, kanal);
    uint8_t *cikti = &cikti_ham[KANARYA_BAYT];
    unsigned long long tohum_once = g_tohum;
    uint32_t sira_giren = rastgele();
    uint32_t sira_cikan = 0u;
    int32_t kayit_cikan = 0;
    int32_t n;
    int32_t i;

    veri_uret(uretec, veri, adet);
    kanarya_doldur(cikti_ham, (KANARYA_BAYT * 2) + kap);

    n = elbari_cerceve_yaz(veri, adet, kanal, sira_giren,
                           calisma, calisma_kap, cikti, kap);
    if (n < 0) { g_cerceve.red++; return; }

    if (kanarya_saglam(cikti_ham, kap) == 0)
    {
        printf("\n  !!! TAMPON TASMASI: cerceve / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_cerceve.tasma++;
        return;
    }

    g_cerceve.kabul++;

    if (elbari_cerceve_oku(cikti, n, kanal, calisma, calisma_kap,
                           geri, adet, &sira_cikan, &kayit_cikan)
        != ELBARI_TAMAM)
    {
        printf("\n  !!! COZULEMEDI: cerceve / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_cerceve.hata++;
        return;
    }

    /* Bicim surumu 4: sira no baslikta 16 BITTIR ve sarar. Kayip ve
     * siralama tespiti icin yeterlidir (RTP de 16 bit kullanir). Bu
     * yuzden karsilastirma dusuk 16 bit uzerinden yapilir. */
    if ((sira_cikan != (sira_giren & 0xFFFFu)) || (kayit_cikan != kayit))
    {
        printf("\n  !!! BASLIK BOZUK: cerceve / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        printf("      sira  giren %u (dusuk 16 bit %u)  cikan %u\n",
               (unsigned)sira_giren, (unsigned)(sira_giren & 0xFFFFu),
               (unsigned)sira_cikan);
        printf("      kayit giren %d  cikan %d\n",
               (int)kayit, (int)kayit_cikan);
        g_cerceve.hata++;
        return;
    }

    for (i = 0; i < adet; i++)
    {
        if (veri[i] != geri[i])
        {
            hata_yaz("cerceve", URETEC_ADI[uretec], tohum_once,
                     i, veri[i], geri[i]);
            g_cerceve.hata++;
            return;
        }
    }
}

static void floatxor_turu(int32_t uretec, int32_t *veri,
                          float *fveri, float *fgeri, uint8_t *cikti_ham)
{
    int32_t adet = (int32_t)rastgele_aralik(512u) + 1;
    int32_t kap = elbari_float_xor_en_kotu_durum_boyutu(adet);
    uint8_t *cikti = &cikti_ham[KANARYA_BAYT];
    unsigned long long tohum_once = g_tohum;
    int32_t n;
    int32_t i;

    /* Bit desenleri float olarak yorumlanir: NaN, sonsuz, -0.0 dahil
     * her sey akisa girer. Karsilastirma da BIT UZERINDEN yapilir. */
    veri_uret(uretec, veri, adet);
    for (i = 0; i < adet; i++)
    {
        (void)memcpy(&fveri[i], &veri[i], sizeof(float));
    }

    kanarya_doldur(cikti_ham, (KANARYA_BAYT * 2) + kap);

    n = elbari_float_xor_kabid(fveri, adet, cikti, kap);
    if (n < 0) { g_floatxor.red++; return; }

    if (kanarya_saglam(cikti_ham, kap) == 0)
    {
        printf("\n  !!! TAMPON TASMASI: float XOR / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_floatxor.tasma++;
        return;
    }

    g_floatxor.kabul++;

    if (elbari_float_xor_basit(cikti, n, fgeri, adet) != ELBARI_TAMAM)
    {
        printf("\n  !!! COZULEMEDI: float XOR / %s (tohum 0x%llX)\n",
               URETEC_ADI[uretec], tohum_once);
        g_floatxor.hata++;
        return;
    }

    for (i = 0; i < adet; i++)
    {
        int32_t a;
        int32_t b;

        (void)memcpy(&a, &fveri[i], sizeof(a));
        (void)memcpy(&b, &fgeri[i], sizeof(b));
        if (a != b)
        {
            /* NaN == NaN yanlis doner; bu yuzden bit karsilastirmasi. */
            hata_yaz("float XOR", URETEC_ADI[uretec], tohum_once, i, a, b);
            g_floatxor.hata++;
            return;
        }
    }
}

/* =====================================================================
 * RAPOR
 * ===================================================================== */

static void katman_yaz(const char *ad, const sayac *s)
{
    printf("    %-16s kabul %8ld   red %8ld   KAYIP %ld   TASMA %ld\n",
           ad, s->kabul, s->red, s->hata, s->tasma);
}

int main(int argc, char **argv)
{
    long tur = 200000;
    long t;
    int32_t *veri;
    int32_t *geri;
    int32_t *calisma;
    float   *fveri;
    float   *fgeri;
    uint8_t *cikti_ham;
    int32_t  cikti_kap;
    long toplam_hata;
    long toplam_tasma;

    if (argc >= 2) { tur = atol(argv[1]); }
    if (tur < 1) { tur = 1; }

    g_tohum = (argc >= 3)
              ? strtoull(argv[2], NULL, 0)
              : 0x9E3779B97F4A7C15ull;
    if (g_tohum == 0ull) { g_tohum = 0x9E3779B97F4A7C15ull; }

    printf("=====================================================================\n");
    printf("  ELBARI - Kodlayici deger fuzz'i (encode -> decode tam tur)\n");
    printf("=====================================================================\n");
    printf("Tur sayisi : %ld\n", tur);
    printf("Tohum      : 0x%llX (deterministik, tekrarlanabilir)\n", g_tohum);
    printf("Soru       : kodlayicinin KABUL ettigi her girdi kayipsiz mi?\n");
    printf("Uretecler  : %d dusmanca deger dagilimi\n\n", (int)URETEC_ADEDI);

    cikti_kap = elbari_kanal_en_kotu_durum_boyutu(MAKS_ELEMAN, 1)
                + (KANARYA_BAYT * 2) + 4096;

    veri      = (int32_t *)malloc((size_t)MAKS_ELEMAN * sizeof(int32_t));
    geri      = (int32_t *)malloc((size_t)MAKS_ELEMAN * sizeof(int32_t));
    calisma   = (int32_t *)malloc((size_t)(MAKS_ELEMAN * 4) * sizeof(int32_t));
    fveri     = (float *)malloc((size_t)MAKS_ELEMAN * sizeof(float));
    fgeri     = (float *)malloc((size_t)MAKS_ELEMAN * sizeof(float));
    cikti_ham = (uint8_t *)malloc((size_t)cikti_kap);

    if ((veri == NULL) || (geri == NULL) || (calisma == NULL) ||
        (fveri == NULL) || (fgeri == NULL) || (cikti_ham == NULL))
    {
        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
        return 2;
    }

    for (t = 0; t < tur; t++)
    {
        int32_t uretec = (int32_t)rastgele_aralik((unsigned int)URETEC_ADEDI);

        switch (rastgele_aralik(4u))
        {
        case 0:  cekirdek_turu(uretec, veri, geri, cikti_ham); break;
        case 1:  kanal_turu(uretec, veri, geri, calisma, cikti_ham); break;
        case 2:  cerceve_turu(uretec, veri, geri, calisma, cikti_ham); break;
        default: floatxor_turu(uretec, veri, fveri, fgeri, cikti_ham); break;
        }

        if ((t % 25000) == 0)
        {
            printf("  ... %ld tur\n", t);
        }
    }

    toplam_hata = g_cekirdek.hata + g_kanal.hata + g_cerceve.hata
                  + g_floatxor.hata;
    toplam_tasma = g_cekirdek.tasma + g_kanal.tasma + g_cerceve.tasma
                   + g_floatxor.tasma;

    printf("\n---------------------------------------------------------------------\n");
    printf("SONUC\n");
    printf("---------------------------------------------------------------------\n");
    printf("  Toplam tur     : %ld\n", tur);
    printf("  KAYIP VEREN    : %ld\n", toplam_hata);
    printf("  TAMPON TASMASI : %ld\n\n", toplam_tasma);

    printf("  Katman kirilimi:\n");
    katman_yaz("cekirdek", &g_cekirdek);
    katman_yaz("kanal", &g_kanal);
    katman_yaz("cerceve", &g_cerceve);
    katman_yaz("float XOR", &g_floatxor);

    printf("\n  red = kodlayici \"sikistirilamaz\" dedi; cagiran ham gonderir.\n");
    printf("        Hata degildir, bu testin konusu disindadir.\n");

    free(veri);
    free(geri);
    free(calisma);
    free(fveri);
    free(fgeri);
    free(cikti_ham);

    if ((toplam_hata == 0) && (toplam_tasma == 0))
    {
        printf("\n  [BASARILI] Kabul edilen her girdi bit bit geri geldi.\n");
        printf("=====================================================================\n");
        return 0;
    }

    printf("\n  [BASARISIZ] Yukaridaki tohumla tekrar calistirin:\n");
    printf("              deger_fuzz <tur> <tohum>\n");
    printf("=====================================================================\n");
    return 1;
}
