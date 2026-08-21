/* =====================================================================
 * SENTETIK MAVLINK AKISI URETECI
 * ---------------------------------------------------------------------
 * !!! DURUSTLUK NOTU - BU VERININ NESI GERCEK, NESI DEGIL !!!
 *
 *   GERCEK : enlem / boylam, verilen GPS fiksturunden gelir (OSM izleri
 *            ya da ArduPilot logundan uretilmis alfa_gps.bin). Gercek
 *            olcum gurultusu ve duzensiz orneklemesiyle birlikte.
 *
 *            YONELIM ve IMU de mav_uretici_gercek_veri ile gercek ucus
 *            logundan beslenebilir (alfa_att.bin / alfa_imu.bin).
 *            Verilmezse asagidaki sentetik modele duserler.
 *
 *   SENTETIK: RC, servo, batarya, titresim - ve att/imu fiksturu
 *            verilmediyse yonelim ile IMU. Bunlar makul bir ucus
 *            modelinden uretilir.
 *
 *   Sentetik veri sikistirma olcumlerini KOLAYCA YANILTIR: fazla duzgun
 *   bir sinyal gercekci olmayan yuksek oranlar verir. Bu yuzden burada
 *   gurultu BILEREK yuksek tutulmustur (IMU'ya +-30 sayim, yonelime
 *   ~0.01 rad). Yani sonuclar iyimser degil, KOTUMSER tarafa egiktir.
 *
 *   Yine de gercek bir ucus logu (PX4 ULog / ArduPilot .bin) ile
 *   tekrarlanmadan bu sayilar teze konmamalidir.
 * ---------------------------------------------------------------------
 * Matematik kutuphanesine bagimlilik yoktur: sinus, kucuk bir tablodan
 * dogrusal aradegerleme ile hesaplanir (-lm gerekmez).
 * ===================================================================== */

#include <string.h>
#include "mav.h"

/* ---------------------------------------------------------------------
 * SINUS TABLOSU (ceyrek dalga, 0..pi/2, 17 nokta)
 * ------------------------------------------------------------------- */

static const double SIN_TABLO[17] =
{
    0.000000, 0.098017, 0.195090, 0.290285, 0.382683, 0.471397,
    0.555570, 0.634393, 0.707107, 0.773010, 0.831470, 0.881921,
    0.923880, 0.956940, 0.980785, 0.995185, 1.000000
};

static double sinus(double x)
{
    const double IKI_PI = 6.283185307179586;
    double u;
    double dilim;
    int32_t i;
    double kesir;
    int32_t isaret = 1;

    /* [0, 2pi) araligina indir */
    u = x - (IKI_PI * (double)(int32_t)(x / IKI_PI));
    if (u < 0.0) { u += IKI_PI; }

    if (u >= 3.141592653589793)
    {
        u -= 3.141592653589793;
        isaret = -1;
    }
    if (u > 1.5707963267948966)
    {
        u = 3.141592653589793 - u;
    }

    dilim = (u / 1.5707963267948966) * 16.0;
    i = (int32_t)dilim;
    if (i > 15) { i = 15; }
    kesir = dilim - (double)i;

    return (double)isaret
           * (SIN_TABLO[i] + ((SIN_TABLO[i + 1] - SIN_TABLO[i]) * kesir));
}

static double kosinus(double x)
{
    return sinus(x + 1.5707963267948966);
}

/* ---------------------------------------------------------------------
 * RASTGELE
 * ------------------------------------------------------------------- */

static uint32_t rast(mav_uretici *u)
{
    u->rastgele = (u->rastgele * 1664525u) + 1013904223u;
    return u->rastgele;
}

/** Gercek log degerini SCALED_IMU'nun int16 alanina sigdirir. */
static int16_t i16_kirp(int32_t v)
{
    if (v >  32767) { return  32767; }
    if (v < -32768) { return -32768; }
    return (int16_t)v;
}

/** [-aralik, +aralik] araliginda tamsayi gurultu. */
static int32_t gurultu(mav_uretici *u, int32_t aralik)
{
    if (aralik <= 0) { return 0; }
    return (int32_t)(rast(u) % (uint32_t)((2 * aralik) + 1)) - aralik;
}

/* ---------------------------------------------------------------------
 * YUK YAZMA YARDIMCILARI (little-endian)
 * ------------------------------------------------------------------- */

static void y8(uint8_t *p, int32_t o, uint32_t v)
{
    p[o] = (uint8_t)(v & 0xFFu);
}

static void y16(uint8_t *p, int32_t o, uint32_t v)
{
    p[o]     = (uint8_t)(v & 0xFFu);
    p[o + 1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void y32(uint8_t *p, int32_t o, uint32_t v)
{
    p[o]     = (uint8_t)(v & 0xFFu);
    p[o + 1] = (uint8_t)((v >> 8) & 0xFFu);
    p[o + 2] = (uint8_t)((v >> 16) & 0xFFu);
    p[o + 3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void y64(uint8_t *p, int32_t o, uint64_t v)
{
    y32(p, o,     (uint32_t)(v & 0xFFFFFFFFu));
    y32(p, o + 4, (uint32_t)((v >> 32) & 0xFFFFFFFFu));
}

static void yf(uint8_t *p, int32_t o, double deger)
{
    float f = (float)deger;
    uint32_t ham;
    (void)memcpy(&ham, &f, sizeof(ham));
    y32(p, o, ham);
}

/* ---------------------------------------------------------------------
 * KURULUM
 * ------------------------------------------------------------------- */

void mav_uretici_kur(mav_uretici *u, const hiz_profili *profil,
                     const int32_t *gps, int32_t gps_kayit)
{
    int32_t adet = 0;
    const mesaj_tanimi *sema = mav_sema_tablosu(&adet);
    int32_t i;

    (void)memset(u, 0, sizeof(*u));
    u->profil     = profil;
    u->gps        = gps;
    u->gps_kayit  = gps_kayit;
    u->gps_indeks = 0;
    u->t          = 0.0;
    u->rastgele   = 0xC0FFEEu;
    u->sira       = 0u;

    for (i = 0; i < adet; i++)
    {
        /* Mesajlar ayni anda baslamasin: faz kaydirmasi gercekci bir
         * akis deseni verir (hepsi ayni tik'ta gelmez). */
        /* Bu profilde hic yayinlanmayan mesaj (0 Hz) ufkun otesine
         * atilir; yoksa t=0'da bir kez uretilirdi. */
        u->sonraki[i] = (mav_hiz(&sema[i], profil) > 0.0)
                        ? ((double)i * 0.017)
                        : 1.0e30;
    }
}

void mav_uretici_gercek_veri(mav_uretici *u,
                             const int32_t *att, int32_t att_kayit,
                             const int32_t *imu, int32_t imu_kayit)
{
    if (u == NULL) { return; }

    /* Bos fikstur sessizce "gercek" sayilmamali: kayit yoksa NULL kalir
     * ve mesaj sentetige duser. */
    u->att        = ((att != NULL) && (att_kayit > 0)) ? att : NULL;
    u->att_kayit  = (u->att != NULL) ? att_kayit : 0;
    u->att_indeks = 0;

    u->imu        = ((imu != NULL) && (imu_kayit > 0)) ? imu : NULL;
    u->imu_kayit  = (u->imu != NULL) ? imu_kayit : 0;
    u->imu_indeks = 0;
}

/* ---------------------------------------------------------------------
 * MESAJ URETIMI
 * ------------------------------------------------------------------- */

static void yuk_uret(mav_uretici *u, const mesaj_tanimi *t, mav_mesaj *m)
{
    double  zaman = u->t;
    int32_t lat;
    int32_t lon;
    uint32_t ms = (uint32_t)(zaman * 1000.0);
    uint64_t us = (uint64_t)(zaman * 1000000.0);

    (void)memset(m->yuk, 0, (size_t)t->yuk_boyutu);
    m->msgid      = t->msgid;
    m->yuk_boyutu = t->yuk_boyutu;
    m->sistem     = 1u;
    m->bilesen    = 1u;
    m->sira       = u->sira;
    u->sira++;

    /* GERCEK GPS: 3 kanal (enlem, boylam, zaman) */
    lat = u->gps[(u->gps_indeks * 3) + 0];
    lon = u->gps[(u->gps_indeks * 3) + 1];

    switch (t->msgid)
    {
    case 0:   /* HEARTBEAT */
        y32(m->yuk, 0, 3u);       /* custom_mode: AUTO      */
        y8(m->yuk, 4, 2u);        /* type: QUADROTOR        */
        y8(m->yuk, 5, 3u);        /* autopilot: ARDUPILOT   */
        y8(m->yuk, 6, 217u);      /* base_mode              */
        y8(m->yuk, 7, 4u);        /* system_status: ACTIVE  */
        y8(m->yuk, 8, 3u);        /* mavlink_version        */
        break;

    case 1:   /* SYS_STATUS */
        y32(m->yuk, 0, 0x0020FFFFu);
        y32(m->yuk, 4, 0x0020FFFFu);
        y32(m->yuk, 8, 0x0020FFFFu);
        y16(m->yuk, 12, (uint32_t)(320 + gurultu(u, 25)));       /* load     */
        y16(m->yuk, 14, (uint32_t)(16800 - (int32_t)(zaman * 1.2)
                                   + gurultu(u, 30)));           /* voltaj mV */
        y16(m->yuk, 16, (uint32_t)(1500 + gurultu(u, 200)));     /* akim     */
        y16(m->yuk, 18, 0u);
        y16(m->yuk, 20, 0u);
        y8(m->yuk, 30, (uint32_t)(95 - (int32_t)(zaman / 60.0)));
        break;

    case 24:  /* GPS_RAW_INT */
        y64(m->yuk, 0, us);
        y32(m->yuk, 8,  (uint32_t)lat);
        y32(m->yuk, 12, (uint32_t)lon);
        y32(m->yuk, 16, (uint32_t)(120000 + (int32_t)(2000.0 * sinus(zaman * 0.05))
                                   + gurultu(u, 150)));
        y16(m->yuk, 20, (uint32_t)(120 + gurultu(u, 15)));   /* eph */
        y16(m->yuk, 22, (uint32_t)(180 + gurultu(u, 20)));   /* epv */
        y16(m->yuk, 24, (uint32_t)(1250 + gurultu(u, 60)));  /* vel */
        y16(m->yuk, 26, (uint32_t)(((int32_t)(zaman * 30.0) % 36000)));
        y8(m->yuk, 28, 3u);                                  /* fix_type 3D */
        y8(m->yuk, 29, (uint32_t)(14 + gurultu(u, 1)));
        u->gps_indeks = (u->gps_indeks + 1) % u->gps_kayit;
        break;

    case 26:  /* SCALED_IMU */
        y32(m->yuk, 0, ms);
        if (u->imu != NULL)
        {
            /* GERCEK IMU. Fikstur sirasi GyrXYZ sonra AccXYZ; SCALED_IMU
             * once ivmeyi bekler. Birimler zaten ortusuyor (mg ve
             * mrad/sn) - bu yuzden donusum yok, yalnizca yeniden
             * siralama. */
            const int32_t *d = &u->imu[u->imu_indeks * 6];

            y16(m->yuk, 4,  (uint32_t)(int32_t)i16_kirp(d[3]));  /* xacc  */
            y16(m->yuk, 6,  (uint32_t)(int32_t)i16_kirp(d[4]));  /* yacc  */
            y16(m->yuk, 8,  (uint32_t)(int32_t)i16_kirp(d[5]));  /* zacc  */
            y16(m->yuk, 10, (uint32_t)(int32_t)i16_kirp(d[0]));  /* xgyro */
            y16(m->yuk, 12, (uint32_t)(int32_t)i16_kirp(d[1]));  /* ygyro */
            y16(m->yuk, 14, (uint32_t)(int32_t)i16_kirp(d[2]));  /* zgyro */
            u->imu_indeks = (u->imu_indeks + 1) % u->imu_kayit;
        }
        else
        {
            /* SENTETIK - gurultu BILEREK yuksek */
            y16(m->yuk, 4,  (uint32_t)(int32_t)(int16_t)(  40 + gurultu(u, 30)));
            y16(m->yuk, 6,  (uint32_t)(int32_t)(int16_t)( -25 + gurultu(u, 30)));
            y16(m->yuk, 8,  (uint32_t)(int32_t)(int16_t)(-1000 + gurultu(u, 35)));
            y16(m->yuk, 10, (uint32_t)(int32_t)(int16_t)(gurultu(u, 12)));
            y16(m->yuk, 12, (uint32_t)(int32_t)(int16_t)(gurultu(u, 12)));
            y16(m->yuk, 14, (uint32_t)(int32_t)(int16_t)(gurultu(u, 10)));
        }
        /* Manyetometre her durumda sentetik: fikstur MAG tasimiyor. */
        y16(m->yuk, 16, (uint32_t)(int32_t)(int16_t)( 220 + gurultu(u, 6)));
        y16(m->yuk, 18, (uint32_t)(int32_t)(int16_t)(  60 + gurultu(u, 6)));
        y16(m->yuk, 20, (uint32_t)(int32_t)(int16_t)(-410 + gurultu(u, 6)));
        break;

    case 30:  /* ATTITUDE */
        y32(m->yuk, 0, ms);
        if (u->att != NULL)
        {
            /* GERCEK yonelim: fikstur milirad tutar, MAVLink radyan. */
            const int32_t *a = &u->att[u->att_indeks * 3];

            yf(m->yuk, 4,  (double)a[0] * 0.001);   /* roll  */
            yf(m->yuk, 8,  (double)a[1] * 0.001);   /* pitch */
            yf(m->yuk, 12, (double)a[2] * 0.001);   /* yaw   */
            u->att_indeks = (u->att_indeks + 1) % u->att_kayit;
        }
        else
        {
            yf(m->yuk, 4,  (0.10 * sinus(zaman * 0.8)) + ((double)gurultu(u, 100) * 0.0001));
            yf(m->yuk, 8,  (0.07 * kosinus(zaman * 0.6)) + ((double)gurultu(u, 100) * 0.0001));
            yf(m->yuk, 12, (0.50 * sinus(zaman * 0.05)) + ((double)gurultu(u, 60) * 0.0001));
        }
        if (u->imu != NULL)
        {
            /* Acisal hizlar gercek jiroskoptan (mrad/sn -> rad/sn).
             * ATT kaydi hiz tasimaz; jiroskop AYNI ucusun kanalidir.
             * Ornek ornek hizalanmis DEGILDIR - ama sikistirma her
             * kanali kendi icinde islediginden onemli olan kanalin
             * istatistigidir, o da gercektir. */
            const int32_t *g = &u->imu[u->imu_indeks * 6];

            yf(m->yuk, 16, (double)g[0] * 0.001);
            yf(m->yuk, 20, (double)g[1] * 0.001);
            yf(m->yuk, 24, (double)g[2] * 0.001);
        }
        else
        {
            yf(m->yuk, 16, (0.08 * kosinus(zaman * 0.8)) + ((double)gurultu(u, 150) * 0.0001));
            yf(m->yuk, 20, (0.04 * sinus(zaman * 0.6)) + ((double)gurultu(u, 150) * 0.0001));
            yf(m->yuk, 24, (0.02 * kosinus(zaman * 0.05)) + ((double)gurultu(u, 120) * 0.0001));
        }
        break;

    case 33:  /* GLOBAL_POSITION_INT */
        y32(m->yuk, 0, ms);
        y32(m->yuk, 4,  (uint32_t)lat);
        y32(m->yuk, 8,  (uint32_t)lon);
        y32(m->yuk, 12, (uint32_t)(120000 + (int32_t)(2000.0 * sinus(zaman * 0.05))
                                   + gurultu(u, 120)));
        y32(m->yuk, 16, (uint32_t)(5000 + (int32_t)(2000.0 * sinus(zaman * 0.05))
                                   + gurultu(u, 120)));
        y16(m->yuk, 20, (uint32_t)(int32_t)(int16_t)(( 900 + gurultu(u, 40))));
        y16(m->yuk, 22, (uint32_t)(int32_t)(int16_t)((-450 + gurultu(u, 40))));
        y16(m->yuk, 24, (uint32_t)(int32_t)(int16_t)((  20 + gurultu(u, 25))));
        y16(m->yuk, 26, (uint32_t)(((int32_t)(zaman * 30.0) % 36000)));
        u->gps_indeks = (u->gps_indeks + 1) % u->gps_kayit;
        break;

    case 36:  /* SERVO_OUTPUT_RAW */
        {
            int32_t k;
            y32(m->yuk, 0, ms);
            for (k = 0; k < 8; k++)
            {
                int32_t taban = (k < 4) ? 1500 : 1000;
                y16(m->yuk, 4 + (k * 2),
                    (uint32_t)(taban + (int32_t)(60.0 * sinus((zaman * 0.8)
                               + ((double)k * 0.5))) + gurultu(u, 4)));
            }
            y8(m->yuk, 20, 0u);
        }
        break;

    case 65:  /* RC_CHANNELS - ilk 8 kanal kullanimda, kalan 10'u sifir
               * (v2 kirpmasi burada devreye girer) */
        {
            int32_t k;
            y32(m->yuk, 0, ms);
            for (k = 0; k < 8; k++)
            {
                int32_t taban = (k < 4) ? 1500 : 1000;
                y16(m->yuk, 4 + (k * 2), (uint32_t)(taban + gurultu(u, 3)));
            }
            y8(m->yuk, 40, 8u);                                /* chancount */
            y8(m->yuk, 41, (uint32_t)(190 + gurultu(u, 8)));   /* rssi      */
        }
        break;

    case 74:  /* VFR_HUD */
        yf(m->yuk, 0,  12.5 + (0.8 * sinus(zaman * 0.3)) + ((double)gurultu(u, 40) * 0.001));
        yf(m->yuk, 4,  12.0 + (0.9 * sinus(zaman * 0.3)) + ((double)gurultu(u, 40) * 0.001));
        yf(m->yuk, 8,  120.0 + (2.0 * sinus(zaman * 0.05)) + ((double)gurultu(u, 30) * 0.001));
        yf(m->yuk, 12, (0.6 * kosinus(zaman * 0.05)) + ((double)gurultu(u, 60) * 0.001));
        y16(m->yuk, 16, (uint32_t)(int32_t)(int16_t)(((int32_t)(zaman * 0.3) % 360)));
        y16(m->yuk, 18, (uint32_t)(55 + gurultu(u, 4)));
        break;

    case 147: /* BATTERY_STATUS */
        {
            int32_t k;
            y32(m->yuk, 0, (uint32_t)(int32_t)(zaman * 4.2));   /* mAh      */
            y32(m->yuk, 4, (uint32_t)(int32_t)(zaman * 15.0));  /* enerji   */
            y16(m->yuk, 8, (uint32_t)(int32_t)(int16_t)(2800 + gurultu(u, 20)));
            for (k = 0; k < 4; k++)
            {
                y16(m->yuk, 10 + (k * 2),
                    (uint32_t)(4200 - (int32_t)(zaman * 0.3) + gurultu(u, 8)));
            }
            /* kalan 6 hucre 65535 = "yok" degil, sifir birakiliyor */
            y16(m->yuk, 30, (uint32_t)(int32_t)(int16_t)(1500 + gurultu(u, 150)));
            y8(m->yuk, 32, 0u);
            y8(m->yuk, 33, 1u);
            y8(m->yuk, 34, 1u);
            y8(m->yuk, 35, (uint32_t)(95 - (int32_t)(zaman / 60.0)));
        }
        break;

    case 241: /* VIBRATION */
    default:
        y64(m->yuk, 0, us);
        yf(m->yuk, 8,  6.0 + ((double)gurultu(u, 900) * 0.001));
        yf(m->yuk, 12, 5.5 + ((double)gurultu(u, 900) * 0.001));
        yf(m->yuk, 16, 8.2 + ((double)gurultu(u, 1200) * 0.001));
        y32(m->yuk, 20, 0u);
        y32(m->yuk, 24, 0u);
        y32(m->yuk, 28, 0u);
        break;
    }
}

int32_t mav_uretici_sonraki(mav_uretici *u, double sure_siniri, mav_mesaj *m)
{
    int32_t adet = 0;
    const mesaj_tanimi *sema = mav_sema_tablosu(&adet);
    int32_t en_erken = -1;
    double  en_erken_t = 0.0;
    int32_t i;

    for (i = 0; i < adet; i++)
    {
        if ((en_erken < 0) || (u->sonraki[i] < en_erken_t))
        {
            en_erken   = i;
            en_erken_t = u->sonraki[i];
        }
    }

    if ((en_erken < 0) || (en_erken_t > sure_siniri)) { return 0; }

    u->t = en_erken_t;
    yuk_uret(u, &sema[en_erken], m);
    {
        double hz = mav_hiz(&sema[en_erken], u->profil);
        /* 0 Hz mesaj bir daha secilmesin diye ufkun otesine atilir. */
        u->sonraki[en_erken] += (hz > 0.0) ? (1.0 / hz) : 1.0e30;
    }
    return 1;
}
