/* =====================================================================
 * ELBARI - C surumu dogrulama programi
 * ---------------------------------------------------------------------
 * AMAC:
 *   1) C surumu, .NET surumuyle BIT BIT AYNI ciktiyi uretiyor mu?
 *   2) C surumu kendi ciktisini kayipsiz geri acabiliyor mu?
 *   3) Kenar durumlar ve bozuk girdi dogru ele aliniyor mu?
 *
 * KULLANIM:
 *   dogrulama <referans_dizini>
 *
 * Referans dizininde .NET tarafinca uretilmis dosyalar beklenir:
 *   girdi.bin        : [int32 kanal][int32 eleman][int32 x N] ham veri
 *   ref_cekirdek.bin : .NET elbari_kabid ciktisi (tek akis)
 *   ref_kanal.bin    : .NET kanal katmani ciktisi
 *   ref_cerceve.bin  : .NET cerceve ciktilari (uzunluk onekli)
 * ===================================================================== */

/* MSVC fopen uyarisi: test kodu, tasinabilirlik icin standart fopen kullanilir */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/elbari.h"

static int g_gecen = 0;
static int g_kalan = 0;

static void sonuc_yaz(const char *ad, int basarili, const char *aciklama)
{
    if (basarili != 0)
    {
        g_gecen++;
        printf("  [GECTI] %-46s %s\n", ad, (aciklama != NULL) ? aciklama : "");
    }
    else
    {
        g_kalan++;
        printf("  [KALDI] %-46s %s\n", ad, (aciklama != NULL) ? aciklama : "");
    }
}

/* Dosyayi tamamen okur. Cagiran free() ile birakmalidir. */
static unsigned char *dosya_oku(const char *yol, long *boyut_cikti)
{
    FILE *f = fopen(yol, "rb");
    unsigned char *tampon;
    long boyut;
    size_t okunan;

    if (f == NULL)
    {
        return NULL;
    }

    (void)fseek(f, 0, SEEK_END);
    boyut = ftell(f);
    (void)fseek(f, 0, SEEK_SET);

    if (boyut <= 0)
    {
        (void)fclose(f);
        return NULL;
    }

    tampon = (unsigned char *)malloc((size_t)boyut);
    if (tampon == NULL)
    {
        (void)fclose(f);
        return NULL;
    }

    okunan = fread(tampon, 1, (size_t)boyut, f);
    (void)fclose(f);

    if (okunan != (size_t)boyut)
    {
        free(tampon);
        return NULL;
    }

    *boyut_cikti = boyut;
    return tampon;
}

static void yol_birlestir(char *hedef, size_t hedef_boyutu,
                          const char *dizin, const char *ad)
{
    (void)snprintf(hedef, hedef_boyutu, "%s/%s", dizin, ad);
}

/* Iki tamponu karsilastirir, ilk farkin konumunu bildirir. */
static int tampon_karsilastir(const unsigned char *a, int32_t a_boy,
                              const unsigned char *b, int32_t b_boy,
                              char *mesaj, size_t mesaj_boyutu)
{
    int32_t i;

    if (a_boy != b_boy)
    {
        (void)snprintf(mesaj, mesaj_boyutu,
                       "boyut farkli: C=%d, .NET=%d", (int)a_boy, (int)b_boy);
        return 0;
    }

    for (i = 0; i < a_boy; i++)
    {
        if (a[i] != b[i])
        {
            (void)snprintf(mesaj, mesaj_boyutu,
                           "bayt %d farkli: C=0x%02X, .NET=0x%02X",
                           (int)i, (unsigned)a[i], (unsigned)b[i]);
            return 0;
        }
    }

    (void)snprintf(mesaj, mesaj_boyutu, "%d bayt birebir ayni", (int)a_boy);
    return 1;
}

int main(int argc, char **argv)
{
    char yol[1024];
    const char *dizin;
    unsigned char *girdi_ham = NULL;
    long girdi_boy = 0;
    int32_t kanal_sayisi;
    int32_t eleman_sayisi;
    int32_t *veri = NULL;
    int32_t *calisma = NULL;
    int32_t *geri = NULL;
    unsigned char *cikti = NULL;
    unsigned char *ref = NULL;
    long ref_boy = 0;
    char mesaj[256];
    int32_t n;
    int32_t i;
    int32_t durum;

    if (argc < 2)
    {
        (void)fprintf(stderr, "Kullanim: dogrulama <referans_dizini>\n");
        return 2;
    }
    dizin = argv[1];

    printf("=====================================================================\n");
    printf("  ELBARI - C surumu ile .NET surumu ikili uyumluluk dogrulamasi\n");
    printf("=====================================================================\n\n");

    /* ---- Girdi verisini yukle ---- */
    yol_birlestir(yol, sizeof(yol), dizin, "girdi.bin");
    girdi_ham = dosya_oku(yol, &girdi_boy);
    if (girdi_ham == NULL)
    {
        (void)fprintf(stderr, "HATA: girdi.bin okunamadi: %s\n", yol);
        return 2;
    }

    (void)memcpy(&kanal_sayisi, &girdi_ham[0], 4);
    (void)memcpy(&eleman_sayisi, &girdi_ham[4], 4);

    veri = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
    geri = (int32_t *)malloc((size_t)eleman_sayisi * sizeof(int32_t));
    if ((veri == NULL) || (geri == NULL))
    {
        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
        return 2;
    }
    (void)memcpy(veri, &girdi_ham[8], (size_t)eleman_sayisi * sizeof(int32_t));

    printf("Veri: %d eleman, %d kanal (%d kayit), %d bayt ham\n\n",
           (int)eleman_sayisi, (int)kanal_sayisi,
           (int)(eleman_sayisi / kanal_sayisi),
           (int)(eleman_sayisi * 4));

    /* =================================================================
     * 1) CEKIRDEK KATMAN - ikili uyumluluk
     * ================================================================= */
    printf("--- 1) Cekirdek katman (tek akis) ---\n");

    n = elbari_cekirdek_en_kotu_durum_boyutu(eleman_sayisi);
    cikti = (unsigned char *)malloc((size_t)n);
    if (cikti == NULL)
    {
        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
        return 2;
    }

    {
        int32_t c_sonuc = elbari_kabid(veri, eleman_sayisi, cikti, n);

        yol_birlestir(yol, sizeof(yol), dizin, "ref_cekirdek.bin");
        ref = dosya_oku(yol, &ref_boy);

        if (ref == NULL)
        {
            /* .NET tarafi da reddettiyse referans dosyasi bos/yok olur */
            sonuc_yaz("cekirdek: .NET de reddetti mi",
                      (c_sonuc == ELBARI_SIKISTIRILAMAZ) ? 1 : 0,
                      (c_sonuc == ELBARI_SIKISTIRILAMAZ)
                          ? "her iki surum de SIKISTIRILAMAZ dedi"
                          : "C kabul etti ama .NET reddetmis");
        }
        else
        {
            int esit = tampon_karsilastir(cikti, (c_sonuc > 0) ? c_sonuc : 0,
                                          ref, (int32_t)ref_boy,
                                          mesaj, sizeof(mesaj));
            sonuc_yaz("cekirdek: C ciktisi == .NET ciktisi", esit, mesaj);

            if ((esit != 0) && (c_sonuc > 0))
            {
                durum = elbari_basit(cikti, c_sonuc, geri, eleman_sayisi);
                if (durum == ELBARI_TAMAM)
                {
                    int ayni = 1;
                    for (i = 0; i < eleman_sayisi; i++)
                    {
                        if (veri[i] != geri[i]) { ayni = 0; break; }
                    }
                    sonuc_yaz("cekirdek: C round-trip kayipsiz", ayni,
                              (ayni != 0) ? "tum elemanlar birebir geri geldi"
                                          : "veri bozuldu");
                }
                else
                {
                    sonuc_yaz("cekirdek: C round-trip kayipsiz", 0, "cozme hatasi");
                }
            }
            free(ref);
            ref = NULL;
        }
    }
    free(cikti);
    cikti = NULL;

    /* =================================================================
     * 2) KANAL KATMANI - ikili uyumluluk
     * ================================================================= */
    printf("\n--- 2) Kanal katmani (cok kanalli) ---\n");

    n = elbari_kanal_en_kotu_durum_boyutu(eleman_sayisi, kanal_sayisi);
    cikti = (unsigned char *)malloc((size_t)n);
    calisma = (int32_t *)malloc(
        (size_t)elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi)
        * sizeof(int32_t));
    if ((cikti == NULL) || (calisma == NULL))
    {
        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
        return 2;
    }

    {
        int32_t c_sonuc = elbari_kanal_kabid(
            veri, eleman_sayisi, kanal_sayisi,
            calisma, elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi),
            cikti, n);

        yol_birlestir(yol, sizeof(yol), dizin, "ref_kanal.bin");
        ref = dosya_oku(yol, &ref_boy);

        if ((ref == NULL) || (c_sonuc < 0))
        {
            sonuc_yaz("kanal: C ciktisi == .NET ciktisi", 0,
                      "referans dosyasi yok ya da C hata dondu");
        }
        else
        {
            int esit = tampon_karsilastir(cikti, c_sonuc, ref, (int32_t)ref_boy,
                                          mesaj, sizeof(mesaj));
            sonuc_yaz("kanal: C ciktisi == .NET ciktisi", esit, mesaj);

            durum = elbari_kanal_basit(
                cikti, c_sonuc, calisma,
                elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi),
                geri, eleman_sayisi);

            if (durum == ELBARI_TAMAM)
            {
                int ayni = 1;
                for (i = 0; i < eleman_sayisi; i++)
                {
                    if (veri[i] != geri[i]) { ayni = 0; break; }
                }
                sonuc_yaz("kanal: C round-trip kayipsiz", ayni,
                          (ayni != 0) ? "tum elemanlar birebir geri geldi"
                                      : "veri bozuldu");
            }
            else
            {
                sonuc_yaz("kanal: C round-trip kayipsiz", 0, "cozme hatasi");
            }

            /* .NET ciktisini C ile cozebiliyor muyuz? (capraz uyumluluk) */
            durum = elbari_kanal_basit(
                ref, (int32_t)ref_boy, calisma,
                elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi),
                geri, eleman_sayisi);

            if (durum == ELBARI_TAMAM)
            {
                int ayni = 1;
                for (i = 0; i < eleman_sayisi; i++)
                {
                    if (veri[i] != geri[i]) { ayni = 0; break; }
                }
                sonuc_yaz("kanal: C, .NET ciktisini cozebiliyor", ayni,
                          (ayni != 0) ? "capraz uyumluluk dogrulandi"
                                      : "capraz cozmede veri bozuldu");
            }
            else
            {
                sonuc_yaz("kanal: C, .NET ciktisini cozebiliyor", 0, "cozme hatasi");
            }

            free(ref);
            ref = NULL;
        }
    }

    /* =================================================================
     * 3) CERCEVE KATMANI
     * ================================================================= */
    printf("\n--- 3) Cerceve katmani (paket kaybina dayanikli) ---\n");

    {
        const int32_t kpc = 100; /* cerceve basina kayit */
        int32_t kayit_sayisi = eleman_sayisi / kanal_sayisi;
        int32_t cerceve_kapasitesi = elbari_cerceve_en_kotu_durum_boyutu(kpc, kanal_sayisi);
        int32_t calisma_kap = elbari_cerceve_gerekli_calisma_alani(kpc, kanal_sayisi);
        unsigned char *paket = (unsigned char *)malloc((size_t)cerceve_kapasitesi);
        int32_t *cerceve_calisma = (int32_t *)malloc((size_t)calisma_kap * sizeof(int32_t));
        int32_t *cerceve_cikti = (int32_t *)malloc((size_t)(kpc * kanal_sayisi) * sizeof(int32_t));
        int32_t k;
        uint32_t sira = 0u;
        int tum_dogru = 1;
        int32_t toplam_bayt = 0;
        int32_t cerceve_sayisi = 0;

        if ((paket == NULL) || (cerceve_calisma == NULL) || (cerceve_cikti == NULL))
        {
            (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
            return 2;
        }

        for (k = 0; k < kayit_sayisi; k += kpc)
        {
            int32_t adet = ((kayit_sayisi - k) < kpc) ? (kayit_sayisi - k) : kpc;
            int32_t yazilan = elbari_cerceve_yaz(
                &veri[k * kanal_sayisi], adet * kanal_sayisi, kanal_sayisi,
                sira, cerceve_calisma, calisma_kap, paket, cerceve_kapasitesi);

            if (yazilan < 0)
            {
                tum_dogru = 0;
                break;
            }
            toplam_bayt += yazilan;
            cerceve_sayisi++;

            /* Hemen bagimsiz olarak coz ve dogrula */
            {
                uint32_t okunan_sira = 0u;
                int32_t okunan_adet = 0;

                durum = elbari_cerceve_oku(paket, yazilan, kanal_sayisi,
                                           cerceve_calisma, calisma_kap,
                                           cerceve_cikti, kpc * kanal_sayisi,
                                           &okunan_sira, &okunan_adet);
                if ((durum != ELBARI_TAMAM) || (okunan_sira != sira) ||
                    (okunan_adet != adet))
                {
                    tum_dogru = 0;
                    break;
                }
                for (i = 0; i < (adet * kanal_sayisi); i++)
                {
                    if (veri[(k * kanal_sayisi) + i] != cerceve_cikti[i])
                    {
                        tum_dogru = 0;
                        break;
                    }
                }
                if (tum_dogru == 0) { break; }
            }
            sira++;
        }

        (void)snprintf(mesaj, sizeof(mesaj),
                       "%d cerceve, %d bayt, oran %.2fx",
                       (int)cerceve_sayisi, (int)toplam_bayt,
                       (toplam_bayt > 0)
                           ? ((double)(eleman_sayisi * 4) / (double)toplam_bayt)
                           : 0.0);
        sonuc_yaz("cerceve: yaz/oku bagimsiz ve kayipsiz", tum_dogru, mesaj);

        /* --- Bozulma tespiti: her pakete tek bit bozulma enjekte et --- */
        {
            int yakalanan = 0;
            int denenen = 0;
            unsigned int tohum = 12345u;

            for (k = 0; k < kayit_sayisi; k += kpc)
            {
                int32_t adet = ((kayit_sayisi - k) < kpc) ? (kayit_sayisi - k) : kpc;
                int32_t yazilan = elbari_cerceve_yaz(
                    &veri[k * kanal_sayisi], adet * kanal_sayisi, kanal_sayisi,
                    0u, cerceve_calisma, calisma_kap, paket, cerceve_kapasitesi);

                if (yazilan <= 0) { break; }

                /* Basit dogrusal uretecle deterministik konum sec */
                tohum = (tohum * 1103515245u) + 12345u;
                {
                    int32_t poz = (int32_t)((tohum >> 16) % (unsigned int)yazilan);
                    int32_t bit = (int32_t)((tohum >> 8) % 8u);
                    paket[poz] ^= (unsigned char)(1u << (unsigned int)bit);
                }

                denenen++;
                if (elbari_cerceve_gecerli_mi(paket, yazilan) == 0)
                {
                    yakalanan++;
                }
            }

            (void)snprintf(mesaj, sizeof(mesaj), "%d/%d bozulma yakalandi",
                           yakalanan, denenen);
            sonuc_yaz("cerceve: tek-bit bozulma CRC ile yakalandi",
                      (denenen > 0) && (yakalanan == denenen), mesaj);
        }

        free(paket);
        free(cerceve_calisma);
        free(cerceve_cikti);
    }

    /* =================================================================
     * 4) FLOAT KUANTALAMA - ikili uyumluluk
     * =================================================================
     * Kayan nokta yuvarlamasi iki dilde kolayca ayrisir. Bu bolum, C ve
     * .NET surumlerinin AYNI tamsayilari uretip uretmedigini dogrular.
     * ================================================================= */
    printf("\n--- 4) Float kuantalama (ikili uyumluluk) ---\n");

    {
        unsigned char *fg = NULL;
        long fg_boy = 0;

        yol_birlestir(yol, sizeof(yol), dizin, "float_girdi.bin");
        fg = dosya_oku(yol, &fg_boy);

        if (fg == NULL)
        {
            printf("  (atlandi: float_girdi.bin bulunamadi)\n");
        }
        else
        {
            int32_t f_kanal;
            int32_t f_adet;
            float  *olcekler;
            float  *degerler;
            int32_t *kuantalanmis;
            unsigned char *ref_tam = NULL;
            long ref_tam_boy = 0;
            int32_t durum2;

            (void)memcpy(&f_kanal, &fg[0], 4);
            (void)memcpy(&f_adet, &fg[4], 4);

            olcekler = (float *)malloc((size_t)f_kanal * sizeof(float));
            degerler = (float *)malloc((size_t)f_adet * sizeof(float));
            kuantalanmis = (int32_t *)malloc((size_t)f_adet * sizeof(int32_t));

            if ((olcekler == NULL) || (degerler == NULL) || (kuantalanmis == NULL))
            {
                (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
                return 2;
            }

            (void)memcpy(olcekler, &fg[8], (size_t)f_kanal * sizeof(float));
            (void)memcpy(degerler, &fg[8 + (f_kanal * 4)], (size_t)f_adet * sizeof(float));

            printf("  veri: %d float, %d kanal\n", (int)f_adet, (int)f_kanal);

            durum2 = elbari_float_kuantala_kanalli(degerler, f_adet, f_kanal,
                                                   olcekler, kuantalanmis);
            if (durum2 != ELBARI_TAMAM)
            {
                sonuc_yaz("float: kuantalama", 0, "hata kodu dondu");
            }
            else
            {
                /* .NET'in urettigi tamsayilarla birebir ayni mi? */
                yol_birlestir(yol, sizeof(yol), dizin, "float_ref_tamsayi.bin");
                ref_tam = dosya_oku(yol, &ref_tam_boy);

                if ((ref_tam == NULL) || (ref_tam_boy != (long)f_adet * 4))
                {
                    sonuc_yaz("float: C kuantalamasi == .NET", 0, "referans dosyasi yok/bozuk");
                }
                else
                {
                    int ayni = 1;
                    for (i = 0; i < f_adet; i++)
                    {
                        int32_t beklenen_deger;
                        (void)memcpy(&beklenen_deger, &ref_tam[i * 4], 4);
                        if (kuantalanmis[i] != beklenen_deger)
                        {
                            (void)snprintf(mesaj, sizeof(mesaj),
                                           "eleman %d: C=%d, .NET=%d",
                                           (int)i, (int)kuantalanmis[i], (int)beklenen_deger);
                            ayni = 0;
                            break;
                        }
                    }
                    if (ayni != 0)
                    {
                        (void)snprintf(mesaj, sizeof(mesaj),
                                       "%d degerin tamami ayni yuvarlandi", (int)f_adet);
                    }
                    sonuc_yaz("float: C kuantalamasi == .NET", ayni, mesaj);
                    free(ref_tam);
                    ref_tam = NULL;
                }

                /* Kuantalanmis veriyi sikistirip .NET ciktisiyla karsilastir */
                {
                    int32_t kap = elbari_kanal_en_kotu_durum_boyutu(f_adet, f_kanal);
                    int32_t cal_kap = elbari_kanal_gerekli_calisma_alani(f_adet, f_kanal);
                    unsigned char *fc = (unsigned char *)malloc((size_t)kap);
                    int32_t *fcal = (int32_t *)malloc((size_t)cal_kap * sizeof(int32_t));
                    int32_t fn;

                    if ((fc == NULL) || (fcal == NULL))
                    {
                        (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
                        return 2;
                    }

                    fn = elbari_kanal_kabid(kuantalanmis, f_adet, f_kanal,
                                            fcal, cal_kap, fc, kap);

                    yol_birlestir(yol, sizeof(yol), dizin, "float_ref_kanal.bin");
                    ref = dosya_oku(yol, &ref_boy);

                    if ((ref != NULL) && (fn > 0))
                    {
                        int esit = tampon_karsilastir(fc, fn, ref, (int32_t)ref_boy,
                                                      mesaj, sizeof(mesaj));
                        sonuc_yaz("float: sikistirma C == .NET", esit, mesaj);
                        free(ref);
                        ref = NULL;
                    }
                    else
                    {
                        sonuc_yaz("float: sikistirma C == .NET", 0, "referans yok ya da hata");
                    }

                    /* Tam tur: coz -> kuantalamadan geri don -> hata olc */
                    {
                        int32_t *gt = (int32_t *)malloc((size_t)f_adet * sizeof(int32_t));
                        float *gf = (float *)malloc((size_t)f_adet * sizeof(float));
                        int32_t *fcal2 = (int32_t *)malloc((size_t)cal_kap * sizeof(int32_t));

                        if ((gt == NULL) || (gf == NULL) || (fcal2 == NULL))
                        {
                            (void)fprintf(stderr, "HATA: bellek ayrilamadi\n");
                            return 2;
                        }

                        durum2 = elbari_kanal_basit(fc, fn, fcal2, cal_kap, gt, f_adet);
                        if (durum2 == ELBARI_TAMAM)
                        {
                            durum2 = elbari_float_coz_kanalli(gt, f_adet, f_kanal, olcekler, gf);
                        }

                        if (durum2 == ELBARI_TAMAM)
                        {
                            float hata = elbari_float_maks_hata(degerler, gf, f_adet);
                            float en_kaba = 0.0f;
                            int32_t k;

                            /* Beklenen sinir: en kaba kanalin yarim adimi */
                            for (k = 0; k < f_kanal; k++)
                            {
                                float adim = 1.0f / olcekler[k];
                                if (adim > en_kaba) { en_kaba = adim; }
                            }

                            (void)snprintf(mesaj, sizeof(mesaj),
                                           "maks hata %.3e, sinir %.3e",
                                           (double)hata, (double)(en_kaba * 0.5f * 1.001f));
                            sonuc_yaz("float: tam tur hata siniri icinde",
                                      (hata <= (en_kaba * 0.5f * 1.001f)) ? 1 : 0, mesaj);
                        }
                        else
                        {
                            sonuc_yaz("float: tam tur hata siniri icinde", 0, "cozme hatasi");
                        }

                        free(gt);
                        free(gf);
                        free(fcal2);
                    }

                    free(fc);
                    free(fcal);
                }
            }

            free(olcekler);
            free(degerler);
            free(kuantalanmis);
            free(fg);
        }
    }

    /* =================================================================
     * 5) KENAR DURUMLAR VE SAVUNMACILIK
     * ================================================================= */
    printf("\n--- 5) Kenar durumlar ve savunmacilik ---\n");

    {
        int32_t kucuk[3];
        unsigned char kucuk_cikti[128];
        int32_t r;

        kucuk[0] = 1000000;
        kucuk[1] = 1000001;
        kucuk[2] = 1000002;

        r = elbari_kabid(kucuk, 3, kucuk_cikti, (int32_t)sizeof(kucuk_cikti));
        sonuc_yaz("kenar: 3 elemanli dizi", (r > 0) ? 1 : 0,
                  (r > 0) ? "sikistirildi" : "reddedildi");

        /* NULL isaretci savunmasi */
        r = elbari_kabid(NULL, 10, kucuk_cikti, (int32_t)sizeof(kucuk_cikti));
        sonuc_yaz("kenar: NULL girdi reddedildi",
                  (r == ELBARI_HATA_PARAMETRE) ? 1 : 0, "cokme yok");

        /* Cok kucuk tampon savunmasi */
        r = elbari_kabid(kucuk, 3, kucuk_cikti, 2);
        sonuc_yaz("kenar: yetersiz tampon reddedildi",
                  (r == ELBARI_HATA_TAMPON_KUCUK) ? 1 : 0, "cokme yok");

        /* Bozuk cerceve savunmasi */
        {
            unsigned char sahte[64];
            int32_t j;
            for (j = 0; j < (int32_t)sizeof(sahte); j++)
            {
                sahte[j] = (unsigned char)(j * 7);
            }
            r = elbari_cerceve_gecerli_mi(sahte, (int32_t)sizeof(sahte));
            sonuc_yaz("kenar: rastgele bayt cerceve degil",
                      (r == 0) ? 1 : 0, "sihirli sayi/CRC tuttu");
        }
    }

    /* ---- Ozet ---- */
    printf("\n=====================================================================\n");
    printf("  SONUC: %d gecti, %d kaldi\n", g_gecen, g_kalan);
    printf("=====================================================================\n");

    free(veri);
    free(geri);
    free(calisma);
    free(cikti);
    free(girdi_ham);

    return (g_kalan == 0) ? 0 : 1;
}
