/* =====================================================================
 * ARDUPILOT DATAFLASH (.bin) LOG OKUYUCUSU
 * ---------------------------------------------------------------------
 * NEDEN BU FORMAT:
 *   ALFA veri setinde ucus verisi uc bicimde var: ROS .bag, .csv ve
 *   Pixhawk'in kendi Dataflash loglari. Ilk ikisi MAVROS'tan gecmistir -
 *   MAVROS koordinat cercevesini cevirir (NED -> ENU), birim donusturur
 *   ve quaternion'a gecer. Yani oradaki sayilar TELDE GIDEN sayilar
 *   DEGILDIR.
 *
 *   Biz telde ne aktigini olcuyoruz. Bu yuzden okudugumuz sey otopilotun
 *   HAM kaydi olmalidir: Dataflash .bin.
 *
 * FORMAT - kendini tanimlar:
 *   Her kayit su basligi tasir:  0xA3 0x95 <tip>
 *   Tip 0x80 (FMT) ozeldir: BASKA bir tipin yapisini tanimlar.
 *   Yani log dosyasi, kendi sozlugunu icinde tasir. Bu sayede
 *   ArduPilot surumu degisse bile ayristirici calismaya devam eder;
 *   alanlar ADIYLA aranir, sabit ofsetle degil.
 *
 *   FMT kaydinin kendisi sabit yapidadir (89 bayt):
 *     [0..2]   0xA3 0x95 0x80
 *     [3]      tanimlanan tip
 *     [4]      o tipin toplam uzunlugu (baslik dahil)
 *     [5..8]   ad      (4 bayt, sifirla dolgulu)
 *     [9..24]  bicim   (16 bayt) - her alan icin bir karakter
 *     [25..88] etiketler (64 bayt) - virgulle ayrilmis alan adlari
 *
 * KAPSAM NOTU:
 *   Bu klasor olcum ve veri hazirlama kodudur. MISRA C:2012 uyum
 *   iddiasi c/src/ icin gecerlidir; burasi kapsam disidir.
 * ===================================================================== */

#ifndef DATAFLASH_H
#define DATAFLASH_H

#include <stdint.h>
#include <stddef.h>

#define DF_BAS1        (0xA3u)
#define DF_BAS2        (0x95u)
#define DF_FMT_TIP     (0x80u)
#define DF_FMT_UZUNLUK (89)
#define DF_MAKS_ALAN   (32)

typedef struct
{
    uint8_t tip;
    uint8_t uzunluk;              /* baslik dahil toplam bayt */
    char    ad[8];                /* "ATT", "IMU", "GPS" ...  */
    int32_t alan_sayisi;
    char    alan_adi[DF_MAKS_ALAN][20];
    char    alan_bicimi[DF_MAKS_ALAN];
    int32_t alan_ofseti[DF_MAKS_ALAN];   /* kayit basindan itibaren */
} df_tanim;

typedef struct
{
    const uint8_t *veri;
    int64_t        boyut;
    int64_t        konum;
    df_tanim       tanimlar[256];
    int32_t        tanimli[256];
    int64_t        atlanan_bayt;   /* senkron kaybi tanisi icin */
} df_okuyucu;

/** Okuyucuyu bellekteki log tamponuna baglar. */
void df_kur(df_okuyucu *o, const uint8_t *veri, int64_t boyut);

/**
 * Bir sonraki kaydi okur. FMT kayitlari icerde islenir ve DISARI
 * VERILMEZ; cagiran yalnizca veri kayitlarini gorur.
 *
 * @param tanim_cikti  okunan kaydin tanimi
 * @param govde_cikti  kaydin basina isaretci (baslik dahil)
 * @return 1 kayit okundu, 0 dosya bitti
 */
int32_t df_sonraki(df_okuyucu *o, const df_tanim **tanim_cikti,
                   const uint8_t **govde_cikti);

/** Ada gore tanim arar (orn. "ATT"). Yoksa NULL. */
const df_tanim *df_tanim_bul(const df_okuyucu *o, const char *ad);

/** Etikete gore alan indeksi arar (orn. "Roll"). Yoksa -1. */
int32_t df_alan_bul(const df_tanim *t, const char *etiket);

/**
 * Bir alani tamsayi olarak okur.
 * Olcekli tipler (c/C/e/E) HAM tutulur - olcegi cagiran uygular.
 */
int64_t df_tamsayi(const df_tanim *t, const uint8_t *govde, int32_t alan);

/**
 * Bir alani ondalikli olarak okur. Olcekli tipler burada olceklenir:
 *   c,C -> /100    e,E -> /100    L -> 1e-7 derece olarak birakilir (ham)
 */
double df_ondalik(const df_tanim *t, const uint8_t *govde, int32_t alan);

/** Bir bicim karakterinin bayt uzunlugu; bilinmiyorsa 0. */
int32_t df_bicim_boyutu(char c);

#endif /* DATAFLASH_H */
