/* =====================================================================
 * ELBARI - Cerceve katmani (paket kaybina dayaniklilik)
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * ---------------------------------------------------------------------
 * NEDEN VAR:
 * Fark kodlamanin olumcul zayifligi zincirleme bagimliliktir: her deger
 * bir oncekine dayanir. Tek bir paket duserse ondan SONRAKI TUM veri
 * cozulemez hale gelir. Standart sikistiricilar bunu cozmez, cunku
 * altlarinda kayipsiz bir tasima (TCP gibi) varsayarlar.
 *
 * Insansiz arac telemetrisi ise kayipli bir telsiz linki uzerinden
 * gider. Paket dusmesi istisna degil, NORMAL calisma kosuludur.
 *
 * COZUM:
 * Akis, her biri BAGIMSIZ COZULEBILIR cercevelere bolunur:
 *   - Her cerceve kendi mutlak referansini tasir (zincir kirilir).
 *   - Her cercevede sira numarasi vardir (hangi kayitlar eksik, bilinir).
 *   - Her cerceve CRC32 ile korunur (bozulma sessizce gecmez).
 *   - Cerceveler sirasiz gelebilir, tek tek dogrulanip cozulebilir.
 *
 * Sonuc: N. cerceve kaybolursa SADECE o cercevenin kayitlari kaybolur.
 * Hata yayilimi bir cerceve ile SINIRLIDIR.
 * ===================================================================== */

#include "elbari.h"
#include "elbari_ic.h"

#define ELBARI_SIHIR_0   (0xEBu)
#define ELBARI_SIHIR_1   (0x71u)
#define ELBARI_SURUM     (1u)

/* ---------------------------------------------------------------------
 * CRC-32 (IEEE 802.3)
 * ---------------------------------------------------------------------
 * Tablo, ilk kullanimda bir kez uretilir. Tablo statiktir; dinamik
 * bellek kullanilmaz. Tek is parcacikli gomulu kullanim hedeflendigi
 * icin kilit yoktur; cok is parcacikli ortamda ilk cagriyi tek bir
 * is parcacigindan yapmak yeterlidir (idempotent islem).
 * ------------------------------------------------------------------- */

static uint32_t s_crc_tablosu[256];
static int32_t  s_crc_tablosu_hazir = 0;

static void elbari_ic_crc_tablosu_kur(void)
{
    uint32_t i;

    for (i = 0u; i < 256u; i++)
    {
        uint32_t c = i;
        int32_t  k;

        for (k = 0; k < 8; k++)
        {
            if ((c & 1u) != 0u)
            {
                c = 0xEDB88320u ^ (c >> 1);
            }
            else
            {
                c = c >> 1;
            }
        }
        s_crc_tablosu[i] = c;
    }
    s_crc_tablosu_hazir = 1;
}

uint32_t elbari_crc32(const uint8_t *veri, int32_t boyut)
{
    uint32_t crc = 0xFFFFFFFFu;
    int32_t  i;

    if ((veri == NULL) || (boyut < 0))
    {
        return 0u;
    }

    if (s_crc_tablosu_hazir == 0)
    {
        elbari_ic_crc_tablosu_kur();
    }

    for (i = 0; i < boyut; i++)
    {
        crc = s_crc_tablosu[(crc ^ (uint32_t)veri[i]) & 0xFFu] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFFu;
}

/* ---------------------------------------------------------------------
 * BOYUT HESAPLARI
 * ------------------------------------------------------------------- */

int32_t elbari_cerceve_en_kotu_durum_boyutu(int32_t kayit_sayisi,
                                            int32_t kanal_sayisi)
{
    int32_t ic;

    if ((kayit_sayisi < 0) || (kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL))
    {
        return ELBARI_HATA_PARAMETRE;
    }

    ic = elbari_kanal_en_kotu_durum_boyutu(kayit_sayisi * kanal_sayisi, kanal_sayisi);
    if (ic < 0)
    {
        return ic;
    }
    return ELBARI_CERCEVE_BASLIK_BOYUTU + ic;
}

int32_t elbari_cerceve_gerekli_calisma_alani(int32_t kayit_sayisi,
                                             int32_t kanal_sayisi)
{
    return elbari_kanal_gerekli_calisma_alani(kayit_sayisi * kanal_sayisi, kanal_sayisi);
}

/* ---------------------------------------------------------------------
 * YAZMA
 * ------------------------------------------------------------------- */

int32_t elbari_cerceve_yaz(const int32_t *kayitlar,
                           int32_t        eleman_sayisi,
                           int32_t        kanal_sayisi,
                           uint32_t       sira_no,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           uint8_t       *cikti,
                           int32_t        cikti_kapasitesi)
{
    int32_t  kayit_sayisi;
    int32_t  yuk_boyutu;
    int32_t  toplam;
    uint32_t crc;

    if ((kayitlar == NULL) || (calisma_alani == NULL) || (cikti == NULL) ||
        (eleman_sayisi < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if ((kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    /* Cerceveler tam kayit sinirinda bolunmelidir. */
    if ((eleman_sayisi % kanal_sayisi) != 0)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (cikti_kapasitesi < ELBARI_CERCEVE_BASLIK_BOYUTU)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    kayit_sayisi = eleman_sayisi / kanal_sayisi;

    /* Yuk: kanal katmani ile sikistir */
    yuk_boyutu = elbari_kanal_kabid(kayitlar, eleman_sayisi, kanal_sayisi,
                                    calisma_alani, calisma_kapasitesi,
                                    &cikti[ELBARI_CERCEVE_BASLIK_BOYUTU],
                                    cikti_kapasitesi - ELBARI_CERCEVE_BASLIK_BOYUTU);
    if (yuk_boyutu < 0)
    {
        return yuk_boyutu;
    }

    /* Baslik */
    cikti[0] = ELBARI_SIHIR_0;
    cikti[1] = ELBARI_SIHIR_1;
    cikti[2] = (uint8_t)ELBARI_SURUM;
    cikti[3] = 0u;
    elbari_ic_u32_yaz(&cikti[8], sira_no);
    elbari_ic_i32_yaz(&cikti[12], kayit_sayisi);

    /* CRC: [8..son] araligi (sira no + kayit sayisi + yuk) */
    toplam = ELBARI_CERCEVE_BASLIK_BOYUTU + yuk_boyutu;
    crc = elbari_crc32(&cikti[8], toplam - 8);
    elbari_ic_u32_yaz(&cikti[4], crc);

    return toplam;
}

/* ---------------------------------------------------------------------
 * OKUMA
 * ------------------------------------------------------------------- */

int32_t elbari_cerceve_gecerli_mi(const uint8_t *cerceve, int32_t cerceve_boyutu)
{
    uint32_t beklenen;
    uint32_t hesaplanan;

    if ((cerceve == NULL) || (cerceve_boyutu < ELBARI_CERCEVE_BASLIK_BOYUTU))
    {
        return 0;
    }
    if ((cerceve[0] != ELBARI_SIHIR_0) || (cerceve[1] != ELBARI_SIHIR_1))
    {
        return 0;
    }
    if (cerceve[2] != (uint8_t)ELBARI_SURUM)
    {
        return 0;
    }
    /* [3] ayrilmis alan: CRC kapsami disinda oldugu icin burada
     * dogrulanir; aksi halde bu bayta dusen bir bit bozulmasi fark
     * edilmeden gecerdi. */
    if (cerceve[3] != 0u)
    {
        return 0;
    }

    beklenen   = elbari_ic_u32_oku(&cerceve[4]);
    hesaplanan = elbari_crc32(&cerceve[8], cerceve_boyutu - 8);

    return (beklenen == hesaplanan) ? 1 : 0;
}

uint32_t elbari_cerceve_sira_no(const uint8_t *cerceve)
{
    if (cerceve == NULL)
    {
        return 0u;
    }
    return elbari_ic_u32_oku(&cerceve[8]);
}

int32_t elbari_cerceve_kayit_sayisi(const uint8_t *cerceve)
{
    if (cerceve == NULL)
    {
        return 0;
    }
    return elbari_ic_i32_oku(&cerceve[12]);
}

int32_t elbari_cerceve_oku(const uint8_t *cerceve,
                           int32_t        cerceve_boyutu,
                           int32_t        kanal_sayisi,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           int32_t       *cikti,
                           int32_t        cikti_kapasitesi,
                           uint32_t      *sira_no_cikti,
                           int32_t       *kayit_sayisi_cikti)
{
    uint32_t sira_no;
    int32_t  kayit_sayisi;
    int32_t  eleman_sayisi;
    int32_t  durum;

    if (sira_no_cikti != NULL)
    {
        *sira_no_cikti = 0u;
    }
    if (kayit_sayisi_cikti != NULL)
    {
        *kayit_sayisi_cikti = 0;
    }

    if ((cerceve == NULL) || (calisma_alani == NULL) || (cikti == NULL))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if ((kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL))
    {
        return ELBARI_HATA_PARAMETRE;
    }

    /* Bozuk/eksik paket sessizce kabul edilmez. */
    if (elbari_cerceve_gecerli_mi(cerceve, cerceve_boyutu) == 0)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    sira_no      = elbari_cerceve_sira_no(cerceve);
    kayit_sayisi = elbari_cerceve_kayit_sayisi(cerceve);

    if (kayit_sayisi < 0)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    eleman_sayisi = kayit_sayisi * kanal_sayisi;
    if (eleman_sayisi > cikti_kapasitesi)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    if (sira_no_cikti != NULL)
    {
        *sira_no_cikti = sira_no;
    }
    if (kayit_sayisi_cikti != NULL)
    {
        *kayit_sayisi_cikti = kayit_sayisi;
    }

    if (eleman_sayisi == 0)
    {
        return ELBARI_TAMAM;
    }

    durum = elbari_kanal_basit(&cerceve[ELBARI_CERCEVE_BASLIK_BOYUTU],
                               cerceve_boyutu - ELBARI_CERCEVE_BASLIK_BOYUTU,
                               calisma_alani, calisma_kapasitesi,
                               cikti, eleman_sayisi);
    if (durum != ELBARI_TAMAM)
    {
        /* CRC gecmis olsa bile yuk tutarsizsa savunmaci davran. */
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    return ELBARI_TAMAM;
}
