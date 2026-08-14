/* =====================================================================
 * ELBARI - Kayipsiz float sikistirma (XOR tabanli)
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * ---------------------------------------------------------------------
 * NEDEN VAR:
 * Kuantalama katmani (elbari_float.c) KAYIPLIDIR. Tam degerin korunmasi
 * gereken durumlar icin (ham sensor kaydi, ucus sonrasi analiz, adli
 * inceleme, kriptografik malzeme) kayipsiz bir yol gerekir.
 *
 * NASIL CALISIR:
 * Ardisik iki float'in BIT DESENI XOR'lanir. Birbirine yakin degerlerde
 * isaret, ustel kisim ve mantisin ust bitleri aynidir; dolayisiyla XOR
 * sonucunun basinda ve sonunda cok sayida sifir bulunur. Yalnizca
 * ortadaki "anlamli" bitler yazilir.
 *
 * Ozel durum: deger hic degismemisse XOR sifirdir ve tek bir bit yeter.
 * Duragan telemetride (batarya, sabit irtifa) bu cok sik gorulur.
 *
 * Bu yaklasim literaturde Gorilla (Facebook, 2015) ve Chimp olarak
 * bilinir. Buradaki uygulama float32 icin uyarlanmistir.
 *
 * ---------------------------------------------------------------------
 * BEKLENTI YONETIMI - DURUST UYARI
 * ---------------------------------------------------------------------
 * Kayipsiz float sikistirma, GURULTULU sensor verisinde az kazandirir.
 * Sebep basit: gurultu mantisin alt bitlerini her orneklemde degistirir
 * ve bu bitler tanimlari geregi sikistirilamaz. Tipik kazanc %10-40
 * bandindadir.
 *
 * Buna karsilik KUANTALAMA ayni veride kat kat iyi sonuc verir, cunku
 * gurultuyu bastan atar. Olcum icin README'ye bakiniz.
 *
 * KURAL: Tam deger gerekmiyorsa kuantalama kullanin. Bu katman
 * "mecbur kalinca" icindir.
 * ---------------------------------------------------------------------
 *
 * BIT BICIMI (bit akisi, dusuk bitten yukseye)
 *
 *   Ilk deger : 32 bit ham (bit deseni oldugu gibi)
 *
 *   Sonraki her deger icin:
 *     XOR == 0  ->  1 bit: 0
 *     XOR != 0  ->  1 bit: 1, ardindan:
 *         onceki pencere kullanilabiliyorsa (BS >= oncekiBS ve
 *         SS >= oncekiSS):
 *             1 bit: 0
 *             (32 - oncekiBS - oncekiSS) bit: anlamli bitler
 *         aksi halde:
 *             1 bit: 1
 *             5 bit: BS  (bastaki sifir sayisi, 0..31)
 *             5 bit: anlamli_uzunluk - 1  (0..31 => 1..32)
 *             anlamli_uzunluk bit: anlamli bitler
 *
 *   BS = bastaki sifir sayisi, SS = sondaki sifir sayisi
 *   anlamli_uzunluk = 32 - BS - SS
 * ===================================================================== */

#include "elbari.h"
#include "elbari_ic.h"

/* ---------------------------------------------------------------------
 * BIT SAYMA (tasinabilir; derleyiciye ozel komut kullanilmaz)
 * ------------------------------------------------------------------- */

/** Bastaki (en anlamli taraftaki) sifir sayisi. Sifir icin 32 doner. */
static int32_t elbari_ic_basta_sifir(uint32_t deger)
{
    int32_t n = 0;
    uint32_t maske = 0x80000000u;

    if (deger == 0u)
    {
        return 32;
    }
    while ((deger & maske) == 0u)
    {
        n++;
        maske >>= 1;
    }
    return n;
}

/** Sondaki (en az anlamli taraftaki) sifir sayisi. Sifir icin 32 doner. */
static int32_t elbari_ic_sonda_sifir(uint32_t deger)
{
    int32_t n = 0;

    if (deger == 0u)
    {
        return 32;
    }
    while ((deger & 1u) == 0u)
    {
        n++;
        deger >>= 1;
    }
    return n;
}

/* ---------------------------------------------------------------------
 * BIT YAZICI
 * ------------------------------------------------------------------- */

typedef struct
{
    uint8_t *tampon;
    int32_t  kapasite;
    int32_t  bayt_indeksi;
    uint64_t bit_tamponu;
    int32_t  bit_sayisi;
    int32_t  tasti;        /* 1 ise kapasite yetmedi */
} elbari_bit_yazici;

static void elbari_ic_yazici_kur(elbari_bit_yazici *y, uint8_t *tampon, int32_t kapasite)
{
    y->tampon = tampon;
    y->kapasite = kapasite;
    y->bayt_indeksi = 0;
    y->bit_tamponu = 0u;
    y->bit_sayisi = 0;
    y->tasti = 0;
}

static void elbari_ic_yazici_bosalt(elbari_bit_yazici *y)
{
    while (y->bit_sayisi >= 8)
    {
        if (y->bayt_indeksi >= y->kapasite)
        {
            y->tasti = 1;
            return;
        }
        y->tampon[y->bayt_indeksi] = (uint8_t)(y->bit_tamponu & 0xFFu);
        y->bayt_indeksi++;
        y->bit_tamponu >>= 8;
        y->bit_sayisi -= 8;
    }
}

/** deger'in dusuk 'adet' bitini akisa yazar (dusuk bitten yukseye). */
static void elbari_ic_bit_yaz(elbari_bit_yazici *y, uint32_t deger, int32_t adet)
{
    uint32_t maske;

    if (y->tasti != 0)
    {
        return;
    }
    if (adet <= 0)
    {
        return;
    }

    maske = (adet >= 32) ? 0xFFFFFFFFu : ((1u << (unsigned int)adet) - 1u);
    y->bit_tamponu |= ((uint64_t)(deger & maske)) << (unsigned int)y->bit_sayisi;
    y->bit_sayisi += adet;

    elbari_ic_yazici_bosalt(y);
}

/** Kalan bitleri son bir bayta yazar. Donus: toplam bayt, hata: -1 */
static int32_t elbari_ic_yazici_kapat(elbari_bit_yazici *y)
{
    if (y->tasti != 0)
    {
        return -1;
    }
    if (y->bit_sayisi > 0)
    {
        if (y->bayt_indeksi >= y->kapasite)
        {
            return -1;
        }
        y->tampon[y->bayt_indeksi] = (uint8_t)(y->bit_tamponu & 0xFFu);
        y->bayt_indeksi++;
    }
    return y->bayt_indeksi;
}

/* ---------------------------------------------------------------------
 * BIT OKUYUCU
 * ------------------------------------------------------------------- */

typedef struct
{
    const uint8_t *tampon;
    int32_t        boyut;
    int32_t        bayt_indeksi;
    uint64_t       bit_tamponu;
    int32_t        bit_sayisi;
    int32_t        bitti;      /* 1 ise girdi tukendi */
} elbari_bit_okuyucu;

static void elbari_ic_okuyucu_kur(elbari_bit_okuyucu *o, const uint8_t *tampon, int32_t boyut)
{
    o->tampon = tampon;
    o->boyut = boyut;
    o->bayt_indeksi = 0;
    o->bit_tamponu = 0u;
    o->bit_sayisi = 0;
    o->bitti = 0;
}

/** Akistan 'adet' bit okur. Girdi tukendiyse bitti bayragi set edilir. */
static uint32_t elbari_ic_bit_oku(elbari_bit_okuyucu *o, int32_t adet)
{
    uint32_t maske;
    uint32_t sonuc;

    if ((o->bitti != 0) || (adet <= 0))
    {
        return 0u;
    }

    while (o->bit_sayisi < adet)
    {
        if (o->bayt_indeksi >= o->boyut)
        {
            o->bitti = 1;
            return 0u;
        }
        o->bit_tamponu |= ((uint64_t)o->tampon[o->bayt_indeksi]) << (unsigned int)o->bit_sayisi;
        o->bayt_indeksi++;
        o->bit_sayisi += 8;
    }

    maske = (adet >= 32) ? 0xFFFFFFFFu : ((1u << (unsigned int)adet) - 1u);
    sonuc = (uint32_t)(o->bit_tamponu & (uint64_t)maske);
    o->bit_tamponu >>= (unsigned int)adet;
    o->bit_sayisi -= adet;

    return sonuc;
}

/* ---------------------------------------------------------------------
 * BOYUT HESABI
 * ------------------------------------------------------------------- */

int32_t elbari_float_xor_en_kotu_durum_boyutu(int32_t adet)
{
    if (adet <= 0)
    {
        return 8;
    }
    if (adet > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    /* En kotu durumda deger basina 1 + 1 + 5 + 5 + 32 = 44 bit = 5.5 bayt.
     * Guvenli tarafta kalmak icin 6 bayt + pay ile hesaplanir. */
    return (adet * 6) + 16;
}

/* ---------------------------------------------------------------------
 * KODLAYICI
 * ------------------------------------------------------------------- */

int32_t elbari_float_xor_kabid(const float *ham_veri,
                               int32_t      adet,
                               uint8_t     *cikti,
                               int32_t      cikti_kapasitesi)
{
    elbari_bit_yazici y;
    uint32_t onceki_bit;
    int32_t  onceki_bs = 32;   /* 32 = "gecerli pencere yok" */
    int32_t  onceki_ss = 32;
    int32_t  i;

    if ((ham_veri == NULL) || (cikti == NULL) || (adet < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (adet > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (adet == 0)
    {
        return 0;
    }

    elbari_ic_yazici_kur(&y, cikti, cikti_kapasitesi);

    /* Ilk deger ham yazilir */
    (void)memcpy(&onceki_bit, &ham_veri[0], sizeof(onceki_bit));
    elbari_ic_bit_yaz(&y, onceki_bit, 32);

    for (i = 1; i < adet; i++)
    {
        uint32_t simdiki_bit;
        uint32_t fark;

        (void)memcpy(&simdiki_bit, &ham_veri[i], sizeof(simdiki_bit));
        fark = simdiki_bit ^ onceki_bit;

        if (fark == 0u)
        {
            /* Deger degismemis: tek bit yeter */
            elbari_ic_bit_yaz(&y, 0u, 1);
        }
        else
        {
            int32_t bs = elbari_ic_basta_sifir(fark);
            int32_t ss = elbari_ic_sonda_sifir(fark);

            elbari_ic_bit_yaz(&y, 1u, 1);

            if ((bs >= onceki_bs) && (ss >= onceki_ss))
            {
                /* Onceki pencere yeterli: uzunluk bilgisi tekrar yazilmaz */
                int32_t uzunluk = 32 - onceki_bs - onceki_ss;

                elbari_ic_bit_yaz(&y, 0u, 1);
                elbari_ic_bit_yaz(&y, fark >> (unsigned int)onceki_ss, uzunluk);
            }
            else
            {
                int32_t uzunluk = 32 - bs - ss;

                elbari_ic_bit_yaz(&y, 1u, 1);
                elbari_ic_bit_yaz(&y, (uint32_t)bs, 5);
                elbari_ic_bit_yaz(&y, (uint32_t)(uzunluk - 1), 5);
                elbari_ic_bit_yaz(&y, fark >> (unsigned int)ss, uzunluk);

                onceki_bs = bs;
                onceki_ss = ss;
            }
        }

        onceki_bit = simdiki_bit;
    }

    {
        int32_t toplam = elbari_ic_yazici_kapat(&y);

        if (toplam < 0)
        {
            return ELBARI_HATA_TAMPON_KUCUK;
        }
        return toplam;
    }
}

/* ---------------------------------------------------------------------
 * COZUCU
 * ------------------------------------------------------------------- */

int32_t elbari_float_xor_basit(const uint8_t *girdi,
                               int32_t        girdi_boyutu,
                               float         *cikti,
                               int32_t        adet)
{
    elbari_bit_okuyucu o;
    uint32_t onceki_bit;
    int32_t  onceki_bs = 32;
    int32_t  onceki_ss = 32;
    int32_t  i;

    if ((girdi == NULL) || (cikti == NULL) || (adet < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (adet > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (adet == 0)
    {
        return ELBARI_TAMAM;
    }
    if (girdi_boyutu < 4)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    elbari_ic_okuyucu_kur(&o, girdi, girdi_boyutu);

    onceki_bit = elbari_ic_bit_oku(&o, 32);
    if (o.bitti != 0)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }
    (void)memcpy(&cikti[0], &onceki_bit, sizeof(float));

    for (i = 1; i < adet; i++)
    {
        uint32_t bayrak = elbari_ic_bit_oku(&o, 1);
        uint32_t simdiki_bit;

        if (o.bitti != 0)
        {
            return ELBARI_HATA_BOZUK_GIRDI;
        }

        if (bayrak == 0u)
        {
            simdiki_bit = onceki_bit;
        }
        else
        {
            uint32_t pencere = elbari_ic_bit_oku(&o, 1);
            uint32_t anlamli;
            int32_t  uzunluk;

            if (o.bitti != 0)
            {
                return ELBARI_HATA_BOZUK_GIRDI;
            }

            if (pencere == 0u)
            {
                if ((onceki_bs >= 32) || (onceki_ss >= 32))
                {
                    /* Gecerli pencere yokken pencere tekrari istenmis:
                     * girdi bozuk. */
                    return ELBARI_HATA_BOZUK_GIRDI;
                }
                uzunluk = 32 - onceki_bs - onceki_ss;
                anlamli = elbari_ic_bit_oku(&o, uzunluk);
                simdiki_bit = onceki_bit ^ (anlamli << (unsigned int)onceki_ss);
            }
            else
            {
                int32_t bs = (int32_t)elbari_ic_bit_oku(&o, 5);
                int32_t uz = (int32_t)elbari_ic_bit_oku(&o, 5) + 1;
                int32_t ss;

                if (o.bitti != 0)
                {
                    return ELBARI_HATA_BOZUK_GIRDI;
                }

                ss = 32 - bs - uz;
                if ((ss < 0) || (bs > 31))
                {
                    return ELBARI_HATA_BOZUK_GIRDI;
                }

                anlamli = elbari_ic_bit_oku(&o, uz);
                simdiki_bit = onceki_bit ^ (anlamli << (unsigned int)ss);

                onceki_bs = bs;
                onceki_ss = ss;
            }

            if (o.bitti != 0)
            {
                return ELBARI_HATA_BOZUK_GIRDI;
            }
        }

        (void)memcpy(&cikti[i], &simdiki_bit, sizeof(float));
        onceki_bit = simdiki_bit;
    }

    return ELBARI_TAMAM;
}

/* ---------------------------------------------------------------------
 * COK KANALLI SARMALAYICI
 * ---------------------------------------------------------------------
 * Ic ice gecmis float akisini kanallara ayirip her kanali kendi icinde
 * XOR ile sikistirir. Ayni kanalin ardisik degerleri birbirine benzer;
 * karisik akista ise her adimda bambaska bir buyukluge geciyor olurduk
 * ve XOR neredeyse hic sifir uretmezdi.
 *
 * BICIM:
 *   [0]              : kanal sayisi K (1 bayt)
 *   [1]              : bayrak bayt sayisi B = ceil(K/8)
 *   [2 .. 2+B)       : ham-gecis bayraklari (kanal basina 1 bit)
 *   [2+B .. 2+B+4K)  : kanal basina yuk boyutu (int32, little-endian)
 *   sonrasi          : kanal yukleri, sirayla
 * ------------------------------------------------------------------- */

int32_t elbari_float_xor_kanal_en_kotu_durum_boyutu(int32_t eleman_sayisi,
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
    baslik = 2 + bayrak_bayt + (kanal_sayisi * 4);

    return baslik + (eleman_sayisi * 6) + (kanal_sayisi * 16) + 64;
}

int32_t elbari_float_xor_kanal_kabid(const float *ham_veri,
                                     int32_t      eleman_sayisi,
                                     int32_t      kanal_sayisi,
                                     float       *calisma_alani,
                                     int32_t      calisma_kapasitesi,
                                     uint8_t     *cikti,
                                     int32_t      cikti_kapasitesi)
{
    int32_t bayrak_bayt;
    int32_t baslik_boyu;
    int32_t yazma_konumu;
    int32_t c;
    int32_t i;
    uint8_t *ham_bayraklari;
    uint8_t *boyut_alani;

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
    if (calisma_kapasitesi < ((eleman_sayisi + kanal_sayisi - 1) / kanal_sayisi))
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    bayrak_bayt = (kanal_sayisi + 7) / 8;
    baslik_boyu = 2 + bayrak_bayt + (kanal_sayisi * 4);
    if (cikti_kapasitesi < baslik_boyu)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    cikti[0] = (uint8_t)kanal_sayisi;
    cikti[1] = (uint8_t)bayrak_bayt;
    ham_bayraklari = &cikti[2];
    boyut_alani = &cikti[2 + bayrak_bayt];

    for (i = 0; i < (bayrak_bayt + (kanal_sayisi * 4)); i++)
    {
        cikti[2 + i] = 0u;
    }

    yazma_konumu = baslik_boyu;

    for (c = 0; c < kanal_sayisi; c++)
    {
        int32_t uzunluk;
        int32_t ham_bayt;
        int32_t sonuc;

        if (c >= eleman_sayisi)
        {
            continue;
        }
        uzunluk = (eleman_sayisi - c + kanal_sayisi - 1) / kanal_sayisi;

        for (i = 0; i < uzunluk; i++)
        {
            calisma_alani[i] = ham_veri[c + (i * kanal_sayisi)];
        }

        ham_bayt = uzunluk * 4;

        sonuc = ELBARI_HATA_TAMPON_KUCUK;
        if ((cikti_kapasitesi - yazma_konumu) >= (ham_bayt + 64))
        {
            sonuc = elbari_float_xor_kabid(calisma_alani, uzunluk,
                                           &cikti[yazma_konumu],
                                           cikti_kapasitesi - yazma_konumu);
        }

        if ((sonuc > 0) && (sonuc < ham_bayt))
        {
            elbari_ic_i32_yaz(&boyut_alani[c * 4], sonuc);
            yazma_konumu += sonuc;
        }
        else
        {
            /* Kazanc yok: ham yaz (kayipsizlik her kosulda korunur) */
            if ((yazma_konumu + ham_bayt) > cikti_kapasitesi)
            {
                return ELBARI_HATA_TAMPON_KUCUK;
            }
            for (i = 0; i < uzunluk; i++)
            {
                uint32_t bit_deseni;
                (void)memcpy(&bit_deseni, &calisma_alani[i], sizeof(bit_deseni));
                elbari_ic_u32_yaz(&cikti[yazma_konumu + (i * 4)], bit_deseni);
            }
            elbari_ic_i32_yaz(&boyut_alani[c * 4], ham_bayt);
            ham_bayraklari[c >> 3] |= (uint8_t)(1u << (unsigned int)(c & 7));
            yazma_konumu += ham_bayt;
        }
    }

    return yazma_konumu;
}

int32_t elbari_float_xor_kanal_basit(const uint8_t *girdi,
                                     int32_t        girdi_boyutu,
                                     float         *calisma_alani,
                                     int32_t        calisma_kapasitesi,
                                     float         *cikti,
                                     int32_t        eleman_sayisi)
{
    int32_t kanal_sayisi;
    int32_t bayrak_bayt;
    int32_t baslik_boyu;
    int32_t okuma_konumu;
    int32_t c;
    int32_t i;
    const uint8_t *ham_bayraklari;
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
    bayrak_bayt = (int32_t)girdi[1];

    if ((kanal_sayisi < 1) || (bayrak_bayt != ((kanal_sayisi + 7) / 8)))
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    baslik_boyu = 2 + bayrak_bayt + (kanal_sayisi * 4);
    if (girdi_boyutu < baslik_boyu)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }
    if (calisma_kapasitesi < ((eleman_sayisi + kanal_sayisi - 1) / kanal_sayisi))
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    ham_bayraklari = &girdi[2];
    boyut_alani = &girdi[2 + bayrak_bayt];
    okuma_konumu = baslik_boyu;

    for (c = 0; c < kanal_sayisi; c++)
    {
        int32_t uzunluk;
        int32_t yuk_boyutu;
        int32_t ham_gecis;

        if (c >= eleman_sayisi)
        {
            continue;
        }
        uzunluk = (eleman_sayisi - c + kanal_sayisi - 1) / kanal_sayisi;

        yuk_boyutu = elbari_ic_i32_oku(&boyut_alani[c * 4]);
        if ((yuk_boyutu <= 0) || ((okuma_konumu + yuk_boyutu) > girdi_boyutu))
        {
            return ELBARI_HATA_BOZUK_GIRDI;
        }

        ham_gecis = ((ham_bayraklari[c >> 3] &
                      (uint8_t)(1u << (unsigned int)(c & 7))) != 0u) ? 1 : 0;

        if (ham_gecis != 0)
        {
            if (yuk_boyutu < (uzunluk * 4))
            {
                return ELBARI_HATA_BOZUK_GIRDI;
            }
            for (i = 0; i < uzunluk; i++)
            {
                uint32_t bit_deseni = elbari_ic_u32_oku(&girdi[okuma_konumu + (i * 4)]);
                (void)memcpy(&calisma_alani[i], &bit_deseni, sizeof(float));
            }
        }
        else
        {
            int32_t durum = elbari_float_xor_basit(&girdi[okuma_konumu], yuk_boyutu,
                                                   calisma_alani, uzunluk);
            if (durum != ELBARI_TAMAM)
            {
                return durum;
            }
        }

        okuma_konumu += yuk_boyutu;

        for (i = 0; i < uzunluk; i++)
        {
            cikti[c + (i * kanal_sayisi)] = calisma_alani[i];
        }
    }

    return ELBARI_TAMAM;
}
