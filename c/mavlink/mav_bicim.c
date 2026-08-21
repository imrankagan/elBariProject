/* =====================================================================
 * MAVLINK v2 CERCEVE OKUMA / YAZMA
 * ---------------------------------------------------------------------
 * Yalnizca olcum icin gereken kadari uygulanmistir: cerceve yapisi,
 * yuk kirpma ve CRC hesabi.
 *
 * CRC_EXTRA:
 *   Gercek MAVLink, CRC'nin sonuna mesaj adindan ve alan tanimlarindan
 *   turetilen bir CRC_EXTRA bayti ekler. Bu bayt burada EZBERDEN
 *   YAZILMAZ, semadan HESAPLANIR (bkz. mav_crc_extra). Boylece sema
 *   duzeltilince CRC de kendini duzeltir.
 *
 *   Semasi bulunmayan msgid'ler icin CRC_EXTRA bilinemez; 0 kullanilir.
 *   Boyle bir mesaj gercek bir otopilot tarafindan reddedilir - ama bu
 *   dogru davranistir: tanimini bilmedigimiz bir mesaji dogrulanmis gibi
 *   gostermek daha kotu olurdu.
 * ===================================================================== */

#include "mav.h"

/* ---------------------------------------------------------------------
 * CRC-16/MCRF4XX  (MAVLink'in kullandigi saglama)
 * ------------------------------------------------------------------- */

uint16_t mav_crc16(const uint8_t *veri, int32_t boyut, uint16_t baslangic)
{
    uint16_t crc = baslangic;
    int32_t  i;

    for (i = 0; i < boyut; i++)
    {
        uint8_t gecici = (uint8_t)(veri[i] ^ (uint8_t)(crc & 0xFFu));
        gecici = (uint8_t)(gecici ^ (uint8_t)(gecici << 4));
        crc = (uint16_t)((crc >> 8)
                         ^ ((uint16_t)gecici << 8)
                         ^ ((uint16_t)gecici << 3)
                         ^ ((uint16_t)gecici >> 4));
    }
    return crc;
}

/* ---------------------------------------------------------------------
 * YAZMA
 * ------------------------------------------------------------------- */

int32_t mav_cerceve_yaz(const mav_mesaj *m, uint8_t *cikti, int32_t kapasite)
{
    int32_t  kirpik;
    int32_t  toplam;
    uint16_t crc;
    int32_t  i;

    if ((m == NULL) || (cikti == NULL)) { return -1; }
    if ((m->yuk_boyutu < 0) || (m->yuk_boyutu > MAV_MAKS_YUK)) { return -1; }

    /* v2 zorunlulugu: sondaki sifir baytlar kirpilir.
     * Bu, taban cizgisi olcumu icin kritiktir - kirpma uygulanmazsa
     * sikistirmasiz durum oldugundan buyuk gorunur. */
    kirpik = m->yuk_boyutu;
    while ((kirpik > 0) && (m->yuk[kirpik - 1] == 0u))
    {
        kirpik--;
    }

    toplam = MAV_EK_YUK + kirpik;
    if (toplam > kapasite) { return -1; }

    cikti[0] = MAV_STX;
    cikti[1] = (uint8_t)kirpik;
    cikti[2] = 0u;                 /* uyumsuzluk bayraklari (imza yok) */
    cikti[3] = 0u;                 /* uyumluluk bayraklari             */
    cikti[4] = m->sira;
    cikti[5] = m->sistem;
    cikti[6] = m->bilesen;
    cikti[7] = (uint8_t)(m->msgid & 0xFFu);
    cikti[8] = (uint8_t)((m->msgid >> 8) & 0xFFu);
    cikti[9] = (uint8_t)((m->msgid >> 16) & 0xFFu);

    for (i = 0; i < kirpik; i++)
    {
        cikti[MAV_BASLIK + i] = m->yuk[i];
    }

    /* CRC: STX haric baslik + yuk + CRC_EXTRA */
    crc = mav_crc16(&cikti[1], (MAV_BASLIK - 1) + kirpik, 0xFFFFu);
    {
        const mesaj_tanimi *st = mav_sema_bul(m->msgid);
        uint8_t ekstra = (st != NULL) ? mav_crc_extra(st) : 0u;
        crc = mav_crc16(&ekstra, 1, crc);
    }
    cikti[MAV_BASLIK + kirpik]     = (uint8_t)(crc & 0xFFu);
    cikti[MAV_BASLIK + kirpik + 1] = (uint8_t)((crc >> 8) & 0xFFu);

    return toplam;
}

/* ---------------------------------------------------------------------
 * OKUMA
 * ------------------------------------------------------------------- */

int32_t mav_cerceve_oku(const uint8_t *girdi, int32_t boyut,
                        mav_mesaj *cikti, int32_t sema_yuk_boyutu)
{
    int32_t  kirpik;
    int32_t  toplam;
    uint16_t crc;
    uint16_t beklenen;
    int32_t  i;
    int32_t  tam;

    if ((girdi == NULL) || (cikti == NULL)) { return -1; }
    if (boyut < MAV_EK_YUK) { return -1; }
    if (girdi[0] != MAV_STX) { return -1; }

    kirpik = (int32_t)girdi[1];
    toplam = MAV_EK_YUK + kirpik;
    if (toplam > boyut) { return -1; }

    cikti->sira    = girdi[4];
    cikti->sistem  = girdi[5];
    cikti->bilesen = girdi[6];
    cikti->msgid   = (uint32_t)girdi[7]
                     | ((uint32_t)girdi[8] << 8)
                     | ((uint32_t)girdi[9] << 16);

    /* CRC dogrulamasi CRC_EXTRA'yi da icerir; bu yuzden msgid once okunur. */
    crc = mav_crc16(&girdi[1], (MAV_BASLIK - 1) + kirpik, 0xFFFFu);
    {
        const mesaj_tanimi *st = mav_sema_bul(cikti->msgid);
        uint8_t ekstra = (st != NULL) ? mav_crc_extra(st) : 0u;
        crc = mav_crc16(&ekstra, 1, crc);
    }
    beklenen = (uint16_t)((uint16_t)girdi[MAV_BASLIK + kirpik]
                          | ((uint16_t)girdi[MAV_BASLIK + kirpik + 1] << 8));
    if (crc != beklenen) { return -1; }

    /* Kirpilan sifirlar geri konur: sema boyutuna kadar sifirla doldur. */
    tam = (sema_yuk_boyutu > kirpik) ? sema_yuk_boyutu : kirpik;
    if (tam > MAV_MAKS_YUK) { return -1; }

    for (i = 0; i < kirpik; i++)
    {
        cikti->yuk[i] = girdi[MAV_BASLIK + i];
    }
    for (i = kirpik; i < tam; i++)
    {
        cikti->yuk[i] = 0u;
    }
    cikti->yuk_boyutu = tam;

    return toplam;
}
