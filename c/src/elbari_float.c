/* =====================================================================
 * ELBARI - Float kuantalama katmani
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * ---------------------------------------------------------------------
 * NEDEN VAR:
 * Cekirdek motor tamsayi (int32) uzerinde calisir. Gercek telemetrinin
 * onemli bir kismi ise ondalikli (float32) tasinir: yonelim acilari,
 * hiz, ivme, batarya gerilimi, quaternion bilesenleri...
 *
 * Bu katman ondalikli degerleri, istenen HASSASIYETE gore olcekleyip
 * tamsayiya cevirir. Sonuc mevcut kanal ve cerceve katmanlarina oldugu
 * gibi verilebilir; BICIM DEGISMEZ.
 *
 * ---------------------------------------------------------------------
 * !!! BU KATMAN KAYIPLIDIR !!!
 * ---------------------------------------------------------------------
 * Kuantalama, sayinin secilen hassasiyetin altindaki kismini atar.
 * Ornegin olcek = 1000 ise 0.0014567 degeri 0.001 veya 0.002 olarak
 * geri gelir. Bu, telemetri icin genellikle istenen davranistir:
 * bir yonelim acisini 0.001 radyan (0.06 derece) hassasiyetle tasimak
 * fazlasiyla yeterlidir ve tam float tasimak bant genisligi israfidir.
 *
 * ANCAK: Tam degerin korunmasi gereken veriler (ham sensor kaydi,
 * saglama toplami, kriptografik malzeme) bu katmandan GECIRILMEMELIDIR.
 *
 * Kayipsiz float sikistirma (XOR tabanli, Gorilla/Chimp ailesi) ayri
 * bir bit bicimi gerektirir ve bu surumde YOKTUR.
 * ---------------------------------------------------------------------
 *
 * HASSASIYET SECIMI (olcek = 1 / hassasiyet):
 *
 *   Buyukluk              Tipik hassasiyet     Olcek
 *   --------------------  -------------------  -------
 *   Yonelim (radyan)      0.001 rad            1000
 *   Yonelim (derece)      0.01 derece          100
 *   Hiz (m/s)             0.01 m/s             100
 *   Irtifa (m)            0.01 m (cm)          100
 *   Batarya (V)           0.01 V               100
 *   Quaternion (birimsiz) 0.0001               10000
 *
 * TASMA:
 * deger * olcek carpimi int32 araligini asarsa hata dondurulur; sessizce
 * yanlis deger uretilmez. Cagiran ya olcegi kucultmeli ya da o kanali
 * kuantalamadan gecirmemelidir.
 *
 * BELIRLENIMCILIK (determinism):
 * Yuvarlama, .NET surumuyle BIREBIR AYNI sonucu vermek zorundadir. Bu
 * yuzden hesap cift duyarlikta (double) yapilir ve yuvarlama acikca
 * "sifirdan uzaga" (round half away from zero) uygulanir. Tek duyarlikta
 * ara islem birakilirsa bazi derleyiciler daha genis ara duyarlik
 * kullanabilir ve iki surum ayrisabilir.
 * ===================================================================== */

#include "elbari.h"
#include "elbari_ic.h"

/* ---------------------------------------------------------------------
 * TEK KANAL
 * ------------------------------------------------------------------- */

int32_t elbari_float_kuantala(const float *girdi,
                              int32_t      adet,
                              float        olcek,
                              int32_t     *cikti)
{
    int32_t i;

    if ((girdi == NULL) || (cikti == NULL) || (adet < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (adet > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    /* Olcek pozitif ve sonlu olmali */
    if (!(olcek > 0.0f) || !(olcek < 3.0e38f))
    {
        return ELBARI_HATA_PARAMETRE;
    }

    for (i = 0; i < adet; i++)
    {
        double deger = (double)girdi[i];
        double olcekli;

        /* NaN ve sonsuz degerler tamsayiya cevrilemez.
         * NaN kendisine esit olmadigi icin bu karsilastirma ile yakalanir. */
        if (!(deger == deger))
        {
            return ELBARI_HATA_PARAMETRE;
        }

        olcekli = deger * (double)olcek;

        /* Sifirdan uzaga yuvarlama (round half away from zero) */
        olcekli = (olcekli >= 0.0) ? (olcekli + 0.5) : (olcekli - 0.5);

        /* Tasma denetimi: sessizce yanlis deger uretme */
        if ((olcekli > 2147483647.0) || (olcekli < -2147483648.0))
        {
            return ELBARI_HATA_PARAMETRE;
        }

        cikti[i] = (int32_t)olcekli;
    }

    return ELBARI_TAMAM;
}

int32_t elbari_float_coz(const int32_t *girdi,
                         int32_t        adet,
                         float          olcek,
                         float         *cikti)
{
    int32_t i;

    if ((girdi == NULL) || (cikti == NULL) || (adet < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (adet > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (!(olcek > 0.0f) || !(olcek < 3.0e38f))
    {
        return ELBARI_HATA_PARAMETRE;
    }

    for (i = 0; i < adet; i++)
    {
        cikti[i] = (float)((double)girdi[i] / (double)olcek);
    }

    return ELBARI_TAMAM;
}

/* ---------------------------------------------------------------------
 * COK KANALLI
 * ---------------------------------------------------------------------
 * Her kanalin kendi olcegi olur; bu onemlidir. Bir yonelim acisi ile
 * batarya gerilimi ayni hassasiyeti gerektirmez ve ayni olcegi
 * kullanmak ya bant genisligi israfi ya da hassasiyet kaybi olur.
 *
 * NOT: Olcekler bicim icinde TASINMAZ. Gonderici ve alici ayni olcek
 * dizisini kullanmak zorundadir (telemetri semasinin parcasi olarak,
 * bant disi anlasilir). Bu, MAVLink gibi protokollerin calisma bicimiyle
 * aynidir: alan tanimlari iki tarafta da bilinir.
 * ------------------------------------------------------------------- */

int32_t elbari_float_kuantala_kanalli(const float *girdi,
                                      int32_t      eleman_sayisi,
                                      int32_t      kanal_sayisi,
                                      const float *olcekler,
                                      int32_t     *cikti)
{
    int32_t i;

    if ((girdi == NULL) || (cikti == NULL) || (olcekler == NULL) ||
        (eleman_sayisi < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if ((kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }

    for (i = 0; i < eleman_sayisi; i++)
    {
        int32_t kanal = i % kanal_sayisi;
        int32_t durum = elbari_float_kuantala(&girdi[i], 1, olcekler[kanal], &cikti[i]);

        if (durum != ELBARI_TAMAM)
        {
            return durum;
        }
    }

    return ELBARI_TAMAM;
}

int32_t elbari_float_coz_kanalli(const int32_t *girdi,
                                 int32_t        eleman_sayisi,
                                 int32_t        kanal_sayisi,
                                 const float   *olcekler,
                                 float         *cikti)
{
    int32_t i;

    if ((girdi == NULL) || (cikti == NULL) || (olcekler == NULL) ||
        (eleman_sayisi < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if ((kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }

    for (i = 0; i < eleman_sayisi; i++)
    {
        int32_t kanal = i % kanal_sayisi;

        if (!(olcekler[kanal] > 0.0f) || !(olcekler[kanal] < 3.0e38f))
        {
            return ELBARI_HATA_PARAMETRE;
        }
        cikti[i] = (float)((double)girdi[i] / (double)olcekler[kanal]);
    }

    return ELBARI_TAMAM;
}

/* ---------------------------------------------------------------------
 * YARDIMCILAR
 * ------------------------------------------------------------------- */

float elbari_float_olcek_oner(float hassasiyet)
{
    if (!(hassasiyet > 0.0f))
    {
        return 0.0f;
    }
    return 1.0f / hassasiyet;
}

float elbari_float_maks_hata(const float *orijinal,
                             const float *geri,
                             int32_t      adet)
{
    float maks = 0.0f;
    int32_t i;

    if ((orijinal == NULL) || (geri == NULL) || (adet <= 0))
    {
        return 0.0f;
    }

    for (i = 0; i < adet; i++)
    {
        float fark = orijinal[i] - geri[i];

        if (fark < 0.0f)
        {
            fark = -fark;
        }
        if (fark > maks)
        {
            maks = fark;
        }
    }

    return maks;
}
