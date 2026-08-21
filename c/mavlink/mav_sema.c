/* =====================================================================
 * MAVLINK SEMA TABLOSU VE KADEME POLITIKASI
 * ---------------------------------------------------------------------
 * Burasi tasarimin kalbi ve DEGISTIRILMESI BEKLENEN yerdir. Iki karar
 * bu tablodadir:
 *
 *   1) Hangi mesaj hangi kademeye gider (canli mi, toplu mu)?
 *   2) Ondalikli alanlar hangi hassasiyetle kuantalanir?
 *
 * KADEME POLITIKASI - gerekce:
 *   CANLI olmasi gerekenler, operatorun ya da otopilotun GERCEK ZAMANDA
 *   uzerine karar verdigi seylerdir:
 *     - HEARTBEAT : link canli mi, ucus modu ne, silahli mi
 *     - SYS_STATUS: batarya, sensor hatalari, alarm durumu
 *   Bunlar zaten dusuk hizlidir; sikistirmadan kazanc da azdir.
 *
 *   TOPLU olanlar yuksek hizli ve gecikmeye toleransli olanlardir:
 *     - ATTITUDE, SCALED_IMU, RC_CHANNELS, SERVO_OUTPUT, VIBRATION
 *     - GPS ve konum (tam hizda)
 *   Bant genisliginin buyuk kismini bunlar yer; kazanc buradan gelir.
 *
 *   SEYRELTME: konum ve VFR_HUD icin "her N'inci ornek AYRICA canli
 *   gider" kurali vardir. Boylece operator haritada 1 Hz canli konum
 *   gorur, tam hizli veri ise sikistirilmis akar. Bedeli kucuk bir
 *   tekrardir; karsiligi sifir gecikmeli bir gostergedir.
 *
 * ---------------------------------------------------------------------
 * !!! ALAN DUZENLERI ELLE GIRILMISTIR !!!
 * common.xml'in yaygin surumune gore yazilmistir. MAVLink v2 "message
 * extension" ozelligi mesajlara SONRADAN alan ekleyebilir. Kullanmadan
 * once kendi dialect surumunuzle dogrulayin. mav_sema_dogrula() alan
 * boyutlarinin toplaminin yuk boyutunu tutturdugunu denetler.
 * ===================================================================== */

#include <string.h>
#include "mav.h"

/* =====================================================================
 * ALAN TABLOLARI
 * ===================================================================== */

static const alan_tanimi A_HEARTBEAT[] = {
    { "custom_mode",     ALAN_U32, 1, 0.0f },
    { "type",            ALAN_U8,  1, 0.0f },
    { "autopilot",       ALAN_U8,  1, 0.0f },
    { "base_mode",       ALAN_U8,  1, 0.0f },
    { "system_status",   ALAN_U8,  1, 0.0f },
    { "mavlink_version", ALAN_U8,  1, 0.0f }
};

static const alan_tanimi A_SYS_STATUS[] = {
    { "sensors_present",   ALAN_U32, 1, 0.0f },
    { "sensors_enabled",   ALAN_U32, 1, 0.0f },
    { "sensors_health",    ALAN_U32, 1, 0.0f },
    { "load",              ALAN_U16, 1, 0.0f },
    { "voltage_battery",   ALAN_U16, 1, 0.0f },
    { "current_battery",   ALAN_I16, 1, 0.0f },
    { "drop_rate_comm",    ALAN_U16, 1, 0.0f },
    { "errors_comm",       ALAN_U16, 1, 0.0f },
    { "errors_count",      ALAN_U16, 4, 0.0f },
    { "battery_remaining", ALAN_I8,  1, 0.0f }
};

static const alan_tanimi A_GPS_RAW_INT[] = {
    { "time_usec",          ALAN_U64, 1, 0.0f },
    { "lat",                ALAN_I32, 1, 0.0f },
    { "lon",                ALAN_I32, 1, 0.0f },
    { "alt",                ALAN_I32, 1, 0.0f },
    { "eph",                ALAN_U16, 1, 0.0f },
    { "epv",                ALAN_U16, 1, 0.0f },
    { "vel",                ALAN_U16, 1, 0.0f },
    { "cog",                ALAN_U16, 1, 0.0f },
    { "fix_type",           ALAN_U8,  1, 0.0f },
    { "satellites_visible", ALAN_U8,  1, 0.0f }
};

static const alan_tanimi A_SCALED_IMU[] = {
    { "time_boot_ms", ALAN_U32, 1, 0.0f },
    { "acc",          ALAN_I16, 3, 0.0f },
    { "gyro",         ALAN_I16, 3, 0.0f },
    { "mag",          ALAN_I16, 3, 0.0f }
};

/* Yonelim acilari: 0.001 rad (~0.06 derece) fazlasiyla yeterlidir. */
static const alan_tanimi A_ATTITUDE[] = {
    { "time_boot_ms", ALAN_U32, 1,    0.0f },
    { "roll",         ALAN_F32, 1, 1000.0f },
    { "pitch",        ALAN_F32, 1, 1000.0f },
    { "yaw",          ALAN_F32, 1, 1000.0f },
    { "rollspeed",    ALAN_F32, 1, 1000.0f },
    { "pitchspeed",   ALAN_F32, 1, 1000.0f },
    { "yawspeed",     ALAN_F32, 1, 1000.0f }
};

static const alan_tanimi A_GLOBAL_POSITION_INT[] = {
    { "time_boot_ms", ALAN_U32, 1, 0.0f },
    { "lat",          ALAN_I32, 1, 0.0f },
    { "lon",          ALAN_I32, 1, 0.0f },
    { "alt",          ALAN_I32, 1, 0.0f },
    { "relative_alt", ALAN_I32, 1, 0.0f },
    { "vx",           ALAN_I16, 1, 0.0f },
    { "vy",           ALAN_I16, 1, 0.0f },
    { "vz",           ALAN_I16, 1, 0.0f },
    { "hdg",          ALAN_U16, 1, 0.0f }
};

static const alan_tanimi A_SERVO_OUTPUT_RAW[] = {
    { "time_usec", ALAN_U32, 1, 0.0f },
    { "servo_raw", ALAN_U16, 8, 0.0f },
    { "port",      ALAN_U8,  1, 0.0f }
};

static const alan_tanimi A_RC_CHANNELS[] = {
    { "time_boot_ms", ALAN_U32, 1,  0.0f },
    { "chan_raw",     ALAN_U16, 18, 0.0f },
    { "chancount",    ALAN_U8,  1,  0.0f },
    { "rssi",         ALAN_U8,  1,  0.0f }
};

static const alan_tanimi A_VFR_HUD[] = {
    { "airspeed",    ALAN_F32, 1, 100.0f },
    { "groundspeed", ALAN_F32, 1, 100.0f },
    { "alt",         ALAN_F32, 1, 100.0f },
    { "climb",       ALAN_F32, 1, 100.0f },
    { "heading",     ALAN_I16, 1,   0.0f },
    { "throttle",    ALAN_U16, 1,   0.0f }
};

static const alan_tanimi A_BATTERY_STATUS[] = {
    { "current_consumed",  ALAN_I32, 1,  0.0f },
    { "energy_consumed",   ALAN_I32, 1,  0.0f },
    { "temperature",       ALAN_I16, 1,  0.0f },
    { "voltages",          ALAN_U16, 10, 0.0f },
    { "current_battery",   ALAN_I16, 1,  0.0f },
    { "id",                ALAN_U8,  1,  0.0f },
    { "battery_function",  ALAN_U8,  1,  0.0f },
    { "type",              ALAN_U8,  1,  0.0f },
    { "battery_remaining", ALAN_I8,  1,  0.0f }
};

static const alan_tanimi A_VIBRATION[] = {
    { "time_usec",   ALAN_U64, 1,    0.0f },
    { "vibration_x", ALAN_F32, 1, 1000.0f },
    { "vibration_y", ALAN_F32, 1, 1000.0f },
    { "vibration_z", ALAN_F32, 1, 1000.0f },
    { "clipping",    ALAN_U32, 3,    0.0f }
};

#define ALAN_ADEDI(a) ((int32_t)(sizeof(a) / sizeof((a)[0])))

/* =====================================================================
 * MESAJ TABLOSU
 * ---------------------------------------------------------------------
 * hz degerleri ArduPilot'un "normal" telemetri onayarina yakin secilmis
 * TEMSILI degerlerdir; kendi aracinizin SRx_* parametreleriyle
 * degistirin.
 * ===================================================================== */

static const mesaj_tanimi SEMA[] =
{
    {   0, "HEARTBEAT",           9, A_HEARTBEAT,           ALAN_ADEDI(A_HEARTBEAT),
        KADEME_CANLI, 0,  1.0 },
    {   1, "SYS_STATUS",         31, A_SYS_STATUS,          ALAN_ADEDI(A_SYS_STATUS),
        KADEME_CANLI, 0,  2.0 },
    {  24, "GPS_RAW_INT",        30, A_GPS_RAW_INT,         ALAN_ADEDI(A_GPS_RAW_INT),
        KADEME_TOPLU, 0,  5.0 },
    {  26, "SCALED_IMU",         22, A_SCALED_IMU,          ALAN_ADEDI(A_SCALED_IMU),
        KADEME_TOPLU, 0, 10.0 },
    {  30, "ATTITUDE",           28, A_ATTITUDE,            ALAN_ADEDI(A_ATTITUDE),
        KADEME_TOPLU, 0, 10.0 },
    {  33, "GLOBAL_POSITION_INT",28, A_GLOBAL_POSITION_INT, ALAN_ADEDI(A_GLOBAL_POSITION_INT),
        KADEME_TOPLU, 5,  5.0 },   /* her 5'incisi ayrica canli -> 1 Hz harita */
    {  36, "SERVO_OUTPUT_RAW",   21, A_SERVO_OUTPUT_RAW,    ALAN_ADEDI(A_SERVO_OUTPUT_RAW),
        KADEME_TOPLU, 0,  5.0 },
    {  65, "RC_CHANNELS",        42, A_RC_CHANNELS,         ALAN_ADEDI(A_RC_CHANNELS),
        KADEME_TOPLU, 0,  5.0 },
    {  74, "VFR_HUD",            20, A_VFR_HUD,             ALAN_ADEDI(A_VFR_HUD),
        KADEME_TOPLU, 4,  4.0 },   /* her 4'uncusu ayrica canli -> 1 Hz gosterge */
    { 147, "BATTERY_STATUS",     36, A_BATTERY_STATUS,      ALAN_ADEDI(A_BATTERY_STATUS),
        KADEME_TOPLU, 0,  1.0 },
    { 241, "VIBRATION",          32, A_VIBRATION,           ALAN_ADEDI(A_VIBRATION),
        KADEME_TOPLU, 0,  2.0 }
};

#define SEMA_ADEDI ((int32_t)(sizeof(SEMA) / sizeof(SEMA[0])))

const mesaj_tanimi *mav_sema_tablosu(int32_t *adet_cikti)
{
    if (adet_cikti != NULL) { *adet_cikti = SEMA_ADEDI; }
    return SEMA;
}

const mesaj_tanimi *mav_sema_bul(uint32_t msgid)
{
    int32_t i;

    for (i = 0; i < SEMA_ADEDI; i++)
    {
        if (SEMA[i].msgid == msgid) { return &SEMA[i]; }
    }
    return NULL;
}

/* =====================================================================
 * ALAN BOYUTU VE KANAL SAYISI
 * ===================================================================== */

static int32_t alan_bayti(alan_turu t)
{
    switch (t)
    {
    case ALAN_U8:  case ALAN_I8:              return 1;
    case ALAN_U16: case ALAN_I16:             return 2;
    case ALAN_U32: case ALAN_I32: case ALAN_F32: return 4;
    case ALAN_U64: default:                   return 8;
    }
}

/* U64 iki kanala bolunur (alt 32 bit + ust 32 bit); digerleri bir kanal. */
static int32_t alan_kanali(alan_turu t)
{
    return (t == ALAN_U64) ? 2 : 1;
}

int32_t mav_kanal_sayisi(const mesaj_tanimi *t)
{
    int32_t toplam = 0;
    int32_t i;

    if (t == NULL) { return 0; }
    for (i = 0; i < t->alan_sayisi; i++)
    {
        toplam += t->alanlar[i].tekrar * alan_kanali(t->alanlar[i].tur);
    }
    return toplam;
}

/**
 * Sema tutarliligi: alan boyutlarinin toplami yuk boyutunu tutmali,
 * kanal sayisi sinirlari asmamalidir.
 * @return hatali mesaj sayisi (0 = temiz)
 */
int32_t mav_sema_dogrula(void (*bildir)(const char *ad, int32_t beklenen,
                                        int32_t bulunan))
{
    int32_t hata = 0;
    int32_t i;

    for (i = 0; i < SEMA_ADEDI; i++)
    {
        int32_t toplam = 0;
        int32_t j;

        for (j = 0; j < SEMA[i].alan_sayisi; j++)
        {
            toplam += SEMA[i].alanlar[j].tekrar
                      * alan_bayti(SEMA[i].alanlar[j].tur);
        }

        if (toplam != SEMA[i].yuk_boyutu)
        {
            if (bildir != NULL)
            {
                bildir(SEMA[i].ad, SEMA[i].yuk_boyutu, toplam);
            }
            hata++;
        }
        if (mav_kanal_sayisi(&SEMA[i]) > MAV_MAKS_KANAL)
        {
            if (bildir != NULL)
            {
                bildir(SEMA[i].ad, MAV_MAKS_KANAL, mav_kanal_sayisi(&SEMA[i]));
            }
            hata++;
        }
    }
    return hata;
}

/* =====================================================================
 * YUK <-> KANAL DONUSUMU
 * ===================================================================== */

static uint32_t oku_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void yaz_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint16_t oku_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void yaz_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

/* Sifirdan uzaga yuvarlama - ElBari float katmaniyla ayni kural. */
static int32_t kuantala_deger(float deger, float olcek)
{
    double x = (double)deger * (double)olcek;

    if (x >= 2147483647.0)  { return 2147483647; }
    if (x <= -2147483648.0) { return (-2147483647 - 1); }
    return (x >= 0.0) ? (int32_t)(x + 0.5) : (int32_t)(x - 0.5);
}

void mav_yuk_coz(const mesaj_tanimi *t, const uint8_t *yuk,
                 int32_t kuantala, int32_t *kanallar)
{
    int32_t ofset = 0;
    int32_t k = 0;
    int32_t i;

    for (i = 0; i < t->alan_sayisi; i++)
    {
        const alan_tanimi *a = &t->alanlar[i];
        int32_t r;

        for (r = 0; r < a->tekrar; r++)
        {
            switch (a->tur)
            {
            case ALAN_U8:
                kanallar[k] = (int32_t)yuk[ofset];
                k++;
                break;
            case ALAN_I8:
                kanallar[k] = (int32_t)(int8_t)yuk[ofset];
                k++;
                break;
            case ALAN_U16:
                kanallar[k] = (int32_t)oku_le16(&yuk[ofset]);
                k++;
                break;
            case ALAN_I16:
                kanallar[k] = (int32_t)(int16_t)oku_le16(&yuk[ofset]);
                k++;
                break;
            case ALAN_U32:
            case ALAN_I32:
                kanallar[k] = (int32_t)oku_le32(&yuk[ofset]);
                k++;
                break;
            case ALAN_U64:
                /* Alt 32 bit ayri kanal: ardisik zaman damgalarinin farki
                 * kucuk kalir, sarma ise 32 bitte bir kez aykiri deger
                 * uretir - PFOR yamalamasi bunu zaten kaldirir. */
                kanallar[k]     = (int32_t)oku_le32(&yuk[ofset]);
                kanallar[k + 1] = (int32_t)oku_le32(&yuk[ofset + 4]);
                k += 2;
                break;
            case ALAN_F32:
            default:
                if ((kuantala != 0) && (a->olcek > 0.0f))
                {
                    float f;
                    uint32_t ham = oku_le32(&yuk[ofset]);
                    (void)memcpy(&f, &ham, sizeof(f));
                    kanallar[k] = kuantala_deger(f, a->olcek);
                }
                else
                {
                    /* Bit deseni oldugu gibi: kayipsiz ama kotu sikisir. */
                    kanallar[k] = (int32_t)oku_le32(&yuk[ofset]);
                }
                k++;
                break;
            }
            ofset += alan_bayti(a->tur);
        }
    }
}

void mav_yuk_yaz(const mesaj_tanimi *t, const int32_t *kanallar,
                 int32_t kuantala, uint8_t *yuk)
{
    int32_t ofset = 0;
    int32_t k = 0;
    int32_t i;

    for (i = 0; i < t->alan_sayisi; i++)
    {
        const alan_tanimi *a = &t->alanlar[i];
        int32_t r;

        for (r = 0; r < a->tekrar; r++)
        {
            switch (a->tur)
            {
            case ALAN_U8:
            case ALAN_I8:
                yuk[ofset] = (uint8_t)((uint32_t)kanallar[k] & 0xFFu);
                k++;
                break;
            case ALAN_U16:
            case ALAN_I16:
                yaz_le16(&yuk[ofset], (uint16_t)((uint32_t)kanallar[k] & 0xFFFFu));
                k++;
                break;
            case ALAN_U32:
            case ALAN_I32:
                yaz_le32(&yuk[ofset], (uint32_t)kanallar[k]);
                k++;
                break;
            case ALAN_U64:
                yaz_le32(&yuk[ofset],     (uint32_t)kanallar[k]);
                yaz_le32(&yuk[ofset + 4], (uint32_t)kanallar[k + 1]);
                k += 2;
                break;
            case ALAN_F32:
            default:
                if ((kuantala != 0) && (a->olcek > 0.0f))
                {
                    float f = (float)((double)kanallar[k] / (double)a->olcek);
                    uint32_t ham;
                    (void)memcpy(&ham, &f, sizeof(ham));
                    yaz_le32(&yuk[ofset], ham);
                }
                else
                {
                    yaz_le32(&yuk[ofset], (uint32_t)kanallar[k]);
                }
                k++;
                break;
            }
            ofset += alan_bayti(a->tur);
        }
    }
}
