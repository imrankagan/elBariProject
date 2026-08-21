/* =====================================================================
 * IKI KADEMELI MAVLINK VEKILI - OLCUM
 * ---------------------------------------------------------------------
 * SORU: Canli MAVLink telemetrisinde bu vekil ne kazandiriyor, ve
 *       hangi gecikme karsiliginda?
 *
 * OLCULEN:
 *   - Taban cizgi : vekil olmasaydi linkte akacak bayt/sn (ham MAVLink,
 *                   v2 kirpmasi uygulanmis)
 *   - Vekil       : canli kademe + toplu kademe bayt/sn
 *   - Kazanc      : taban / vekil
 *   - En buyuk paket: telsiz MTU'suna sigiyor mu
 *   - Tam tur     : yer istasyonu ayni mesajlari geri aliyor mu
 *
 * Calistirma:
 *   mav_olcum <gps.bin> [saniye] [--att <att.bin>] [--imu <imu.bin>]
 *
 *   --att / --imu verilirse yonelim ve IMU kanallari GERCEK ucus
 *   logundan beslenir (donustur.exe'nin urettigi alfa_att.bin /
 *   alfa_imu.bin). Verilmezse o kanallar sentetik kalir ve bu ciktida
 *   acikca yazilir.
 * ===================================================================== */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mav.h"

/* Tipik SiK / RFD900 sinifi telsizlerde kullanilabilir yuk. */
#define TELSIZ_MTU (250)

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

/**
 * Fikstur yukler ve kanal sayisini DOGRULAR.
 *
 * Yanlis kanalli bir dosyayi sessizce kabul etmek, olcumu fark
 * edilmeden bozardi (orn. att fiksturunu IMU yerine vermek).
 *
 * @param ham_cikti  serbest birakilacak ham tampon
 * @return kayit dizisine isaretci, ya da NULL
 */
static const int32_t *fikstur_yukle(const char *yol, int32_t beklenen_kanal,
                                    int32_t *kayit_cikti,
                                    unsigned char **ham_cikti)
{
    long boyut = 0;
    unsigned char *ham = dosya_oku(yol, &boyut);
    int32_t kanal = 0;
    int32_t eleman = 0;

    *ham_cikti  = NULL;
    *kayit_cikti = 0;

    if (ham == NULL)
    {
        (void)fprintf(stderr, "HATA: fikstur okunamadi: %s\n", yol);
        return NULL;
    }
    if (boyut < 8)
    {
        (void)fprintf(stderr, "HATA: fikstur cok kisa: %s\n", yol);
        free(ham);
        return NULL;
    }

    (void)memcpy(&kanal, &ham[0], 4);
    (void)memcpy(&eleman, &ham[4], 4);

    if (kanal != beklenen_kanal)
    {
        (void)fprintf(stderr,
                      "HATA: %s -> %d kanal, %d bekleniyordu\n",
                      yol, (int)kanal, (int)beklenen_kanal);
        free(ham);
        return NULL;
    }
    if ((eleman <= 0) || ((long)eleman * 4L) > (boyut - 8L))
    {
        (void)fprintf(stderr, "HATA: fikstur basligi tutmuyor: %s\n", yol);
        free(ham);
        return NULL;
    }

    *ham_cikti   = ham;
    *kayit_cikti = eleman / kanal;
    return (const int32_t *)(const void *)&ham[8];
}

/* ---------------------------------------------------------------------
 * SEMA DOGRULAMA GERI CAGRISI
 * ------------------------------------------------------------------- */

static void sema_hatasi(const char *ad, int32_t beklenen, int32_t bulunan)
{
    printf("  [SEMA HATASI] %-22s beklenen %d, bulunan %d\n",
           ad, (int)beklenen, (int)bulunan);
}

/* ---------------------------------------------------------------------
 * YUK KARSILASTIRMA
 * ---------------------------------------------------------------------
 * Kuantalama acikken ondalikli alanlar bit bit ayni gelmez; hata
 * yariml adimi asmamalidir. Diger tum alanlar TAM ayni olmalidir.
 * @return 0 uyumlu, 1 uyumsuz
 * ------------------------------------------------------------------- */

static int32_t yuk_karsilastir(const mesaj_tanimi *t,
                               const uint8_t *a, const uint8_t *b,
                               int32_t kuantala, double *en_kotu_oran)
{
    int32_t ofset = 0;
    int32_t i;

    for (i = 0; i < t->alan_sayisi; i++)
    {
        const alan_tanimi *alan = &t->alanlar[i];
        int32_t bayt = 0;
        int32_t r;

        switch (alan->tur)
        {
        case ALAN_U8:  case ALAN_I8:  bayt = 1; break;
        case ALAN_U16: case ALAN_I16: bayt = 2; break;
        case ALAN_U64:                bayt = 8; break;
        default:                      bayt = 4; break;
        }

        for (r = 0; r < alan->tekrar; r++)
        {
            if ((alan->tur == ALAN_F32) && (kuantala != 0) && (alan->olcek > 0.0f))
            {
                float fa;
                float fb;
                double fark;
                double sinir = 0.5 / (double)alan->olcek;
                double oran;

                (void)memcpy(&fa, &a[ofset], sizeof(fa));
                (void)memcpy(&fb, &b[ofset], sizeof(fb));
                fark = (double)fa - (double)fb;
                if (fark < 0.0) { fark = -fark; }

                oran = fark / sinir;
                if (oran > *en_kotu_oran) { *en_kotu_oran = oran; }

                /* Kayan nokta yuvarlamasi icin kucuk pay birakilir. */
                if (fark > (sinir * 1.001)) { return 1; }
            }
            else
            {
                if (memcmp(&a[ofset], &b[ofset], (size_t)bayt) != 0)
                {
                    return 1;
                }
            }
            ofset += bayt;
        }
    }
    return 0;
}

/* =====================================================================
 * TEK BIR AYARI OLC
 * ===================================================================== */

typedef struct
{
    double  gecikme;
    int32_t kuantala;
    double  taban_bsn;
    double  vekil_bsn;
    double  canli_bsn;
    double  toplu_bsn;
    double  kazanc;
    long    en_buyuk_paket;
    int32_t dogru;
    double  en_kotu_hata_orani;
    /* Tam tur kalirsa HANGI mesajda kaldigi: "KALDI" tek basina tanisiz. */
    const char *hatali_mesaj;
} olcum_sonucu;

static int32_t olc(const hiz_profili *profil,
                   const mav_mesaj *mesajlar, int32_t mesaj_adedi,
                   double sure, double gecikme, int32_t kuantala,
                   long taban_bayt, uint8_t *link, int32_t link_kap,
                   mav_mesaj *geri, int32_t geri_kap, int32_t kirilim,
                   olcum_sonucu *s)
{
    mav_vekil v;
    int32_t   p = 0;
    int32_t   i;
    int32_t   geri_adedi = 0;
    int32_t   okunan = 0;

    (void)memset(s, 0, sizeof(*s));
    s->gecikme  = gecikme;
    s->kuantala = kuantala;

    if (mav_vekil_kur(&v, profil, gecikme, kuantala, TELSIZ_MTU) < 0) { return -1; }

    for (i = 0; i < mesaj_adedi; i++)
    {
        int32_t n = mav_vekil_ver(&v, &mesajlar[i], &link[p], link_kap - p);
        if (n < 0) { mav_vekil_birak(&v); return -1; }
        p += n;
    }
    {
        int32_t n = mav_vekil_bosalt(&v, &link[p], link_kap - p);
        if (n < 0) { mav_vekil_birak(&v); return -1; }
        p += n;
    }

    /* --- Alici taraf: link akisini coz --- */
    while (okunan < p)
    {
        int32_t adet = 0;
        int32_t n;
        uint8_t tur = link[okunan];

        if ((geri_adedi + MAV_MAKS_KAYIT) > geri_kap) { break; }

        n = mav_vekil_coz(&v, &link[okunan], p - okunan,
                          &geri[geri_adedi], geri_kap - geri_adedi, &adet);
        if (n <= 0) { mav_vekil_birak(&v); return -1; }

        /* Seyreltme kopyalari (canli gonderilen TOPLU mesajlar) dogrulama
         * icin YOK SAYILIR: bunlar ayni verinin ikinci kopyasidir. */
        if (tur == (uint8_t)LINK_TUR_CANLI)
        {
            const mesaj_tanimi *t = mav_sema_bul(geri[geri_adedi].msgid);
            if ((t != NULL) && (t->hangi_kademe == KADEME_TOPLU))
            {
                adet = 0;
            }
        }

        geri_adedi += adet;
        okunan += n;
    }

    /* --- Dogrulama: msgid basina sirayla karsilastir --- */
    {
        int32_t sema_adedi = 0;
        const mesaj_tanimi *sema = mav_sema_tablosu(&sema_adedi);
        int32_t hata = 0;
        int32_t k;

        s->en_kotu_hata_orani = 0.0;

        for (k = 0; k < sema_adedi; k++)
        {
            int32_t ai = 0;
            int32_t bi = 0;
            int32_t eslesen = 0;

            for (;;)
            {
                while ((ai < mesaj_adedi) && (mesajlar[ai].msgid != sema[k].msgid))
                {
                    ai++;
                }
                while ((bi < geri_adedi) && (geri[bi].msgid != sema[k].msgid))
                {
                    bi++;
                }
                if ((ai >= mesaj_adedi) || (bi >= geri_adedi)) { break; }

                if (yuk_karsilastir(&sema[k], mesajlar[ai].yuk, geri[bi].yuk,
                                    kuantala, &s->en_kotu_hata_orani) != 0)
                {
                    hata++;
                    if (s->hatali_mesaj == NULL) { s->hatali_mesaj = sema[k].ad; }
                }
                eslesen++;
                ai++;
                bi++;
            }

            /* Kalan mesaj varsa kayip demektir. */
            while ((ai < mesaj_adedi))
            {
                if (mesajlar[ai].msgid == sema[k].msgid)
                {
                    hata++;
                    if (s->hatali_mesaj == NULL) { s->hatali_mesaj = sema[k].ad; }
                }
                ai++;
            }
            (void)eslesen;
        }
        s->dogru = (hata == 0) ? 1 : 0;
    }

    s->taban_bsn      = (double)taban_bayt / sure;
    s->vekil_bsn      = (double)(v.canli_bayt + v.toplu_bayt) / sure;
    s->canli_bsn      = (double)v.canli_bayt / sure;
    s->toplu_bsn      = (double)v.toplu_bayt / sure;
    s->kazanc         = s->taban_bsn / s->vekil_bsn;
    s->en_buyuk_paket = v.en_buyuk_paket;

    /* Mesaj basina kirilim: hangi mesaj kazandiriyor, hangisi ham gecise
     * dusuyor? Toplam kazanc rakami bunu gizler. */
    if (kirilim != 0)
    {
        int32_t sema_adedi = 0;
        const mesaj_tanimi *sema = mav_sema_tablosu(&sema_adedi);
        int32_t k;

        printf("\n  Mesaj basina kirilim (gecikme %.2f sn, %s):\n", gecikme,
               (kuantala != 0) ? "kuantalanmis" : "kayipsiz");
        printf("    %-22s %9s %9s %8s %7s %s\n",
               "mesaj", "ham B/sn", "link B/sn", "kazanc", "paket", "ham gecis");
        printf("    -----------------------------------------------------------"
               "-----------\n");
        for (k = 0; k < sema_adedi; k++)
        {
            double hamb = (double)v.msg_ham_bayt[k] / sure;
            double linkb = (double)v.msg_link_bayt[k] / sure;

            if (sema[k].hangi_kademe == KADEME_CANLI) { continue; }
            printf("    %-22s %9.0f %9.0f %7.2fx %7ld %ld\n",
                   sema[k].ad, hamb, linkb,
                   (linkb > 0.0) ? (hamb / linkb) : 0.0,
                   v.msg_paket[k], v.msg_geri_dusme[k]);
        }
        printf("\n");
    }

    mav_vekil_birak(&v);
    return 0;
}

/* =====================================================================
 * ANA
 * ===================================================================== */

int main(int argc, char **argv)
{
    unsigned char *ham;
    long ham_boy = 0;
    int32_t gps_kanal;
    int32_t gps_eleman;
    const int32_t *gps;
    double sure = 300.0;
    mav_uretici u;
    mav_mesaj *mesajlar;
    mav_mesaj *geri;
    int32_t mesaj_kap;
    int32_t mesaj_adedi = 0;
    long taban_bayt = 0;
    uint8_t *link;
    int32_t link_kap;
    int32_t i;
    int32_t profil_i;
    const char *att_yolu = NULL;
    const char *imu_yolu = NULL;
    unsigned char *att_ham = NULL;
    unsigned char *imu_ham = NULL;
    const int32_t *att = NULL;
    const int32_t *imu = NULL;
    int32_t att_kayit = 0;
    int32_t imu_kayit = 0;

    static const double GECIKMELER[] = { 0.25, 0.5, 1.0, 2.0, 5.0 };
    const int32_t GECIKME_ADEDI =
        (int32_t)(sizeof(GECIKMELER) / sizeof(GECIKMELER[0]));

    if (argc < 2)
    {
        (void)fprintf(stderr,
            "Kullanim: mav_olcum <gps.bin> [saniye] [--att <dosya>] [--imu <dosya>]\n"
            "  --att : 3 kanalli gercek yonelim fiksturu (alfa_att.bin)\n"
            "  --imu : 6 kanalli gercek IMU fiksturu    (alfa_imu.bin)\n"
            "Verilmeyen kanallar sentetik uretilir.\n");
        return 2;
    }
    for (i = 2; i < argc; i++)
    {
        if ((strcmp(argv[i], "--att") == 0) && ((i + 1) < argc))
        {
            att_yolu = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "--imu") == 0) && ((i + 1) < argc))
        {
            imu_yolu = argv[i + 1];
            i++;
        }
        else
        {
            sure = atof(argv[i]);
            if (sure < 1.0) { sure = 1.0; }
        }
    }

    printf("=====================================================================\n");
    printf("  IKI KADEMELI MAVLINK VEKILI - olcum\n");
    printf("=====================================================================\n\n");

    /* --- 1) Sema tutarliligi --- */
    printf("--- Sema dogrulamasi ---\n");
    if (mav_sema_dogrula(sema_hatasi) != 0)
    {
        printf("  Sema tablosu tutarsiz; olcum durduruldu.\n");
        return 1;
    }
    printf("  Tum mesajlarda alan boyutlari yuk boyutunu tutuyor.\n\n");

    /* --- 1b) CRC_EXTRA: hesaplanan degerler referansla ortusuyor mu --- */
    {
        /* Referans degerler MAVLink common dialect'inden bilinen
         * CRC_EXTRA baytlaridir. Hesaplama BAGIMSIZ bir yoldan
         * (mesaj adi + alan tanimlari) ayni sonuca variyorsa, sema
         * tablosundaki alan adlari ve turleri de dogrulanmis olur.
         *
         * !!! Bu tablo yine de KENDI dialect surumunuzle karsilastirilmali:
         *     uretilmis common.h icindeki MAVLINK_MESSAGE_CRCS listesi. */
        static const struct { uint32_t msgid; uint8_t beklenen; } REFERANS[] =
        {
            {   0,  50 }, {   1, 124 }, {  24,  24 }, {  26, 170 },
            {  30,  39 }, {  33, 104 }, {  36, 222 }, {  65, 118 },
            {  74,  20 }, { 147, 154 }, { 241,  90 }
        };
        const int32_t REF_ADEDI =
            (int32_t)(sizeof(REFERANS) / sizeof(REFERANS[0]));
        int32_t sema_adedi = 0;
        const mesaj_tanimi *sema = mav_sema_tablosu(&sema_adedi);
        int32_t uyumsuz = 0;
        int32_t k;

        printf("--- CRC_EXTRA (semadan HESAPLANDI, ezberden yazilmadi) ---\n");
        printf("  %-22s %10s %10s %s\n",
               "mesaj", "hesaplanan", "referans", "durum");
        printf("  --------------------------------------------------------\n");

        for (k = 0; k < sema_adedi; k++)
        {
            uint8_t hesap = mav_crc_extra(&sema[k]);
            int32_t j;
            int32_t bulundu = 0;
            uint8_t ref = 0u;

            for (j = 0; j < REF_ADEDI; j++)
            {
                if (REFERANS[j].msgid == sema[k].msgid)
                {
                    ref = REFERANS[j].beklenen;
                    bulundu = 1;
                    break;
                }
            }

            if (bulundu == 0)
            {
                printf("  %-22s %10u %10s %s\n",
                       sema[k].ad, (unsigned)hesap, "-", "referans yok");
            }
            else if (hesap == ref)
            {
                printf("  %-22s %10u %10u %s\n",
                       sema[k].ad, (unsigned)hesap, (unsigned)ref, "ORTUSTU");
            }
            else
            {
                printf("  %-22s %10u %10u %s\n",
                       sema[k].ad, (unsigned)hesap, (unsigned)ref,
                       "UYUMSUZ  <-- sema alan adlari/turleri hatali");
                uyumsuz++;
            }
        }

        if (uyumsuz == 0)
        {
            printf("\n  Tum degerler ortustu. Iki bagimsiz yol (alan\n");
            printf("  tanimlarindan hesap ve bilinen referans) ayni sonuca\n");
            printf("  vardigi icin sema tablosu da dogrulanmis oldu.\n");
        }
        else
        {
            printf("\n  %d UYUMSUZLUK. Sema tablosundaki alan adlari veya\n",
                   (int)uyumsuz);
            printf("  turleri kendi dialect surumunuzle ortusmuyor.\n");
        }
        printf("\n  Dogrulama: uretilmis common.h icindeki MAVLINK_MESSAGE_CRCS\n");
        printf("  listesiyle karsilastirin.\n\n");
    }

    /* --- 2) Veri --- */
    ham = dosya_oku(argv[1], &ham_boy);
    if (ham == NULL)
    {
        (void)fprintf(stderr, "HATA: veri okunamadi: %s\n", argv[1]);
        return 2;
    }
    (void)memcpy(&gps_kanal, &ham[0], 4);
    (void)memcpy(&gps_eleman, &ham[4], 4);
    gps = (const int32_t *)(const void *)&ham[8];
    if (gps_kanal != 3)
    {
        (void)fprintf(stderr, "HATA: 3 kanalli GPS verisi bekleniyordu\n");
        return 2;
    }

    if (att_yolu != NULL)
    {
        att = fikstur_yukle(att_yolu, 3, &att_kayit, &att_ham);
        if (att == NULL) { return 2; }
    }
    if (imu_yolu != NULL)
    {
        imu = fikstur_yukle(imu_yolu, 6, &imu_kayit, &imu_ham);
        if (imu == NULL) { return 2; }
    }

    printf("--- Veri kaynaklari ---\n");
    printf("  enlem/boylam        : GERCEK   %s (%d kayit)\n",
           argv[1], (int)(gps_eleman / 3));
    printf("  yonelim (ATTITUDE)  : %s\n",
           (att != NULL) ? "GERCEK" : "sentetik");
    if (att != NULL) { printf("                        %s (%d kayit)\n",
                              att_yolu, (int)att_kayit); }
    printf("  IMU (SCALED_IMU)    : %s\n",
           (imu != NULL) ? "GERCEK" : "sentetik");
    if (imu != NULL) { printf("                        %s (%d kayit)\n",
                              imu_yolu, (int)imu_kayit); }
    printf("  RC, servo, batarya, titresim, manyetometre : sentetik\n\n");

    for (profil_i = 0; profil_i < mav_hiz_profili_adedi(); profil_i++)
    {
        const hiz_profili *profil = mav_hiz_profili(profil_i);

        mesaj_adedi = 0;
        taban_bayt  = 0;

        printf("#####################################################################\n");
        printf("  HIZ PROFILI %d/%d: %s\n", (int)(profil_i + 1),
               (int)mav_hiz_profili_adedi(), profil->ad);
        printf("  %s\n", profil->aciklama);
        printf("#####################################################################\n\n");

        /* --- 3) Akisi uret --- */
        /* Kapasite profilin TOPLAM mesaj hizindan hesaplanir: SR0 profili
         * SR1'in birkac katini uretir, sabit bir tahmin tasardi. */
        {
            int32_t sa = 0;
            const mesaj_tanimi *st = mav_sema_tablosu(&sa);
            double  toplam_hz = 0.0;
            int32_t g;

            for (g = 0; g < sa; g++) { toplam_hz += mav_hiz(&st[g], profil); }
            mesaj_kap = (int32_t)(sure * toplam_hz) + 1024;
        }
        mesajlar = (mav_mesaj *)malloc((size_t)mesaj_kap * sizeof(mav_mesaj));
        geri     = (mav_mesaj *)malloc((size_t)mesaj_kap * sizeof(mav_mesaj));
        if ((mesajlar == NULL) || (geri == NULL)) { return 2; }

        mav_uretici_kur(&u, profil, gps, gps_eleman / 3);
        mav_uretici_gercek_veri(&u, att, att_kayit, imu, imu_kayit);
        while ((mesaj_adedi < mesaj_kap)
               && (mav_uretici_sonraki(&u, sure, &mesajlar[mesaj_adedi]) != 0))
        {
            uint8_t gecici[MAV_MAKS_CERCEVE];
            int32_t n = mav_cerceve_yaz(&mesajlar[mesaj_adedi], gecici,
                                        (int32_t)sizeof(gecici));
            if (n > 0) { taban_bayt += (long)n; }
            mesaj_adedi++;
        }

        printf("--- Uretilen akis ---\n");
        printf("  Sure          : %.0f saniye\n", sure);
        printf("  Mesaj         : %d\n", (int)mesaj_adedi);
        printf("  Taban cizgi   : %ld bayt  =  %.0f bayt/sn  (ham MAVLink v2)\n\n",
               taban_bayt, (double)taban_bayt / sure);

        /* --- 4) Mesaj karisimi --- */
        {
            int32_t sema_adedi = 0;
            const mesaj_tanimi *sema = mav_sema_tablosu(&sema_adedi);
            int32_t k;

            printf("--- Mesaj karisimi ve kademe atamasi ---\n");
            printf("  %-22s %6s %7s %10s %8s %s\n",
                   "mesaj", "Hz", "adet", "bayt/sn", "pay", "kademe");
            printf("  ---------------------------------------------------------------"
                   "------\n");

            for (k = 0; k < sema_adedi; k++)
            {
                long bayt = 0;
                int32_t adet = 0;

                for (i = 0; i < mesaj_adedi; i++)
                {
                    if (mesajlar[i].msgid == sema[k].msgid)
                    {
                        uint8_t gecici[MAV_MAKS_CERCEVE];
                        int32_t n = mav_cerceve_yaz(&mesajlar[i], gecici,
                                                    (int32_t)sizeof(gecici));
                        if (n > 0) { bayt += (long)n; }
                        adet++;
                    }
                }

                printf("  %-22s %6.1f %7d %10.0f %7.1f%% %s%s\n",
                       sema[k].ad, mav_hiz(&sema[k], profil), (int)adet, (double)bayt / sure,
                       (100.0 * (double)bayt) / (double)taban_bayt,
                       (sema[k].hangi_kademe == KADEME_CANLI) ? "CANLI" : "toplu",
                       (sema[k].canli_seyreltme > 0) ? " (+seyreltilmis canli)" : "");
            }
            printf("\n");
        }

        /* --- 5) Vekil olcumleri --- */
        link_kap = (int32_t)(taban_bayt * 2) + 65536;
        link = (uint8_t *)malloc((size_t)link_kap);
        if (link == NULL) { return 2; }

        printf("--- Vekil: gecikme butcesi suprumu ---\n");
        printf("  Ondalikli alanlar KUANTALANIYOR (yonelim 0.001 rad, hiz 0.01 m/sn)\n\n");
        printf("  %8s %10s %10s %10s %9s %9s %8s %s\n",
               "gecikme", "taban B/sn", "vekil B/sn", "kazanc",
               "canli", "toplu", "enb pkt", "tam tur");
        printf("  ---------------------------------------------------------------"
               "----------------\n");

        for (i = 0; i < GECIKME_ADEDI; i++)
        {
            olcum_sonucu s;
            if (olc(profil, mesajlar, mesaj_adedi, sure, GECIKMELER[i], 1,
                    taban_bayt, link, link_kap, geri, mesaj_kap,
                    0, &s) < 0)
            {
                printf("  %8.2f  <-- olcum basarisiz\n", GECIKMELER[i]);
                continue;
            }
            printf("  %7.2fs %10.0f %10.0f %9.2fx %9.0f %9.0f %8ld %s%s\n",
                   s.gecikme, s.taban_bsn, s.vekil_bsn, s.kazanc,
                   s.canli_bsn, s.toplu_bsn, s.en_buyuk_paket,
                   (s.dogru != 0) ? "GECTI" : (s.hatali_mesaj != NULL)
                                            ? s.hatali_mesaj : "KALDI",
                   (s.en_buyuk_paket > TELSIZ_MTU) ? "  <-- MTU asildi" : "");
        }

        printf("\n  Ondalikli alanlar KUANTALANMIYOR (bit bit kayipsiz)\n\n");
        printf("  %8s %10s %10s %10s %9s %9s %8s %s\n",
               "gecikme", "taban B/sn", "vekil B/sn", "kazanc",
               "canli", "toplu", "enb pkt", "tam tur");
        printf("  ---------------------------------------------------------------"
               "----------------\n");

        for (i = 0; i < GECIKME_ADEDI; i++)
        {
            olcum_sonucu s;
            if (olc(profil, mesajlar, mesaj_adedi, sure, GECIKMELER[i], 0,
                    taban_bayt, link, link_kap, geri, mesaj_kap,
                    0, &s) < 0)
            {
                printf("  %8.2f  <-- olcum basarisiz\n", GECIKMELER[i]);
                continue;
            }
            printf("  %7.2fs %10.0f %10.0f %9.2fx %9.0f %9.0f %8ld %s%s\n",
                   s.gecikme, s.taban_bsn, s.vekil_bsn, s.kazanc,
                   s.canli_bsn, s.toplu_bsn, s.en_buyuk_paket,
                   (s.dogru != 0) ? "GECTI" : (s.hatali_mesaj != NULL)
                                            ? s.hatali_mesaj : "KALDI",
                   (s.en_buyuk_paket > TELSIZ_MTU) ? "  <-- MTU asildi" : "");
        }

        /* --- 6) Tani: 2 saniyelik butcede mesaj basina kirilim --- */
        {
            olcum_sonucu s;
            printf("\n--- Tani: hangi mesaj kazandiriyor? ---");
            (void)olc(profil, mesajlar, mesaj_adedi, sure, 2.0, 1,
                      taban_bayt, link, link_kap, geri, mesaj_kap, 1, &s);
            printf("  ham gecis sutunu: sikistirma kazandirmadigi icin kayitlarin\n");
            printf("  ham gonderildigi paket sayisi. Vekil bu sayede taban\n");
            printf("  cizgisinden hicbir kosulda kotu olamaz.\n");
        }

        free(link);
        free(mesajlar);
        free(geri);
    }

    printf("\n  canli  = sifir gecikmeli ham MAVLink (kalp atisi, sistem durumu,\n");
    printf("           seyreltilmis konum/gosterge)\n");
    printf("  toplu  = biriktirilip sikistirilan yuksek hizli telemetri\n");
    printf("  enb pkt= en buyuk link paketi; telsiz MTU'su ~%d bayt varsayildi\n",
           TELSIZ_MTU);
    printf("  tam tur= yer istasyonu ayni mesajlari geri aliyor mu\n");
    printf("           (kuantalamada hata sinirlari icinde)\n");

    printf("=====================================================================\n");

    free(ham);
    free(att_ham);
    free(imu_ham);
    return 0;
}
