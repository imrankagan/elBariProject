/* =====================================================================
 * ELBARI - Cekirdek katman (kodlayici / cozucu)
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * ---------------------------------------------------------------------
 * ALGORITMA
 *   Ardisik elemanlar arasindaki fark (delta) alinir, farklar 8'li
 *   bloklar halinde gruplanir ve her blok icin gereken en kucuk bit
 *   genisligi secilir (1/2/3/4/5/8/10/16). Esigi asan buyuk sapmalar "aykiri
 *   deger" olarak isaretlenip ayrica 32 bit ile kodlanir.
 *
 *   Literaturdeki adi: PFOR-Delta (Frame of Reference + patching).
 *
 * BLOK BICIMI (bit akisi, dusuk bitten yuksege)
 *   4 bit  : etiket = (mod << 1) | aykiri_var
 *            mod: 0->1bit 1->2bit 2->3bit 3->4bit
                 4->5bit 5->8bit 6->10bit 7->16bit
 *   8 bit  : aykiri maske (yalnizca aykiri_var ise)
 *   n x w  : aykiri OLMAYAN farklar, blok bit genisligi w ile
 *   m x 32 : aykiri farklar, tam genislikte
 *
 * NOT: Bu surum saf skalerdir. .NET surumundeki AVX2/NEON yollari
 * yalnizca hizlandirmadir ve ayni bit akisini uretir; dolayisiyla
 * ciktilar birebir ayni olur.
 * ===================================================================== */

#include "elbari.h"
#include "elbari_ic.h"

/* ---------------------------------------------------------------------
 * IC SABITLER
 * ------------------------------------------------------------------- */

/* BIT GENISLIGI TABLOSU (bicim surumu 2)
 *
 * Etiketteki 'mod' alani 3 bittir, yani 8 deger tutabilir. Surum 1'de bunun
 * yalnizca 4'u kullaniliyordu (2/4/8/16); kalan 4 yuva bostaydi. Bu, 5 bit
 * gereken bir farkin 8 bitle, 10 bit gerekenin 16 bitle yazilmasi demekti.
 *
 * Bos yuvalara ara genislikler eklendi. ETIKET ALANI BUYUMEDI - yalnizca
 * zaten var olan bitler degerlendirildi.
 *
 * Kume, gercek veri uzerinde kaba kuvvet aramasiyla secildi: 16 zorunlu
 * (aykiri esigi 32767), kalan 7 yuva icin 1..15 arasindaki tum kombinasyonlar
 * denendi ve iki veri setinde birden en iyi sonucu veren kume alindi.
 * Olculen kazanc: gercek GPS %25.6, IHA telemetrisi %24.0. */
/* mod = 0 -> SIFIR BLOK: aykiri olmayan tum farklar sifirdir ve HIC veri
 * biti yazilmaz. Bu bir eniyileme degil, bilgi tasimayan bitlerin
 * kaldirilmasidir: mod 0 yalnizca M <= 0 iken secilir, yani aykiri olmayan
 * farklarin hepsi tam olarak sifirdir. Eskiden her biri icin garantili sifir
 * olan 1 bit yaziliyordu - sifir bilgi, sekiz bit.
 * Olculen kazanc: gercek GPS %5.4, IHA telemetrisi %3.5. */
#define ELBARI_GENISLIK_0               (0)    /* M <= 0, veri biti YOK */
#define ELBARI_GENISLIK_1               (2)    /* M <= 1   */
#define ELBARI_GENISLIK_2               (3)    /* M <= 3   */
#define ELBARI_GENISLIK_3               (4)    /* M <= 7   */
#define ELBARI_GENISLIK_4               (5)    /* M <= 15  */
#define ELBARI_GENISLIK_5               (8)    /* M <= 127 */
#define ELBARI_GENISLIK_6               (10)   /* M <= 511 */
#define ELBARI_GENISLIK_7               (16)   /* M <= 32767 */
#define ELBARI_ETIKET_MASKESI           (0x0F)

/* ---------------------------------------------------------------------
 * BLOK-USTU SIFIR KOSUSU  (bicim surumu 3)
 * ---------------------------------------------------------------------
 * Sifir blok (mod 0) veri biti yazmaz ama ETIKETINI yine yazar. Bu,
 * bicime deger basina 0,5 bitlik bir taban ve 32 bitlik degerler icin
 * 64x'lik sert bir TAVAN koyar - veri ne kadar sabit olursa olsun.
 *
 * Olculdu: gercek bir kumanda girisi (RCIN) kanalinda bloklarin %94,5'i
 * sifir blok ve bunlarin %93'u uzun kosular halinde. Yani ciktinin
 * ucte ikisi etiketten ibaret. Sprintz-Delta ayni veride 74x aliyordu -
 * yani ElBari'nin TEORIK TAVANININ ustunde.
 *
 * KACIS KODU - neden bu birlesim:
 *   Etiket "mod 0 + aykiri_var 1" ardindan 8 bitlik aykiri maskesi 0x00,
 *   surum 2 kodlayicisinda ULASILAMAZ bir birlesimdir: aykiri_var ancak
 *   maske sifir DEGILKEN kurulur. Bu yuzden maske 0x00 kacis isareti
 *   olarak kullanilabilir ve SURUM 2 AKISLARI SURUM 3 COZUCUSUNDE
 *   AYNEN calismaya devam eder. Yeni bir etiket degeri harcanmaz.
 *
 * MALIYET: kacis 4 + 8 + 16 = 28 bit. Duz kodlama kosu basina 4 bit.
 *          Basabas nokta 7 bloktur; bu yuzden esik 8'dir.
 * ------------------------------------------------------------------- */

/** Kacisin kazanmaya basladigi kosu uzunlugu (blok cinsinden). */
#define ELBARI_SIFIR_KOSU_ESIGI         (8)

/** 16 bitlik kosu sayacinin siniri. */
#define ELBARI_SIFIR_KOSU_MAKS          (65535)

/** Kosu sayacinin bit genisligi. */
#define ELBARI_SIFIR_KOSU_BITI          (16)
#define ELBARI_BAYT_MASKESI             (0xFFu)

/**
 * Erken iptal esigi. Ilk blok kumesi islendikten sonra sikistirma
 * orani bu degerin altindaysa islem iptal edilir; bu sayede
 * sikismayan veri icin bosuna CPU harcanmaz.
 */
#define ELBARI_ERKEN_IPTAL_ESIGI        (1.5f)

/**
 * Ornekleme sirasinda aykiri deger orani bu esigi asarsa veri
 * "gercek dunya verisi degil" kabul edilip reddedilir.
 */
#define ELBARI_MAKS_AYKIRI_ORANI        (0.30f)

/** Hizli tarama icin en fazla incelenecek eleman sayisi. */
#define ELBARI_HIZLI_TARAMA_ORNEKLEM    (1000)

/** Ortalama fark bu degeri asarsa veri rastgele kabul edilir (INT32_MAX / 4). */
#define ELBARI_ORTALAMA_FARK_SINIRI     ((int64_t)536870911)


/* ---------------------------------------------------------------------
 * BOYUT HESABI
 * ------------------------------------------------------------------- */

int32_t elbari_cekirdek_en_kotu_durum_boyutu(int32_t eleman_sayisi)
{
    int32_t sonuc;

    if (eleman_sayisi <= 0)
    {
        sonuc = ELBARI_REFERANS_BOYUTU;
    }
    else if (eleman_sayisi > ELBARI_MAKS_ELEMAN)
    {
        sonuc = ELBARI_HATA_PARAMETRE;
    }
    else
    {
        /* Referans + her eleman icin 4 bayt (aykiri durumu) + etiket/maske payi */
        sonuc = ELBARI_REFERANS_BOYUTU
              + (eleman_sayisi * 4)
              + (eleman_sayisi / 2)
              + 64;
    }
    return sonuc;
}

/* ---------------------------------------------------------------------
 * HIZLI TARAMA
 * ---------------------------------------------------------------------
 * Veriyi kabaca tarayarak sikistirmaya deger olup olmadigina bakar.
 * Amac, tamamen rastgele/anlamsiz veriyi erkenden reddedip CPU
 * harcamamaktir. Gercek dunya verisi (sensor, GPS, telemetri) duzgun
 * degisir; rastgele veri ise her adimda buyuk siciramalar yapar.
 * ------------------------------------------------------------------- */
static int32_t elbari_ic_sikistirilabilir_mi(const int32_t *ham_veri,
                                             int32_t        eleman_sayisi)
{
    int32_t  orneklem_boyutu;
    int64_t  fark_mutlak_toplam = 0;
    int32_t  aykiri_sayisi = 0;
    int32_t  maks_fark = 0;
    int32_t  i;
    float    aykiri_orani;
    int64_t  ortalama_fark;
    int32_t  bolen;
    int32_t  ornek_eksi_bir;

    if (eleman_sayisi < 2)
    {
        return 1; /* Cok kucuk veri: kabul et */
    }

    /* Ilk %10'u, en fazla ELBARI_HIZLI_TARAMA_ORNEKLEM eleman incelenir.
     * Cok kisa dizilerde orneklem tum diziye genisletilir. */
    orneklem_boyutu = eleman_sayisi / 10;
    if (orneklem_boyutu > ELBARI_HIZLI_TARAMA_ORNEKLEM)
    {
        orneklem_boyutu = ELBARI_HIZLI_TARAMA_ORNEKLEM;
    }
    if (orneklem_boyutu < 10)
    {
        orneklem_boyutu = (eleman_sayisi < 100) ? eleman_sayisi : 100;
    }

    for (i = 1; i < orneklem_boyutu; i++)
    {
        int32_t fark = elbari_ic_fark(ham_veri[i], ham_veri[i - 1]);
        int32_t mutlak_fark = elbari_ic_mutlak_deger(fark);

        fark_mutlak_toplam += (int64_t)mutlak_fark;

        if (mutlak_fark > ELBARI_AYKIRI_ESIK)
        {
            aykiri_sayisi++;
        }
        if (mutlak_fark > maks_fark)
        {
            maks_fark = mutlak_fark;
        }
    }

    bolen = orneklem_boyutu - 1;
    if (bolen < 1)
    {
        bolen = 1;
    }

    /* Olcut 1: aykiri oran cok yuksekse veri uygun degil */
    /* MISRA 10.8: bilesik ifade dogrudan donusturulmez; once degiskene alinir. */
    ornek_eksi_bir = orneklem_boyutu - 1;
    aykiri_orani = (float)aykiri_sayisi / (float)ornek_eksi_bir;
    if (aykiri_orani > ELBARI_MAKS_AYKIRI_ORANI)
    {
        return 0;
    }

    /* Olcut 2: ortalama fark cok buyukse veri rastgeledir */
    ortalama_fark = fark_mutlak_toplam / (int64_t)bolen;
    if (ortalama_fark > ELBARI_ORTALAMA_FARK_SINIRI)
    {
        return 0;
    }

    /* Olcut 3: hem cok buyuk fark hem kayda deger aykiri oran -> supheli */
    if ((maks_fark > (INT32_MAX / 2)) &&
        (aykiri_orani > (ELBARI_MAKS_AYKIRI_ORANI / 3.0f)))
    {
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------------------
 * BIT TAMPONU ISLEMLERI
 * ------------------------------------------------------------------- */

/**
 * Bit tamponundaki tam baytlari cikti dizisine bosaltir.
 * @return 1 basarili, 0 cikti tamponu doldu
 */
static int32_t elbari_ic_bit_bosalt(uint64_t *bit_tamponu,
                                    int32_t  *bit_sayisi,
                                    uint8_t  *cikti,
                                    int32_t   cikti_kapasitesi,
                                    int32_t  *bayt_indeksi)
{
    while (*bit_sayisi >= 8)
    {
        if (*bayt_indeksi >= cikti_kapasitesi)
        {
            return 0;
        }
        cikti[*bayt_indeksi] = (uint8_t)(*bit_tamponu & ELBARI_BAYT_MASKESI);
        (*bayt_indeksi)++;
        *bit_tamponu >>= 8;
        *bit_sayisi -= 8;
    }
    return 1;
}

/**
 * Bit tamponuna, istenen bit sayisi birikene kadar bayt yukler.
 * @return 1 basarili, 0 girdi tukendi
 */
static int32_t elbari_ic_bit_yukle(uint64_t      *bit_tamponu,
                                   int32_t       *bit_sayisi,
                                   const uint8_t *girdi,
                                   int32_t        girdi_boyutu,
                                   int32_t       *bayt_indeksi,
                                   int32_t        gerekli_bitler)
{
    while (*bit_sayisi < gerekli_bitler)
    {
        if (*bayt_indeksi >= girdi_boyutu)
        {
            return 0;
        }
        *bit_tamponu |= ((uint64_t)girdi[*bayt_indeksi]) << (unsigned int)(*bit_sayisi);
        (*bayt_indeksi)++;
        *bit_sayisi += 8;
    }
    return 1;
}

/**
 * ham_veri[i .. i+7] araligindaki TUM farklar sifir mi?
 * Cagiran, i+7'nin sinirlar icinde oldugunu garanti etmelidir.
 */
static int32_t elbari_ic_sifir_blok_mu(const int32_t *ham_veri, int32_t i)
{
    int32_t j;

    for (j = 0; j < ELBARI_BLOK_BOYUTU; j++)
    {
        if (elbari_ic_fark(ham_veri[i + j], ham_veri[(i + j) - 1]) != 0)
        {
            return 0;
        }
    }
    return 1;
}

/* ---------------------------------------------------------------------
 * KODLAYICI
 * ------------------------------------------------------------------- */

int32_t elbari_kabid(const int32_t *ham_veri,
                     int32_t        eleman_sayisi,
                     uint8_t       *cikti,
                     int32_t        cikti_kapasitesi)
{
    int32_t  bayt_indeksi;
    uint64_t bit_tamponu = 0u;
    int32_t  bit_sayisi = 0;
    int32_t  veri_indeksi = 1;
    int32_t  erken_iptal_bakildi = 0;
    int32_t  islenen_bayt;
    uint32_t etiket_bitleri;
    int32_t  en_az_gereken;

    if ((ham_veri == NULL) || (cikti == NULL) || (eleman_sayisi < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    /* Boyut hesaplarinda 32 bit tasmasini onler (bkz. ELBARI_MAKS_ELEMAN). */
    if (eleman_sayisi > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi == 0)
    {
        return 0;
    }

    /* Gercek dunya verisi mi? Degilse bosuna ugrasma. */
    if (elbari_ic_sikistirilabilir_mi(ham_veri, eleman_sayisi) == 0)
    {
        return ELBARI_SIKISTIRILAMAZ;
    }

    /* En kotu durumda her eleman 32 bit ile kodlanabilir; tampon yetmeli. */
    en_az_gereken = ELBARI_REFERANS_BOYUTU + (eleman_sayisi * 4);
    if (cikti_kapasitesi < en_az_gereken)
    {
        return ELBARI_HATA_TAMPON_KUCUK;
    }

    /* Akisin basina mutlak referans deger yazilir. */
    elbari_ic_i32_yaz(cikti, ham_veri[0]);
    bayt_indeksi = ELBARI_REFERANS_BOYUTU;

    while (veri_indeksi < eleman_sayisi)
    {
        int32_t kalan = eleman_sayisi - veri_indeksi;
        int32_t blok_boyu = (kalan < ELBARI_BLOK_BOYUTU) ? kalan : ELBARI_BLOK_BOYUTU;
        int32_t maks_mutlak = 0;
        uint8_t aykiri_maske = 0u;
        int32_t aykiri_var;
        int32_t bit_genisligi;
        int32_t mod;
        int32_t etiket;
        uint32_t maske;
        int32_t j;

        /* 0) BLOK-USTU SIFIR KOSUSU (bicim surumu 3).
         *    Ileriye bakilir; yeterince uzun bir tam-sifir blok dizisi
         *    varsa hepsi TEK bir kacisla yazilir. Kisa kosularda kacis
         *    zarar edeceginden esik altinda duz kodlama surer. */
        if (blok_boyu == ELBARI_BLOK_BOYUTU)
        {
            int32_t kosu = 0;
            int32_t ileri = veri_indeksi;

            while (((eleman_sayisi - ileri) >= ELBARI_BLOK_BOYUTU) &&
                   (kosu < ELBARI_SIFIR_KOSU_MAKS) &&
                   (elbari_ic_sifir_blok_mu(ham_veri, ileri) != 0))
            {
                kosu++;
                ileri += ELBARI_BLOK_BOYUTU;
            }

            if (kosu >= ELBARI_SIFIR_KOSU_ESIGI)
            {
                /* Etiket: mod 0, aykiri_var 1 -> ham deger 1 */
                bit_tamponu |= ((uint64_t)1u) << (unsigned int)bit_sayisi;
                bit_sayisi += 4;
                if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                         cikti, cikti_kapasitesi,
                                         &bayt_indeksi) == 0)
                {
                    return ELBARI_HATA_TAMPON_KUCUK;
                }

                /* Aykiri maskesi 0x00: kacis isareti. Tamponun bu bitleri
                 * zaten sifirdir, yalnizca sayac ilerletilir. */
                bit_sayisi += 8;
                if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                         cikti, cikti_kapasitesi,
                                         &bayt_indeksi) == 0)
                {
                    return ELBARI_HATA_TAMPON_KUCUK;
                }

                /* Kosu uzunlugu (16 bit) */
                bit_tamponu |= ((uint64_t)(uint32_t)kosu)
                               << (unsigned int)bit_sayisi;
                bit_sayisi += ELBARI_SIFIR_KOSU_BITI;
                if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                         cikti, cikti_kapasitesi,
                                         &bayt_indeksi) == 0)
                {
                    return ELBARI_HATA_TAMPON_KUCUK;
                }

                veri_indeksi = ileri;
                continue;
            }
        }

        /* 1) Blok icindeki farklari incele: en buyuk mutlak deger ve
         *    aykiri deger maskesi belirlenir. */
        for (j = 0; j < blok_boyu; j++)
        {
            int32_t fark = elbari_ic_fark(ham_veri[veri_indeksi + j],
                                          ham_veri[veri_indeksi + j - 1]);
            /* Buyukluk 64 BITTE olculur. 32 bitlik mutlak deger INT32_MIN
             * icin negatif kalir (bkz. elbari_ic_mutlak_deger notu); o
             * durumda asagidaki iki karsilastirma da sessizce basarisiz
             * olur - fark ne aykiri isaretlenir ne de blok bit genisligini
             * yukseltir, sonra dar maskeyle paketlenip UST BITI KAYBEDER.
             * Tam olarak 2^31'lik bir fark, orn. isareti degisen ama
             * buyuklugu ayni kalan bir float bit deseni (+0.001 -> -0.001),
             * kayipsizligi bu yolla bozuyordu. Gercek ucus logundaki
             * jiroskop kanalinda olcumle yakalandi. */
            int64_t m = elbari_ic_mutlak64(fark);

            if (m > (int64_t)ELBARI_AYKIRI_ESIK)
            {
                aykiri_maske |= (uint8_t)(1u << (unsigned int)j);
            }
            else if (m > (int64_t)maks_mutlak)
            {
                /* Bu dalda m <= ELBARI_AYKIRI_ESIK; daralma guvenli. */
                maks_mutlak = (int32_t)m;
            }
            else
            {
                /* MISRA: else-if zinciri else ile biter */
            }
        }

        /* MISRA 10.6: "kosul ? 1 : 0" bilesik ifadedir; int32_t'ye
         * atanmasi genisletme sayilir. if/else ile ayni sonuc, kural disi. */
        if (aykiri_maske != 0u)
        {
            aykiri_var = 1;
        }
        else
        {
            aykiri_var = 0;
        }

        /* 2) Bu blok icin en kucuk yeterli bit genisligi (bkz. genislik tablosu) */
        if (maks_mutlak <= 0)
        {
            bit_genisligi = ELBARI_GENISLIK_0; mod = 0;
        }
        else if (maks_mutlak <= 1)
        {
            bit_genisligi = ELBARI_GENISLIK_1; mod = 1;
        }
        else if (maks_mutlak <= 3)
        {
            bit_genisligi = ELBARI_GENISLIK_2; mod = 2;
        }
        else if (maks_mutlak <= 7)
        {
            bit_genisligi = ELBARI_GENISLIK_3; mod = 3;
        }
        else if (maks_mutlak <= 15)
        {
            bit_genisligi = ELBARI_GENISLIK_4; mod = 4;
        }
        else if (maks_mutlak <= 127)
        {
            bit_genisligi = ELBARI_GENISLIK_5; mod = 5;
        }
        else if (maks_mutlak <= 511)
        {
            bit_genisligi = ELBARI_GENISLIK_6; mod = 6;
        }
        else
        {
            bit_genisligi = ELBARI_GENISLIK_7; mod = 7;
        }

        /* 3) Etiket (4 bit) */
        /* MISRA 10.1: bit islemleri isaretsiz tip uzerinde yapilir. */
        etiket_bitleri = ((uint32_t)mod << 1) | (uint32_t)aykiri_var;
        etiket = (int32_t)etiket_bitleri;
        bit_tamponu |= ((uint64_t)(uint32_t)etiket) << (unsigned int)bit_sayisi;
        bit_sayisi += 4;

        if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                 cikti, cikti_kapasitesi, &bayt_indeksi) == 0)
        {
            return ELBARI_HATA_TAMPON_KUCUK;
        }

        /* 4) Aykiri maske (8 bit, yalnizca gerekiyorsa) */
        if (aykiri_var != 0)
        {
            bit_tamponu |= ((uint64_t)aykiri_maske) << (unsigned int)bit_sayisi;
            bit_sayisi += 8;

            if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                     cikti, cikti_kapasitesi, &bayt_indeksi) == 0)
            {
                return ELBARI_HATA_TAMPON_KUCUK;
            }
        }

        maske = elbari_ic_alt_maske(bit_genisligi);

        /* 5) Aykiri OLMAYAN farklar, blok bit genisligi ile.
         *    SIFIR BLOK (bit_genisligi == 0) durumunda hic veri biti
         *    yazilmaz; farklarin hepsi zaten sifirdir. */
        if (bit_genisligi > 0)
        {
            for (j = 0; j < blok_boyu; j++)
            {
                int32_t fark;
                uint32_t paketli;

                if ((aykiri_var != 0) && ((aykiri_maske & (uint8_t)(1u << (unsigned int)j)) != 0u))
                {
                    continue;
                }

                fark = elbari_ic_fark(ham_veri[veri_indeksi + j],
                                      ham_veri[veri_indeksi + j - 1]);
                paketli = elbari_ic_isaretsize_cevir(fark) & maske;

                bit_tamponu |= ((uint64_t)paketli) << (unsigned int)bit_sayisi;
                bit_sayisi += bit_genisligi;

                if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                         cikti, cikti_kapasitesi, &bayt_indeksi) == 0)
                {
                    return ELBARI_HATA_TAMPON_KUCUK;
                }
            }
        }

        /* 6) Aykiri farklar, tam 32 bit ile */
        if (aykiri_var != 0)
        {
            for (j = 0; j < blok_boyu; j++)
            {
                if ((aykiri_maske & (uint8_t)(1u << (unsigned int)j)) != 0u)
                {
                    int32_t fark = elbari_ic_fark(ham_veri[veri_indeksi + j],
                                                  ham_veri[veri_indeksi + j - 1]);
                    uint32_t tam = elbari_ic_isaretsize_cevir(fark);

                    bit_tamponu |= ((uint64_t)tam) << (unsigned int)bit_sayisi;
                    bit_sayisi += ELBARI_AYKIRI_BIT_GENISLIGI;

                    if (elbari_ic_bit_bosalt(&bit_tamponu, &bit_sayisi,
                                             cikti, cikti_kapasitesi, &bayt_indeksi) == 0)
                    {
                        return ELBARI_HATA_TAMPON_KUCUK;
                    }
                }
            }
        }

        veri_indeksi += blok_boyu;

        /* 7) Erken iptal: yalnizca bir kez, ilk ~64 elemandan sonra bakilir.
         *    Kazanc yoksa devam etmenin anlami yok. */
        if ((erken_iptal_bakildi == 0) &&
            (veri_indeksi >= ((64 < eleman_sayisi) ? 64 : eleman_sayisi)))
        {
            float oran;
            erken_iptal_bakildi = 1;

            /* MISRA 10.8: bilesik ifade once degiskene alinir. */
            islenen_bayt = veri_indeksi * 4;
            oran = (float)islenen_bayt / (float)bayt_indeksi;
            if ((oran < ELBARI_ERKEN_IPTAL_ESIGI) && (veri_indeksi < eleman_sayisi))
            {
                return ELBARI_SIKISTIRILAMAZ;
            }
        }
    }

    /* 8) Tamponda kalan bitler (8'den az) son bir bayta yazilir. */
    if (bit_sayisi > 0)
    {
        if (bayt_indeksi >= cikti_kapasitesi)
        {
            return ELBARI_HATA_TAMPON_KUCUK;
        }
        cikti[bayt_indeksi] = (uint8_t)(bit_tamponu & ELBARI_BAYT_MASKESI);
        bayt_indeksi++;
    }

    return bayt_indeksi;
}

/* ---------------------------------------------------------------------
 * COZUCU
 * ------------------------------------------------------------------- */

/**
 * Cozme cekirdegi. Tuketilen bayt sayisini bildirir, ARTIK KONTROLU
 * YAPMAZ - o karar cagirana aittir.
 */
static int32_t elbari_ic_coz(const uint8_t *girdi,
                             int32_t        girdi_boyutu,
                             int32_t       *cikti,
                             int32_t        eleman_sayisi,
                             int32_t       *tuketilen_cikti)
{
    int32_t  bayt_indeksi;
    uint64_t bit_tamponu = 0u;
    int32_t  bit_sayisi = 0;
    int32_t  cikti_indeksi = 1;
    int32_t  gecici[ELBARI_BLOK_BOYUTU];
    uint64_t etiket_ham;
    int32_t  isaret_konumu;
    unsigned int kaydirma;

    if ((girdi == NULL) || (cikti == NULL) || (eleman_sayisi < 0))
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi > ELBARI_MAKS_ELEMAN)
    {
        return ELBARI_HATA_PARAMETRE;
    }
    if (eleman_sayisi == 0)
    {
        return ELBARI_TAMAM;
    }
    if (girdi_boyutu < ELBARI_REFERANS_BOYUTU)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }

    /* Akisin basindaki mutlak referans */
    cikti[0] = elbari_ic_i32_oku(girdi);
    bayt_indeksi = ELBARI_REFERANS_BOYUTU;

    while (cikti_indeksi < eleman_sayisi)
    {
        int32_t etiket;
        int32_t mod;
        int32_t aykiri_var;
        int32_t bit_genisligi;
        int32_t kalan;
        int32_t blok_boyu;
        uint32_t maske;
        uint32_t aykiri_maske = 0u;
        int32_t j;

        /* 1) Etiket (4 bit) */
        if (elbari_ic_bit_yukle(&bit_tamponu, &bit_sayisi,
                                girdi, girdi_boyutu, &bayt_indeksi, 4) == 0)
        {
            return ELBARI_HATA_BOZUK_GIRDI;
        }

        /* MISRA 10.8: bilesik ifade once ayni genislikte degiskene alinir. */
        etiket_ham = bit_tamponu & (uint64_t)ELBARI_ETIKET_MASKESI;
        etiket = (int32_t)etiket_ham;
        bit_tamponu >>= 4;
        bit_sayisi -= 4;

        /* MISRA 10.1: kaydirma ve maskeleme isaretsiz tip uzerinde.
         * MISRA 12.1: kosul ifadesi acikca parantezlenir. */
        mod = (int32_t)((uint32_t)etiket >> 1);
        /* MISRA 10.6: "kosul ? 1 : 0" bilesik ifadedir; int32_t'ye
         * atanmasi genisletme sayilir. if/else ile ayni sonuc, kural disi. */
        if ((((uint32_t)etiket) & 1u) != 0u)
        {
            aykiri_var = 1;
        }
        else
        {
            aykiri_var = 0;
        }

        switch (mod)
        {
            case 0:  bit_genisligi = ELBARI_GENISLIK_0; break;
            case 1:  bit_genisligi = ELBARI_GENISLIK_1; break;
            case 2:  bit_genisligi = ELBARI_GENISLIK_2; break;
            case 3:  bit_genisligi = ELBARI_GENISLIK_3; break;
            case 4:  bit_genisligi = ELBARI_GENISLIK_4; break;
            case 5:  bit_genisligi = ELBARI_GENISLIK_5; break;
            case 6:  bit_genisligi = ELBARI_GENISLIK_6; break;
            default: bit_genisligi = ELBARI_GENISLIK_7; break;
        }

        kalan = eleman_sayisi - cikti_indeksi;
        blok_boyu = (kalan < ELBARI_BLOK_BOYUTU) ? kalan : ELBARI_BLOK_BOYUTU;
        maske = elbari_ic_alt_maske(bit_genisligi);

        /* 2) Aykiri maske */
        if (aykiri_var != 0)
        {
            if (elbari_ic_bit_yukle(&bit_tamponu, &bit_sayisi,
                                    girdi, girdi_boyutu, &bayt_indeksi, 8) == 0)
            {
                return ELBARI_HATA_BOZUK_GIRDI;
            }
            aykiri_maske = (uint32_t)(bit_tamponu & (uint64_t)ELBARI_BAYT_MASKESI);
            bit_tamponu >>= 8;
            bit_sayisi -= 8;

            /* BLOK-USTU SIFIR KOSUSU (bicim surumu 3).
             * Maske 0x00, surum 2 kodlayicisinin URETEMEYECEGI bir
             * degerdir (aykiri_var ancak maske sifir degilken kurulur);
             * bu yuzden kacis isareti olarak kullanilir. Surum 2 akislari
             * bu daldan hic gecmez ve aynen cozulur. */
            if ((mod == 0) && (aykiri_maske == 0u))
            {
                int32_t kosu;
                int32_t r;

                if (elbari_ic_bit_yukle(&bit_tamponu, &bit_sayisi,
                                        girdi, girdi_boyutu, &bayt_indeksi,
                                        ELBARI_SIFIR_KOSU_BITI) == 0)
                {
                    return ELBARI_HATA_BOZUK_GIRDI;
                }
                kosu = (int32_t)(uint32_t)(bit_tamponu & 0xFFFFu);
                bit_tamponu >>= (unsigned int)ELBARI_SIFIR_KOSU_BITI;
                bit_sayisi -= ELBARI_SIFIR_KOSU_BITI;

                /* Kodlayici sifir uzunlukta kosu yazmaz. */
                if (kosu <= 0) { return ELBARI_HATA_BOZUK_GIRDI; }

                for (r = 0; r < kosu; r++)
                {
                    for (j = 0; j < ELBARI_BLOK_BOYUTU; j++)
                    {
                        if (cikti_indeksi >= eleman_sayisi)
                        {
                            return ELBARI_HATA_BOZUK_GIRDI;
                        }
                        cikti[cikti_indeksi] = cikti[cikti_indeksi - 1];
                        cikti_indeksi++;
                    }
                }
                continue;
            }
        }

        /* 3) Aykiri OLMAYAN farklar (isaret genisletmesi ile) */
        for (j = 0; j < blok_boyu; j++)
        {
            uint32_t ham;
            uint32_t isaret_biti;

            if ((aykiri_var != 0) && ((aykiri_maske & (1u << (unsigned int)j)) != 0u))
            {
                continue;
            }

            /* SIFIR BLOK: veri biti yazilmamisti, fark sifirdir. */
            if (bit_genisligi <= 0)
            {
                gecici[j] = 0;
                continue;
            }

            if (elbari_ic_bit_yukle(&bit_tamponu, &bit_sayisi,
                                    girdi, girdi_boyutu, &bayt_indeksi,
                                    bit_genisligi) == 0)
            {
                return ELBARI_HATA_BOZUK_GIRDI;
            }

            ham = (uint32_t)(bit_tamponu & (uint64_t)maske);
            bit_tamponu >>= (unsigned int)bit_genisligi;
            bit_sayisi -= bit_genisligi;

            /* Isaret genisletme: dar alanda saklanan negatif deger geri kurulur */
            /* MISRA 10.8 + 12.2: kaydirma miktari once degiskene alinir ve
             * araligi acikca sinirlanir (bit_genisligi 1..16'dir). */
            isaret_konumu = bit_genisligi - 1;
            kaydirma = (unsigned int)isaret_konumu & 31u;
            isaret_biti = 1u << kaydirma;
            if ((ham & isaret_biti) != 0u)
            {
                ham |= ~maske;
            }

            gecici[j] = elbari_ic_isaretliye_cevir(ham);
        }

        /* 4) Aykiri farklar (tam 32 bit, isaret genisletmesi gerekmez) */
        if (aykiri_var != 0)
        {
            for (j = 0; j < blok_boyu; j++)
            {
                if ((aykiri_maske & (1u << (unsigned int)j)) != 0u)
                {
                    uint32_t tam;

                    if (elbari_ic_bit_yukle(&bit_tamponu, &bit_sayisi,
                                            girdi, girdi_boyutu, &bayt_indeksi,
                                            ELBARI_AYKIRI_BIT_GENISLIGI) == 0)
                    {
                        return ELBARI_HATA_BOZUK_GIRDI;
                    }

                    tam = (uint32_t)(bit_tamponu & 0xFFFFFFFFu);
                    bit_tamponu >>= (unsigned int)ELBARI_AYKIRI_BIT_GENISLIGI;
                    bit_sayisi -= ELBARI_AYKIRI_BIT_GENISLIGI;

                    gecici[j] = elbari_ic_isaretliye_cevir(tam);
                }
            }
        }

        /* 5) Onek toplam ile orijinal degerleri geri kur */
        for (j = 0; j < blok_boyu; j++)
        {
            cikti[cikti_indeksi] = elbari_ic_topla(cikti[cikti_indeksi - 1], gecici[j]);
            cikti_indeksi++;
        }
    }

    /* 6) YAPISAL DOGRULAMA - tuketim kontrolu
     *
     * Gecerli bir sikistirilmis akis, girdinin TAMAMINI tuketir: kodlayici
     * tam olarak gerektigi kadar bayt yazar, cozucu de tam olarak o kadarini
     * okur. Geriye kayda deger bir artik kalmissa girdi bu kodlayicidan
     * cikmamis demektir.
     *
     * Bu, saglama toplaminin yerini TUTMAZ; ancak maliyeti tek bir
     * karsilastirmadir ve rastgele/bozuk verinin buyuk kismini eler.
     * Butunluk garantisi icin cerceve katmani (CRC32) kullanilmalidir.
     *
     * NOT: girdi_boyutu, sikistirilmis verinin TAM boyutu olmalidir.
     * Daha buyuk bir tampon verilirse bu kontrol devreye girer ve
     * ELBARI_HATA_BOZUK_GIRDI donulur.
     *
     * Kontrol asagidaki elbari_basit sarmalayicisinda yapilir; cekirdek
     * yalnizca tuketilen bayt sayisini bildirir. Kanal katmani ardisik
     * akislari tek tamponda cozdugu icin bu ayrimi gerektirir. */
    if (tuketilen_cikti != NULL) { *tuketilen_cikti = bayt_indeksi; }
    return ELBARI_TAMAM;
}

int32_t elbari_basit(const uint8_t *girdi,
                     int32_t        girdi_boyutu,
                     int32_t       *cikti,
                     int32_t        eleman_sayisi)
{
    int32_t tuketilen = 0;
    int32_t durum = elbari_ic_coz(girdi, girdi_boyutu, cikti,
                                  eleman_sayisi, &tuketilen);

    if (durum != ELBARI_TAMAM) { return durum; }

    /* YAPISAL DOGRULAMA - tuketim kontrolu (yukaridaki nota bakiniz).
     * Tek akis API'sinde girdi_boyutu TAM boyut olmalidir. */
    if ((girdi_boyutu - tuketilen) > ELBARI_ARTIK_TOLERANSI)
    {
        return ELBARI_HATA_BOZUK_GIRDI;
    }
    return ELBARI_TAMAM;
}

int32_t elbari_basit_akis(const uint8_t *girdi,
                          int32_t        girdi_boyutu,
                          int32_t       *cikti,
                          int32_t        eleman_sayisi,
                          int32_t       *tuketilen_cikti)
{
    if (tuketilen_cikti == NULL) { return ELBARI_HATA_PARAMETRE; }
    return elbari_ic_coz(girdi, girdi_boyutu, cikti, eleman_sayisi,
                         tuketilen_cikti);
}
