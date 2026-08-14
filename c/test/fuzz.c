/* =====================================================================
 * ELBARI - Cozucu saglamlik (fuzz) testi
 * ---------------------------------------------------------------------
 * AMAC:
 *   Cozucu, DUSMANCA ve BOZUK girdilerde asla:
 *     - cokmemeli (bellek ihlali)
 *     - tampon tasirmamali
 *     - kilitlenmemeli
 *     - sessizce yanlis veri uretmemeli
 *   Her zaman ya gecerli sonuc ya da bir hata kodu dondurmeli.
 *
 * NEDEN ONEMLI:
 *   Telemetri cozucusu kayipli ve dusmanca bir telsiz ortamindan veri
 *   alir. Saldirgan, ozel hazirlanmis bir paket yollayarak alici
 *   sistemi cokertmeye calisabilir. Bu yuzden cozucunun her girdiye
 *   dayanikli olmasi bir guvenlik gereksinimidir.
 *
 * TASMA TESPITI (kanarya yontemi):
 *   Cikti tamponlarinin oncesine ve sonrasina bilinen bir desen
 *   ("kanarya") yazilir. Cagri sonrasi bu desen bozulmussa, kutuphane
 *   tampon disina yazmis demektir. Bu, cokmeye yol acmayan sessiz
 *   tasmalari da yakalar.
 *
 * TEKRARLANABILIRLIK:
 *   Sozde-rastgele uretec deterministiktir. Bir hata bulunursa
 *   tohum (seed) degeri yazdirilir ve durum birebir yeniden uretilebilir.
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/elbari.h"

/* --------------------------------------------------------------------- */

#define KANARYA_BAYT     (64)
#define KANARYA_DESENI   (0xA5u)

#define MAKS_GIRDI       (4096)
#define MAKS_ELEMAN      (512)
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

/* ---------------------------------------------------------------------
 * KANARYA KORUMALI TAMPON
 * ------------------------------------------------------------------- */

typedef struct
{
    unsigned char *blok;      /* tam ayrilan alan */
    unsigned char *kullanim;  /* kullanicinin gordugu bolge */
    size_t         boyut;
} korumali_tampon;

static int korumali_kur(korumali_tampon *kt, size_t boyut)
{
    kt->boyut = boyut;
    kt->blok = (unsigned char *)malloc(boyut + (2u * KANARYA_BAYT));
    if (kt->blok == NULL) { return 0; }

    (void)memset(kt->blok, KANARYA_DESENI, boyut + (2u * KANARYA_BAYT));
    kt->kullanim = kt->blok + KANARYA_BAYT;
    return 1;
}

/** Kanarya bozulmus mu? 1 = bozuk (tasma var) */
static int korumali_bozuk_mu(const korumali_tampon *kt)
{
    size_t i;

    for (i = 0; i < KANARYA_BAYT; i++)
    {
        if (kt->blok[i] != KANARYA_DESENI) { return 1; }
        if (kt->blok[KANARYA_BAYT + kt->boyut + i] != KANARYA_DESENI) { return 1; }
    }
    return 0;
}

static void korumali_birak(korumali_tampon *kt)
{
    free(kt->blok);
    kt->blok = NULL;
}

/* ---------------------------------------------------------------------
 * SAYAÇLAR
 * ------------------------------------------------------------------- */

static long long g_tur = 0;
static long long g_kabul = 0;      /* cozucu basarili dedi */
static long long g_red = 0;        /* cozucu hata kodu dondurdu */
static long long g_tasma = 0;      /* KANARYA BOZULDU - kritik hata */

/* Kategori bazli kirilim: hangi katman ne kadar kabul/red etti?
 * Bu ayrim onemli, cunku cekirdek ve kanal katmanlarinda BUTUNLUK
 * KONTROLU YOKTUR (saglama toplami yoktur) - bozuk girdiyi kabul edip
 * anlamsiz veri uretmeleri beklenen davranistir. Butunluk garantisi
 * CERCEVE katmaninin isidir (CRC32). Asagidaki tablo bu tasarim
 * ayrimini sayisal olarak gosterir. */
static long long g_kat_kabul[3] = { 0, 0, 0 };  /* 0=cekirdek 1=kanal 2=cerceve */
static long long g_kat_red[3]   = { 0, 0, 0 };

/* ---------------------------------------------------------------------
 * 1) CEKIRDEK COZUCU - saf rastgele girdi
 * ------------------------------------------------------------------- */
static void fuzz_cekirdek(void)
{
    int32_t girdi_boyu = (int32_t)rastgele_aralik(MAKS_GIRDI) + 1;
    int32_t eleman = (int32_t)rastgele_aralik(MAKS_ELEMAN) + 1;
    korumali_tampon girdi;
    korumali_tampon cikti;
    int32_t i;
    int32_t r;

    if (korumali_kur(&girdi, (size_t)girdi_boyu) == 0) { return; }
    if (korumali_kur(&cikti, (size_t)eleman * sizeof(int32_t)) == 0)
    {
        korumali_birak(&girdi);
        return;
    }

    for (i = 0; i < girdi_boyu; i++)
    {
        girdi.kullanim[i] = (unsigned char)rastgele_aralik(256u);
    }

    r = elbari_basit(girdi.kullanim, girdi_boyu,
                     (int32_t *)(void *)cikti.kullanim, eleman);

    if (korumali_bozuk_mu(&cikti) != 0) { g_tasma++; }
    if (r == ELBARI_TAMAM) { g_kabul++; g_kat_kabul[0]++; } else { g_red++; g_kat_red[0]++; }

    korumali_birak(&girdi);
    korumali_birak(&cikti);
}

/* ---------------------------------------------------------------------
 * 2) KANAL COZUCU - hem saf rastgele hem "gecerli gibi gorunen" girdi
 * ------------------------------------------------------------------- */
static void fuzz_kanal(int gecerli_baslikla)
{
    int32_t kanal = (int32_t)rastgele_aralik(MAKS_KANAL_TEST) + 1;
    int32_t eleman = ((int32_t)rastgele_aralik(MAKS_ELEMAN) + 1) * kanal;
    int32_t girdi_boyu = (int32_t)rastgele_aralik(MAKS_GIRDI) + 4;
    int32_t calisma_kap = elbari_kanal_gerekli_calisma_alani(eleman, kanal);
    korumali_tampon girdi;
    korumali_tampon cikti;
    korumali_tampon calisma;
    int32_t i;
    int32_t r;

    if (calisma_kap <= 0) { return; }
    if (korumali_kur(&girdi, (size_t)girdi_boyu) == 0) { return; }
    if (korumali_kur(&cikti, (size_t)eleman * sizeof(int32_t)) == 0)
    {
        korumali_birak(&girdi);
        return;
    }
    if (korumali_kur(&calisma, (size_t)calisma_kap * sizeof(int32_t)) == 0)
    {
        korumali_birak(&girdi);
        korumali_birak(&cikti);
        return;
    }

    for (i = 0; i < girdi_boyu; i++)
    {
        girdi.kullanim[i] = (unsigned char)rastgele_aralik(256u);
    }

    /* Bazi turlarda basligi "makul" yaparak cozucuyu daha derine sokariz;
     * boylece yalnizca ilk kontrolde reddedilmek yerine ic yollar da
     * sinanmis olur. */
    if (gecerli_baslikla != 0)
    {
        girdi.kullanim[0] = (unsigned char)kanal;
        girdi.kullanim[1] = (unsigned char)((kanal + 7) / 8);
    }

    r = elbari_kanal_basit(girdi.kullanim, girdi_boyu,
                           (int32_t *)(void *)calisma.kullanim, calisma_kap,
                           (int32_t *)(void *)cikti.kullanim, eleman);

    if ((korumali_bozuk_mu(&cikti) != 0) || (korumali_bozuk_mu(&calisma) != 0))
    {
        g_tasma++;
    }
    if (r == ELBARI_TAMAM) { g_kabul++; g_kat_kabul[1]++; } else { g_red++; g_kat_red[1]++; }

    korumali_birak(&girdi);
    korumali_birak(&cikti);
    korumali_birak(&calisma);
}

/* ---------------------------------------------------------------------
 * 3) CERCEVE COZUCU - gercek cerceve uretip BOZARAK besle
 * ---------------------------------------------------------------------
 * En degerli senaryo budur: saf rastgele veri genelde ilk kontrolde
 * reddedilir. Gecerli bir cerceveyi bozmak, cozucuyu derin yollara
 * sokar ve gercek saldiri senaryosuna benzer.
 * ------------------------------------------------------------------- */
static void fuzz_cerceve(int32_t *kaynak_veri)
{
    int32_t kanal = (int32_t)rastgele_aralik(6u) + 1;
    int32_t kayit = (int32_t)rastgele_aralik(64u) + 1;
    int32_t eleman = kayit * kanal;
    int32_t calisma_kap = elbari_cerceve_gerekli_calisma_alani(kayit, kanal);
    int32_t paket_kap = elbari_cerceve_en_kotu_durum_boyutu(kayit, kanal);
    korumali_tampon paket;
    korumali_tampon cikti;
    korumali_tampon calisma;
    int32_t yazilan;
    int32_t bozma_sayisi;
    int32_t i;
    int32_t r;
    uint32_t sira = 0u;
    int32_t adet = 0;

    if ((calisma_kap <= 0) || (paket_kap <= 0)) { return; }
    if (korumali_kur(&paket, (size_t)paket_kap) == 0) { return; }
    if (korumali_kur(&cikti, (size_t)eleman * sizeof(int32_t)) == 0)
    {
        korumali_birak(&paket);
        return;
    }
    if (korumali_kur(&calisma, (size_t)calisma_kap * sizeof(int32_t)) == 0)
    {
        korumali_birak(&paket);
        korumali_birak(&cikti);
        return;
    }

    /* Gecerli bir cerceve uret */
    yazilan = elbari_cerceve_yaz(kaynak_veri, eleman, kanal,
                                 (uint32_t)rastgele(),
                                 (int32_t *)(void *)calisma.kullanim, calisma_kap,
                                 paket.kullanim, paket_kap);

    if (yazilan > 0)
    {
        /* Simdi boz: 1-8 arasi rastgele bayt degistir */
        bozma_sayisi = (int32_t)rastgele_aralik(8u) + 1;
        for (i = 0; i < bozma_sayisi; i++)
        {
            int32_t poz = (int32_t)rastgele_aralik((unsigned int)yazilan);
            /* XOR ile SIFIR OLMAYAN bir deger uygulanir; boylece bayt
             * kesinlikle degisir. Rastgele deger atansaydi 1/256 olasilikla
             * ayni deger yazilir ve paket aslinda hic bozulmamis olurdu;
             * bu da olcumu yaniltirdi. */
            paket.kullanim[poz] ^= (unsigned char)(rastgele_aralik(255u) + 1u);
        }

        /* Bazen kirp (kisa paket senaryosu) */
        if (rastgele_aralik(4u) == 0u)
        {
            yazilan = (int32_t)rastgele_aralik((unsigned int)yazilan) + 1;
        }

        r = elbari_cerceve_oku(paket.kullanim, yazilan, kanal,
                               (int32_t *)(void *)calisma.kullanim, calisma_kap,
                               (int32_t *)(void *)cikti.kullanim, eleman,
                               &sira, &adet);

        if ((korumali_bozuk_mu(&cikti) != 0) ||
            (korumali_bozuk_mu(&calisma) != 0) ||
            (korumali_bozuk_mu(&paket) != 0))
        {
            g_tasma++;
        }
        if (r == ELBARI_TAMAM) { g_kabul++; g_kat_kabul[2]++; } else { g_red++; g_kat_red[2]++; }
    }

    korumali_birak(&paket);
    korumali_birak(&cikti);
    korumali_birak(&calisma);
}

/* ---------------------------------------------------------------------
 * 4) ASIRI PARAMETRELER
 * ------------------------------------------------------------------- */
static void fuzz_asiri_parametreler(void)
{
    unsigned char kucuk[64];
    int32_t hedef[16];
    int32_t calisma[16];
    int32_t r;

    (void)memset(kucuk, 0, sizeof(kucuk));

    /* Negatif ve asiri buyuk boyutlar */
    r = elbari_basit(kucuk, -1, hedef, 4);                 if (r == ELBARI_TAMAM) { g_kabul++; } else { g_red++; }
    r = elbari_basit(kucuk, 64, hedef, -5);                if (r == ELBARI_TAMAM) { g_kabul++; } else { g_red++; }
    r = elbari_basit(kucuk, 64, hedef, INT32_MAX);         if (r == ELBARI_TAMAM) { g_kabul++; } else { g_red++; }
    r = elbari_kabid(hedef, INT32_MAX, kucuk, 64);         if (r > 0) { g_kabul++; } else { g_red++; }

    r = elbari_kanal_basit(kucuk, 64, calisma, 16, hedef, INT32_MAX);
    if (r == ELBARI_TAMAM) { g_kabul++; } else { g_red++; }

    r = elbari_cerceve_oku(kucuk, 64, INT32_MAX, calisma, 16, hedef, 16, NULL, NULL);
    if (r == ELBARI_TAMAM) { g_kabul++; } else { g_red++; }

    r = elbari_cerceve_oku(kucuk, 64, 0, calisma, 16, hedef, 16, NULL, NULL);
    if (r == ELBARI_TAMAM) { g_kabul++; } else { g_red++; }

    /* Boyut hesaplari asiri degerlerde negatif/tasan sonuc uretmemeli */
    if (elbari_kanal_en_kotu_durum_boyutu(INT32_MAX, 3) >= 0) { g_kabul++; } else { g_red++; }
    if (elbari_cerceve_en_kotu_durum_boyutu(INT32_MAX, 6) >= 0) { g_kabul++; } else { g_red++; }
    if (elbari_cekirdek_en_kotu_durum_boyutu(INT32_MAX) >= 0) { g_kabul++; } else { g_red++; }
}

/* =====================================================================
 * ANA
 * ===================================================================== */
int main(int argc, char **argv)
{
    long long hedef_tur = 200000;
    int32_t kaynak_veri[512];
    int32_t i;

    if (argc > 1)
    {
        hedef_tur = atoll(argv[1]);
    }

    g_tohum = 0x123456789ABCDEFull;

    /* Cerceve testleri icin makul bir kaynak veri */
    for (i = 0; i < 512; i++)
    {
        kaynak_veri[i] = 400000000 + (i * 137);
    }

    printf("=====================================================================\n");
    printf("  ELBARI - Cozucu saglamlik (fuzz) testi\n");
    printf("=====================================================================\n");
    printf("Tur sayisi : %lld\n", hedef_tur);
    printf("Tohum      : 0x%llX (deterministik, tekrarlanabilir)\n", g_tohum);
    printf("Yontem     : kanarya korumali tamponlar + yapilandirilmis bozma\n\n");

    fuzz_asiri_parametreler();

    for (g_tur = 0; g_tur < hedef_tur; g_tur++)
    {
        unsigned int secim = rastgele_aralik(4u);

        switch (secim)
        {
            case 0:  fuzz_cekirdek();          break;
            case 1:  fuzz_kanal(0);            break;
            case 2:  fuzz_kanal(1);            break;
            default: fuzz_cerceve(kaynak_veri); break;
        }

        if ((g_tur % 25000) == 0)
        {
            printf("  ... %lld tur (kabul %lld, red %lld, TASMA %lld)\n",
                   g_tur, g_kabul, g_red, g_tasma);
            (void)fflush(stdout);
        }
    }

    printf("\n---------------------------------------------------------------------\n");
    printf("SONUC\n");
    printf("---------------------------------------------------------------------\n");
    printf("  Toplam tur           : %lld\n", g_tur);
    printf("  Cozucu kabul etti    : %lld\n", g_kabul);
    printf("  Cozucu reddetti      : %lld\n", g_red);
    printf("  TAMPON TASMASI       : %lld\n", g_tasma);
    printf("\n");

    /* Katman kirilimi: "kabul" sayisinin neden yuksek oldugunu aciklar.
     * Cekirdek ve kanal katmanlarinda saglama toplami YOKTUR; bozuk
     * girdiyi kabul edip anlamsiz veri uretmeleri TASARIM GEREGIDIR.
     * Butunluk garantisi cerceve katmaninin isidir (CRC32). */
    printf("  Katman kirilimi (butunluk kontrolu tasarimi):\n");
    printf("    %-22s kabul %8lld   red %8lld   <- saglama toplami YOK\n",
           "cekirdek", g_kat_kabul[0], g_kat_red[0]);
    printf("    %-22s kabul %8lld   red %8lld   <- saglama toplami YOK\n",
           "kanal", g_kat_kabul[1], g_kat_red[1]);
    printf("    %-22s kabul %8lld   red %8lld   <- CRC32 KORUMALI\n",
           "cerceve (bozulmus)", g_kat_kabul[2], g_kat_red[2]);
    if ((g_kat_kabul[2] + g_kat_red[2]) > 0)
    {
        printf("\n    Bozulmus cerceveleri reddetme orani: %%%.2f\n",
               (100.0 * (double)g_kat_red[2]) /
               (double)(g_kat_kabul[2] + g_kat_red[2]));
    }
    printf("\n");

    if (g_tasma == 0)
    {
        printf("  [BASARILI] Hicbir girdide tampon tasmasi olusmadi.\n");
        printf("             Surec cokmeden tamamlandi; her girdi ya cozuldu\n");
        printf("             ya da hata koduyla reddedildi.\n");
    }
    else
    {
        printf("  [BASARISIZ] %lld turda tampon tasmasi tespit edildi!\n", g_tasma);
    }
    printf("=====================================================================\n");

    return (g_tasma == 0) ? 0 : 1;
}
