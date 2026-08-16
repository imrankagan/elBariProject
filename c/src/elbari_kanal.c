/* =====================================================================
 * ELBARI - Kanal katmani (cok kanalli telemetri)
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * ---------------------------------------------------------------------
 * NEDEN VAR:
 * Gercek telemetri tek bir sayi akisi degil, bir KAYIT akisidir:
 *   [enlem, boylam, irtifa, ... , enlem, boylam, irtifa, ...]
 * Bu akis oldugu gibi cekirdege verilirse ardisik farklar kanallar
 * arasinda ziplar (enlem->boylam farki milyonlarca birim olur), aykiri
 * oran %100'e cikar ve veri "sikistirilamaz" diye reddedilir.
 *
 * Bu katman akisi once kanallara ayirir, her kanali KENDI ICINDE
 * sikistirir, sonra tekrar birlestirir.
 *
 * IKINCI DERECE FARK:
 * Sabit hizla ilerleyen bir kanalda (GPS enlemi gibi) ardisik farklar
 * neredeyse sabittir. Bu kanal once kendi fark akisina cevrilirse
 * cekirdek iceride bir kez daha fark aldigi icin net etki ikinci
 * derece fark olur ve degerler cok kuculur. Gurultulu kanallarda ise
 * ikinci derece fark varyansi BUYUTUR; bu yuzden karar kanal basina,
 * veriye bakilarak verilir.
 *
 * VERI KAYBI YOK:
 * Bir kanal sikistirilamazsa HAM yazilir ve bayragi isaretlenir.
 * Kayipsizlik her kosulda korunur.
 *
 * BICIM (bayt duzeni):
 *   [0]                : kanal sayisi K            (1 bayt)
 *   [1]                : bayrak bayt sayisi B      (1 bayt, B = ceil(K/8))
 *   [2 .. 2+B)         : ikinci-derece bayraklari  (kanal basina 1 bit)
 *   [2+B .. 2+2B)      : ham-gecis bayraklari      (kanal basina 1 bit)
 *   [2+2B .. 2+2B+4K)  : kanal basina yuk boyutu   (int32)
 *   sonrasi            : kanal yukleri, sirayla
 * ===================================================================== */

#include "elbari.h"
#include "elbari_ic.h"

/** Ikinci derece karar verirken incelenecek en fazla eleman sayisi. */
#define ELBARI_KANAL_ORNEKLEM (512)

/* ---------------------------------------------------------------------
 * BOYUT HESAPLARI
 * ------------------------------------------------------------------- */

int32_t elbari_kanal_en_kotu_durum_boyutu(int32_t eleman_sayisi,
                                          int32_t kanal_sayisi)
{
    int32_t bayrak_bayt;
    int32_t baslik;

    if ((kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL) ||
        (eleman_sayisi < 0) || (eleman_sayisi > ELBARI_MAKS_ELEMAN))
    {
        return ELBARI_HATA_PARAMETRE;
    }

    bayrak_bayt = (kanal_sayisi + 7) / 8;
    baslik = 2 + (2 * bayrak_bayt) + (kanal_sayisi * 4);

    /* eleman*4 (ham) + eleman/2 (paketleme payi) + kanal basina referans/pay */
    return baslik
         + (eleman_sayisi * 4)
         + (eleman_sayisi / 2)
         + (kanal_sayisi * 68)
         + 64;
}

int32_t elbari_kanal_gerekli_calisma_alani(int32_t eleman_sayisi,
                                           int32_t kanal_sayisi)
{
    if (kanal_sayisi <= 0)
    {
        return 0;
    }
    return (eleman_sayisi + kanal_sayisi - 1) / kanal_sayisi;
}

/** c numarali kanalin kac eleman icerdigi (eksik kayitlara toleransli). */
static int32_t elbari_ic_kanal_uzunlugu(int32_t toplam,
                                        int32_t kanal_sayisi,
                                        int32_t c)
{
    if (c >= toplam)
    {
        return 0;
    }
    return (toplam - c + kanal_sayisi - 1) / kanal_sayisi;
}

/* ---------------------------------------------------------------------
 * IKINCI DERECE KARARI
 * ---------------------------------------------------------------------
 * Ardisik farklarin toplam mutlak buyuklugu ile farklarin farkinin
 * toplam mutlak buyuklugu karsilastirilir; kucuk olan kazanir.
 * Duzgun/dogrusal sinyallerde ikinci derece, gurultulu sinyallerde
 * birinci derece kazanir.
 * ------------------------------------------------------------------- */
static int32_t elbari_ic_ikinci_derece_daha_iyi_mi(const int32_t *kanal,
                                                   int32_t        uzunluk)
{
    int32_t ornek;
    int64_t birinci_toplam = 0;
    int64_t ikinci_toplam = 0;
    int32_t onceki_fark;
    int32_t i;

    if (uzunluk < 3)
    {
        return 0;
    }

    ornek = (uzunluk < ELBARI_KANAL_ORNEKLEM) ? uzunluk : ELBARI_KANAL_ORNEKLEM;

    onceki_fark = elbari_ic_fark(kanal[1], kanal[0]);
    birinci_toplam += elbari_ic_mutlak64(onceki_fark);

    for (i = 2; i < ornek; i++)
    {
        int32_t fark = elbari_ic_fark(kanal[i], kanal[i - 1]);
        int32_t ikinci = elbari_ic_fark(fark, onceki_fark);

        birinci_toplam += elbari_ic_mutlak64(fark);
        ikinci_toplam  += elbari_ic_mutlak64(ikinci);

        onceki_fark = fark;
    }

    return (ikinci_toplam < birinci_toplam) ? 1 : 0;
}

/* ---------------------------------------------------------------------
 * KODLAYICI
 * ------------------------------------------------------------------- */

int32_t elbari_kanal_kabid(const int32_t *ham_veri,
                           int32_t        eleman_sayisi,
                           int32_t        kanal_sayisi,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           uint8_t       *cikti,
                           int32_t        cikti_kapasitesi)
{
    int32_t bayrak_bayt;
    int32_t baslik_boyu;
    int32_t gerekli_calisma;
    int32_t yazma_konumu;
    int32_t c;
    uint8_t *ikinci_derece_bayraklari;
    uint8_t *ham_gecis_bayraklari;
    uint8_t *boyut_alani;
    int32_t i;

    if ((ham_veri == NULL) || (calisma_alani == NULL) || (cikti == NULL) ||
        (eleman_sayisi < 0) || (eleman_sayisi > ELBARI_MAKS_ELEMAN))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if ((kanal_sayisi < 1) || (kanal_sayisi > ELBARI_MAKS_KANAL))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi == 0)
    {
        return 0;
    }

    gerekli_calisma = elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi);
    if (calisma_kapasitesi < gerekli_calisma)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    bayrak_bayt = (kanal_sayisi + 7) / 8;
    baslik_boyu = 2 + (2 * bayrak_bayt) + (kanal_sayisi * 4);
    if (cikti_kapasitesi < baslik_boyu)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    cikti[0] = (uint8_t)kanal_sayisi;
    cikti[1] = (uint8_t)bayrak_bayt;

    ikinci_derece_bayraklari = &cikti[2];
    ham_gecis_bayraklari     = &cikti[2 + bayrak_bayt];
    boyut_alani              = &cikti[2 + (2 * bayrak_bayt)];

    for (i = 0; i < (2 * bayrak_bayt) + (kanal_sayisi * 4); i++)
    {
        cikti[2 + i] = 0u;
    }

    yazma_konumu = baslik_boyu;

    for (c = 0; c < kanal_sayisi; c++)
    {
        int32_t uzunluk = elbari_ic_kanal_uzunlugu(eleman_sayisi, kanal_sayisi, c);
        int32_t ikinci_derece;
        int32_t ilk_deger = 0;
        int32_t yuk_uzunlugu;
        int32_t on_ek_boyu;
        int32_t ham_bayt;
        int32_t yuk_konumu;
        int32_t sonuc;
        int32_t kayitli_boyut;

        if (uzunluk == 0)
        {
            continue;
        }

        /* 1) Kanali topla (deinterleave) */
        for (i = 0; i < uzunluk; i++)
        {
            calisma_alani[i] = ham_veri[c + (i * kanal_sayisi)];
        }

        /* 2) Ikinci derece fark daha mi iyi?
         *
         * ONEMLI: Ikinci dereceye gecerken mutlak ilk deger akisin ICINDE
         * birakilmaz. Birakilsaydi akis [x0, d1, d2, ...] olurdu; x0 mutlak
         * (or. 1.7 milyar), d1 ise minik bir fark oldugu icin 0->1 gecisinde
         * YAPAY ve devasa bir sicrama olusurdu. Bu sicrama hem bir aykiri
         * deger harcar hem de hizli tarama istatistiklerini bozarak veriyi
         * gereksiz yere reddettirebilir. Bu yuzden ilk deger yukun basina
         * ayri bir alan olarak yazilir ve akista yalnizca farklar kalir. */
        ikinci_derece = ((uzunluk >= 3) &&
                         (elbari_ic_ikinci_derece_daha_iyi_mi(calisma_alani, uzunluk) != 0))
                        ? 1 : 0;

        if (ikinci_derece != 0)
        {
            ilk_deger = calisma_alani[0];
            /* Yerinde sola kaydirarak fark akisi uret: [d1, d2, ..., d(m-1)] */
            for (i = 0; i < (uzunluk - 1); i++)
            {
                calisma_alani[i] = elbari_ic_fark(calisma_alani[i + 1], calisma_alani[i]);
            }
            yuk_uzunlugu = uzunluk - 1;
            elbari_ic_bayrak_kur(ikinci_derece_bayraklari, c);
        }
        else
        {
            yuk_uzunlugu = uzunluk;
        }

        on_ek_boyu = (ikinci_derece != 0) ? 4 : 0;
        ham_bayt = yuk_uzunlugu * 4;

        if ((yazma_konumu + on_ek_boyu) > cikti_kapasitesi)
        {
            return ELBARI_HATA_TAMPON_KUCUK;
        }

        if (ikinci_derece != 0)
        {
            elbari_ic_i32_yaz(&cikti[yazma_konumu], ilk_deger);
        }

        yuk_konumu = yazma_konumu + on_ek_boyu;

        /* 3) Sikistirmayi dene */
        sonuc = ELBARI_SIKISTIRILAMAZ;
        if ((yuk_uzunlugu > 0) && ((cikti_kapasitesi - yuk_konumu) >= (ham_bayt + 64)))
        {
            sonuc = elbari_kabid(calisma_alani, yuk_uzunlugu,
                                 &cikti[yuk_konumu], cikti_kapasitesi - yuk_konumu);
        }

        if ((sonuc > 0) && (sonuc < ham_bayt))
        {
            /* Sikistirma kazancli */
            kayitli_boyut = on_ek_boyu + sonuc;
            elbari_ic_i32_yaz(&boyut_alani[c * 4], kayitli_boyut);
            yazma_konumu = yuk_konumu + sonuc;
        }
        else
        {
            /* 4) HAM GECIS: kazanc yok ya da reddedildi -> veriyi ham yaz.
             *    Kayipsizlik her kosulda korunur. */
            if ((yuk_konumu + ham_bayt) > cikti_kapasitesi)
            {
                return ELBARI_HATA_TAMPON_KUCUK;
            }

            for (i = 0; i < yuk_uzunlugu; i++)
            {
                elbari_ic_i32_yaz(&cikti[yuk_konumu + (i * 4)], calisma_alani[i]);
            }

            kayitli_boyut = on_ek_boyu + ham_bayt;
            elbari_ic_i32_yaz(&boyut_alani[c * 4], kayitli_boyut);
            elbari_ic_bayrak_kur(ham_gecis_bayraklari, c);
            yazma_konumu = yuk_konumu + ham_bayt;
        }
    }

    return yazma_konumu;
}

/* ---------------------------------------------------------------------
 * COZUCU
 * ------------------------------------------------------------------- */

int32_t elbari_kanal_basit(const uint8_t *girdi,
                           int32_t        girdi_boyutu,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           int32_t       *cikti,
                           int32_t        eleman_sayisi)
{
    int32_t kanal_sayisi;
    int32_t bayrak_bayt;
    int32_t baslik_boyu;
    int32_t gerekli_calisma;
    int32_t okuma_konumu;
    int32_t c;
    int32_t i;
    const uint8_t *ikinci_derece_bayraklari;
    const uint8_t *ham_gecis_bayraklari;
    const uint8_t *boyut_alani;

    if ((girdi == NULL) || (calisma_alani == NULL) || (cikti == NULL) ||
        (eleman_sayisi < 0) || (eleman_sayisi > ELBARI_MAKS_ELEMAN))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi == 0)
    {
        return ELBARI_TAMAM;
    }
    if (girdi_boyutu < 2)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    kanal_sayisi = (int32_t)girdi[0];
    bayrak_bayt  = (int32_t)girdi[1];

    if ((kanal_sayisi < 1) || (bayrak_bayt != ((kanal_sayisi + 7) / 8)))
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    baslik_boyu = 2 + (2 * bayrak_bayt) + (kanal_sayisi * 4);
    if (girdi_boyutu < baslik_boyu)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    gerekli_calisma = elbari_kanal_gerekli_calisma_alani(eleman_sayisi, kanal_sayisi);
    if (calisma_kapasitesi < gerekli_calisma)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    ikinci_derece_bayraklari = &girdi[2];
    ham_gecis_bayraklari     = &girdi[2 + bayrak_bayt];
    boyut_alani              = &girdi[2 + (2 * bayrak_bayt)];

    okuma_konumu = baslik_boyu;

    for (c = 0; c < kanal_sayisi; c++)
    {
        int32_t uzunluk = elbari_ic_kanal_uzunlugu(eleman_sayisi, kanal_sayisi, c);
        int32_t yuk_boyutu;
        int32_t ham_gecis;
        int32_t ikinci_derece;
        int32_t on_ek_boyu;
        int32_t ilk_deger = 0;
        int32_t yuk_konumu;
        int32_t ic_boyut;
        int32_t hedef_uzunluk;

        if (uzunluk == 0)
        {
            continue;
        }

        yuk_boyutu = elbari_ic_i32_oku(&boyut_alani[c * 4]);
        if ((yuk_boyutu <= 0) || ((okuma_konumu + yuk_boyutu) > girdi_boyutu))
        {
            return ELBARI_HATA_BOZUK_GIRDI;
        }

        ham_gecis     = elbari_ic_bayrak_var_mi(ham_gecis_bayraklari, c);
        ikinci_derece = elbari_ic_bayrak_var_mi(ikinci_derece_bayraklari, c);

        /* Ikinci derece kanallarda yukun basinda mutlak ilk deger bulunur. */
        on_ek_boyu = (ikinci_derece != 0) ? 4 : 0;
        if (yuk_boyutu < on_ek_boyu)
        {
            return ELBARI_HATA_BOZUK_GIRDI;
        }

        if (ikinci_derece != 0)
        {
            ilk_deger = elbari_ic_i32_oku(&girdi[okuma_konumu]);
        }

        yuk_konumu    = okuma_konumu + on_ek_boyu;
        ic_boyut      = yuk_boyutu - on_ek_boyu;
        hedef_uzunluk = (ikinci_derece != 0) ? (uzunluk - 1) : uzunluk;

        if (hedef_uzunluk > 0)
        {
            if (ham_gecis != 0)
            {
                if (ic_boyut < (hedef_uzunluk * 4))
                {
                    return ELBARI_HATA_BOZUK_GIRDI;
                }
                for (i = 0; i < hedef_uzunluk; i++)
                {
                    calisma_alani[i] = elbari_ic_i32_oku(&girdi[yuk_konumu + (i * 4)]);
                }
            }
            else
            {
                int32_t durum = elbari_basit(&girdi[yuk_konumu], ic_boyut,
                                             calisma_alani, hedef_uzunluk);
                if (durum != ELBARI_TAMAM)
                {
                    return durum;
                }
            }
        }

        okuma_konumu += yuk_boyutu;

        if (ikinci_derece != 0)
        {
            /* Fark akisini saga kaydirip mutlak ilk degeri basa koy,
             * sonra onek toplam al. */
            for (i = uzunluk - 1; i >= 1; i--)
            {
                calisma_alani[i] = calisma_alani[i - 1];
            }
            calisma_alani[0] = ilk_deger;

            for (i = 1; i < uzunluk; i++)
            {
                calisma_alani[i] = elbari_ic_topla(calisma_alani[i], calisma_alani[i - 1]);
            }
        }

        /* Kanali ic ice duzene geri yerlestir */
        for (i = 0; i < uzunluk; i++)
        {
            cikti[c + (i * kanal_sayisi)] = calisma_alani[i];
        }
    }

    return ELBARI_TAMAM;
}
