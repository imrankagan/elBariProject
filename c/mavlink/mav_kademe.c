/* =====================================================================
 * IKI KADEMELI VEKIL - gonderici ve alici taraf
 * ---------------------------------------------------------------------
 * GONDERICI (mav_vekil_ver):
 *   Gelen her MAVLink mesaji semaya bakilarak yonlendirilir.
 *
 *     KADEME_CANLI  -> tur bayti + ham cerceve, HEMEN gonderilir
 *     KADEME_TOPLU  -> msgid'sine ait biriktiriciye yazilir; biriktirici
 *                      dolunca ElBari cerceve katmaniyla sikistirilip
 *                      tek paket olarak gonderilir
 *
 *   Ayrica canli_seyreltme > 0 olan mesajlarin her N'incisi AYRICA canli
 *   akisa da konur (operatorun sifir gecikmeli gostergesi icin).
 *
 * ALICI (mav_vekil_coz):
 *   Link paketini acar ve icindeki MAVLink mesajlarini geri uretir.
 *   Yer istasyonu farki anlamaz.
 *
 * NEDEN ELBARI CERCEVE KATMANI:
 *   Toplu paket, tek bir telsiz paketi olarak gider. Cerceve katmani
 *   zaten CRC32 + sira numarasi tasir ve bagimsiz cozulur; boylece bir
 *   toplu paket dusse bile digerleri etkilenmez.
 * ===================================================================== */

#include <stdlib.h>
#include <string.h>

#include "mav.h"
#include "../src/elbari.h"

/* ---------------------------------------------------------------------
 * KURULUM
 * ------------------------------------------------------------------- */

/**
 * Gecikme butcesinden bu mesaj icin biriktirilecek ornek sayisini bulur.
 * En az 1 (yoksa mesaj hic gonderilmez), en fazla MAV_MAKS_KAYIT.
 */
static int32_t hedef_hesapla(double hz, double gecikme)
{
    double x = hz * gecikme;
    int32_t n = (int32_t)(x + 0.5);

    if (n < 1) { n = 1; }
    if (n > MAV_MAKS_KAYIT) { n = MAV_MAKS_KAYIT; }
    return n;
}

int32_t mav_vekil_kur(mav_vekil *v, const hiz_profili *profil,
                      double gecikme_saniye, int32_t kuantala, int32_t mtu)
{
    int32_t sema_adedi = 0;
    const mesaj_tanimi *sema = mav_sema_tablosu(&sema_adedi);
    int32_t i;
    int32_t havuz = 0;
    int32_t ofset = 0;

    if ((v == NULL) || (profil == NULL) || (gecikme_saniye <= 0.0))
    {
        return -1;
    }

    (void)memset(v, 0, sizeof(*v));
    v->gecikme_saniye = gecikme_saniye;
    v->kuantala       = kuantala;
    /* MTU verilmezse sinirsiz kabul edilir (yalnizca olcum senaryolari icin). */
    v->mtu            = (mtu > 0) ? mtu : 1000000;

    for (i = 0; i < sema_adedi; i++)
    {
        havuz += hedef_hesapla(mav_hiz(&sema[i], profil), gecikme_saniye)
                 * mav_kanal_sayisi(&sema[i]);
    }

    v->tampon_havuzu = (int32_t *)malloc((size_t)havuz * sizeof(int32_t));
    v->biriktiriciler = (mav_biriktirici *)malloc((size_t)sema_adedi
                                                  * sizeof(mav_biriktirici));
    if ((v->tampon_havuzu == NULL) || (v->biriktiriciler == NULL))
    {
        mav_vekil_birak(v);
        return -1;
    }
    v->biriktirici_adedi = sema_adedi;

    for (i = 0; i < sema_adedi; i++)
    {
        int32_t kanal = mav_kanal_sayisi(&sema[i]);
        int32_t hedef = hedef_hesapla(mav_hiz(&sema[i], profil), gecikme_saniye);

        v->biriktiriciler[i].msgid        = sema[i].msgid;
        v->biriktiriciler[i].kayit_sayisi = 0;
        v->biriktiriciler[i].hedef_kayit  = hedef;
        v->biriktiriciler[i].kanal_sayisi = kanal;
        v->biriktiriciler[i].tampon       = &v->tampon_havuzu[ofset];
        v->biriktiriciler[i].sistem       = 1u;
        v->biriktiriciler[i].bilesen      = 1u;
        v->biriktiriciler[i].ilk_sira     = 0u;
        ofset += hedef * kanal;
    }

    /* ElBari cerceve katmani calisma alani: en genis mesaji karsilamali. */
    v->elbari_calisma_kap = 0;
    for (i = 0; i < sema_adedi; i++)
    {
        int32_t gerek = elbari_cerceve_gerekli_calisma_alani(
                            hedef_hesapla(mav_hiz(&sema[i], profil), gecikme_saniye),
                            mav_kanal_sayisi(&sema[i]));
        if (gerek > v->elbari_calisma_kap) { v->elbari_calisma_kap = gerek; }
    }
    v->elbari_calisma = (int32_t *)malloc((size_t)v->elbari_calisma_kap
                                          * sizeof(int32_t));

    /* Alici tarafin cozme tamponu da onceden ayrilir: projenin
     * "calisma aninda tahsisat yok" kuralini vekil de bozmamali. */
    v->coz_tampon_kap = 0;
    for (i = 0; i < sema_adedi; i++)
    {
        int32_t gerek = hedef_hesapla(mav_hiz(&sema[i], profil), gecikme_saniye)
                        * mav_kanal_sayisi(&sema[i]);
        if (gerek > v->coz_tampon_kap) { v->coz_tampon_kap = gerek; }
    }
    v->coz_tampon = (int32_t *)malloc((size_t)v->coz_tampon_kap * sizeof(int32_t));

    /* Sikistirma denemesi ve ham geri dusme denemesi icin gecici alanlar.
     * En kotu durum: her kayit tam boy MAVLink cercevesi olarak yazilir. */
    v->kazan_tampon_kap = 0;
    v->ham_tampon_kap   = 0;
    for (i = 0; i < sema_adedi; i++)
    {
        int32_t hedef = hedef_hesapla(mav_hiz(&sema[i], profil), gecikme_saniye);
        int32_t sikistirma = elbari_cerceve_en_kotu_durum_boyutu(
                                 hedef, mav_kanal_sayisi(&sema[i]));
        int32_t hamm = hedef * (MAV_EK_YUK + MAV_MAKS_YUK + 1);

        if (sikistirma > v->kazan_tampon_kap) { v->kazan_tampon_kap = sikistirma; }
        if (hamm > v->ham_tampon_kap) { v->ham_tampon_kap = hamm; }
    }
    v->kazan_tampon = (uint8_t *)malloc((size_t)v->kazan_tampon_kap);
    v->ham_tampon   = (uint8_t *)malloc((size_t)v->ham_tampon_kap);

    v->msg_link_bayt  = (long *)calloc((size_t)sema_adedi, sizeof(long));
    v->msg_ham_bayt   = (long *)calloc((size_t)sema_adedi, sizeof(long));
    v->msg_geri_dusme = (long *)calloc((size_t)sema_adedi, sizeof(long));
    v->msg_paket      = (long *)calloc((size_t)sema_adedi, sizeof(long));

    if ((v->elbari_calisma == NULL) || (v->coz_tampon == NULL) ||
        (v->kazan_tampon == NULL) || (v->ham_tampon == NULL) ||
        (v->msg_link_bayt == NULL) || (v->msg_ham_bayt == NULL) ||
        (v->msg_geri_dusme == NULL) || (v->msg_paket == NULL))
    {
        mav_vekil_birak(v);
        return -1;
    }

    return 0;
}

void mav_vekil_birak(mav_vekil *v)
{
    if (v == NULL) { return; }
    free(v->tampon_havuzu);
    free(v->biriktiriciler);
    free(v->elbari_calisma);
    free(v->coz_tampon);
    free(v->kazan_tampon);
    free(v->ham_tampon);
    free(v->msg_link_bayt);
    free(v->msg_ham_bayt);
    free(v->msg_geri_dusme);
    free(v->msg_paket);
    v->tampon_havuzu  = NULL;
    v->biriktiriciler = NULL;
    v->elbari_calisma = NULL;
    v->coz_tampon     = NULL;
    v->kazan_tampon   = NULL;
    v->ham_tampon     = NULL;
    v->msg_link_bayt  = NULL;
    v->msg_ham_bayt   = NULL;
    v->msg_geri_dusme = NULL;
    v->msg_paket      = NULL;
}

/* ---------------------------------------------------------------------
 * CANLI PAKET YAZ
 * ------------------------------------------------------------------- */

static int32_t canli_yaz(mav_vekil *v, const mav_mesaj *m,
                         uint8_t *cikti, int32_t kapasite)
{
    int32_t n;

    if (kapasite < 1) { return -1; }
    cikti[0] = (uint8_t)LINK_TUR_CANLI;

    n = mav_cerceve_yaz(m, &cikti[1], kapasite - 1);
    if (n < 0) { return -1; }

    v->canli_bayt += (long)(n + 1);
    v->canli_paket++;
    if ((long)(n + 1) > v->en_buyuk_paket) { v->en_buyuk_paket = (long)(n + 1); }
    return n + 1;
}

/* ---------------------------------------------------------------------
 * BIR BIRIKTIRICIYI BOSALT
 * ------------------------------------------------------------------- */

/**
 * [r0, r0+n) kayitlarini sikistirmayi dener; sonuc v->kazan_tampon'da.
 * @return link paketinin toplam boyutu, ya da < 0 (sikistirilamadi)
 */
static int32_t parca_dene(mav_vekil *v, const mav_biriktirici *b,
                          int32_t r0, int32_t n)
{
    int32_t yazilan = elbari_cerceve_yaz(&b->tampon[r0 * b->kanal_sayisi],
                                         n * b->kanal_sayisi,
                                         b->kanal_sayisi,
                                         v->cerceve_sira,
                                         v->elbari_calisma, v->elbari_calisma_kap,
                                         v->kazan_tampon, v->kazan_tampon_kap);
    if (yazilan < 0) { return -1; }
    return LINK_TOPLU_BAS + yazilan;
}

/**
 * Bir biriktiriciyi bosaltir.
 *
 * IKI BAGIMSIZ KISIT AYNI ANDA KARSILANIR:
 *
 *   1) MTU. Toplu paket telsizin tasiyabildigi boyutu ASMAMALIDIR.
 *      Asarsa alt katman parcalar; bir parca duserse cerceve komple
 *      gider ve "bagimsiz cerceve" garantisi coker. Bu yuzden paket,
 *      MTU'ya sigacak PARCALARA bolunur. Sonuc: paket boyutu gecikme
 *      butcesinden BAGIMSIZ kalir - gecikmeyi buyutmek paketleri
 *      buyutmez, yalnizca daha cok parca uretir.
 *
 *   2) Kazanc. Her parca icin iki aday uretilir:
 *        a) sikistirilmis toplu paket (link basligi + ElBari cercevesi)
 *        b) kayitlarin ham MAVLink cerceveleri (LINK_TUR_HAM_TOPLU)
 *      ve KUCUK OLANI secilir. Kucuk parcalarda sabit yuk (9 + 16 = 25
 *      bayt) sikistirma kazancini asabilir; secim yapilmazsa vekil dusuk
 *      gecikme butcelerinde taban cizgisinden DAHA COK bayt harcar -
 *      olculmustur (0.80x).
 */
static int32_t biriktirici_bosalt(mav_vekil *v, int32_t indeks,
                                  uint8_t *cikti, int32_t kapasite)
{
    mav_biriktirici    *b = &v->biriktiriciler[indeks];
    const mesaj_tanimi *t;
    int32_t r0 = 0;
    int32_t yazilan = 0;
    int32_t kalan_kayit;

    if (b->kayit_sayisi <= 0) { return 0; }

    t = mav_sema_bul(b->msgid);
    if (t == NULL) { b->kayit_sayisi = 0; return 0; }

    kalan_kayit = b->kayit_sayisi;

    while (r0 < kalan_kayit)
    {
        int32_t n = kalan_kayit - r0;
        int32_t sikistirilmis;
        int32_t ham_boyut = 0;
        int32_t ham_en_buyuk = 0;
        int32_t j;

        /* --- MTU'ya sigacak en buyuk parcayi bul --- */
        for (;;)
        {
            sikistirilmis = parca_dene(v, b, r0, n);
            if ((sikistirilmis > 0) && (sikistirilmis <= v->mtu)) { break; }
            if (n <= 1)
            {
                /* Tek kayit bile sigmadi ya da sikistirilamadi: ham gecis. */
                sikistirilmis = -1;
                break;
            }
            n = n / 2;
        }

        /* --- Aday (b): ham --- */
        for (j = 0; j < n; j++)
        {
            mav_mesaj m;
            int32_t   cn;

            m.msgid      = b->msgid;
            m.sira       = (uint8_t)((uint32_t)b->ilk_sira + (uint32_t)(r0 + j));
            m.sistem     = b->sistem;
            m.bilesen    = b->bilesen;
            m.yuk_boyutu = t->yuk_boyutu;
            (void)memset(m.yuk, 0, (size_t)t->yuk_boyutu);
            mav_yuk_yaz(t, &b->tampon[(r0 + j) * b->kanal_sayisi],
                        v->kuantala, m.yuk);

            if ((ham_boyut + 1) > v->ham_tampon_kap) { return -1; }
            v->ham_tampon[ham_boyut] = (uint8_t)LINK_TUR_HAM_TOPLU;
            ham_boyut++;

            cn = mav_cerceve_yaz(&m, &v->ham_tampon[ham_boyut],
                                 v->ham_tampon_kap - ham_boyut);
            if (cn < 0) { return -1; }
            ham_boyut += cn;
            if ((cn + 1) > ham_en_buyuk) { ham_en_buyuk = cn + 1; }
        }

        /* --- Kucuk olani sec ve yaz --- */
        if ((sikistirilmis > 0) && (sikistirilmis <= ham_boyut))
        {
            int32_t yuk = sikistirilmis - LINK_TOPLU_BAS;
            uint8_t *p = &cikti[yazilan];

            if ((yazilan + sikistirilmis) > kapasite) { return -1; }

            p[0] = (uint8_t)LINK_TUR_TOPLU;
            p[1] = (uint8_t)(b->msgid & 0xFFu);
            p[2] = (uint8_t)((b->msgid >> 8) & 0xFFu);
            p[3] = (uint8_t)((b->msgid >> 16) & 0xFFu);
            p[4] = (uint8_t)((b->msgid >> 24) & 0xFFu);
            p[5] = (uint8_t)((uint32_t)n & 0xFFu);
            p[6] = (uint8_t)(((uint32_t)n >> 8) & 0xFFu);
            p[7] = (uint8_t)((uint32_t)yuk & 0xFFu);
            p[8] = (uint8_t)(((uint32_t)yuk >> 8) & 0xFFu);
            (void)memcpy(&p[LINK_TOPLU_BAS], v->kazan_tampon, (size_t)yuk);

            v->cerceve_sira++;
            v->toplu_paket++;
            v->msg_paket[indeks]++;
            v->toplu_bayt += (long)sikistirilmis;
            v->msg_link_bayt[indeks] += (long)sikistirilmis;
            if ((long)sikistirilmis > v->en_buyuk_paket)
            {
                v->en_buyuk_paket = (long)sikistirilmis;
            }
            yazilan += sikistirilmis;
        }
        else
        {
            if ((yazilan + ham_boyut) > kapasite) { return -1; }
            (void)memcpy(&cikti[yazilan], v->ham_tampon, (size_t)ham_boyut);

            v->msg_geri_dusme[indeks]++;
            v->toplu_paket += (long)n;
            v->msg_paket[indeks] += (long)n;
            v->toplu_bayt += (long)ham_boyut;
            v->msg_link_bayt[indeks] += (long)ham_boyut;
            /* Ham gecisde her kayit AYRI pakettir; en buyugu tek cercevedir. */
            if ((long)ham_en_buyuk > v->en_buyuk_paket)
            {
                v->en_buyuk_paket = (long)ham_en_buyuk;
            }
            yazilan += ham_boyut;
        }

        r0 += n;
    }

    b->kayit_sayisi = 0;
    return yazilan;
}

/* ---------------------------------------------------------------------
 * MESAJ VER
 * ------------------------------------------------------------------- */

int32_t mav_vekil_ver(mav_vekil *v, const mav_mesaj *m,
                      uint8_t *cikti, int32_t kapasite)
{
    const mesaj_tanimi *t;
    int32_t yazilan = 0;
    int32_t i;
    int32_t indeks = -1;
    mav_biriktirici *b;

    if ((v == NULL) || (m == NULL) || (cikti == NULL)) { return -1; }

    v->giren_mesaj++;
    t = mav_sema_bul(m->msgid);

    for (i = 0; i < v->biriktirici_adedi; i++)
    {
        if (v->biriktiriciler[i].msgid == m->msgid) { indeks = i; break; }
    }

    {
        /* Vekil olmasaydi linkte akacak bayt: ham MAVLink cercevesi. */
        uint8_t gecici[MAV_MAKS_CERCEVE];
        int32_t ham = mav_cerceve_yaz(m, gecici, (int32_t)sizeof(gecici));
        if (ham > 0)
        {
            v->giren_bayt += (long)ham;
            if (indeks >= 0) { v->msg_ham_bayt[indeks] += (long)ham; }
        }
    }

    /* Semasi olmayan mesaj KAYBOLMAZ; ham gecer, yalnizca sikismaz. */
    if ((t == NULL) || (t->hangi_kademe == KADEME_CANLI) || (indeks < 0))
    {
        return canli_yaz(v, m, cikti, kapasite);
    }

    /* Seyreltme: her N'inci ornek AYRICA canli akisa da konur. */
    if (t->canli_seyreltme > 0)
    {
        if ((v->seyreltme_sayaci[indeks] % t->canli_seyreltme) == 0)
        {
            int32_t n = canli_yaz(v, m, cikti, kapasite);
            if (n < 0) { return -1; }
            yazilan += n;
        }
        v->seyreltme_sayaci[indeks]++;
    }

    b = &v->biriktiriciler[indeks];
    if (b->kayit_sayisi == 0)
    {
        b->sistem   = m->sistem;
        b->bilesen  = m->bilesen;
        b->ilk_sira = m->sira;
    }

    mav_yuk_coz(t, m->yuk, v->kuantala,
                &b->tampon[b->kayit_sayisi * b->kanal_sayisi]);
    b->kayit_sayisi++;

    if (b->kayit_sayisi >= b->hedef_kayit)
    {
        int32_t n = biriktirici_bosalt(v, indeks, &cikti[yazilan],
                                       kapasite - yazilan);
        if (n < 0) { return -1; }
        yazilan += n;
    }

    return yazilan;
}

/* ---------------------------------------------------------------------
 * TUMUNU BOSALT
 * ------------------------------------------------------------------- */

int32_t mav_vekil_bosalt(mav_vekil *v, uint8_t *cikti, int32_t kapasite)
{
    int32_t yazilan = 0;
    int32_t i;

    for (i = 0; i < v->biriktirici_adedi; i++)
    {
        int32_t n = biriktirici_bosalt(v, i, &cikti[yazilan],
                                       kapasite - yazilan);
        if (n < 0) { return -1; }
        yazilan += n;
    }
    return yazilan;
}

/* ---------------------------------------------------------------------
 * ALICI TARAF
 * ------------------------------------------------------------------- */

int32_t mav_vekil_coz(mav_vekil *v, const uint8_t *paket, int32_t boyut,
                      mav_mesaj *mesajlar, int32_t maks_mesaj,
                      int32_t *adet_cikti)
{
    if ((paket == NULL) || (boyut < 1)) { return -1; }
    *adet_cikti = 0;

    if ((paket[0] == (uint8_t)LINK_TUR_CANLI)
        || (paket[0] == (uint8_t)LINK_TUR_HAM_TOPLU))
    {
        const mesaj_tanimi *t;
        int32_t n;
        int32_t sema_boyut = 0;

        if (maks_mesaj < 1) { return -1; }

        /* Sema boyutunu bilmek icin once msgid'yi okumak gerekir;
         * cerceve okuyucusuna 0 verip sonra tamamlamak yerine iki
         * asamali yapiyoruz: once kirpik oku, sonra semayla tamamla. */
        n = mav_cerceve_oku(&paket[1], boyut - 1, &mesajlar[0], 0);
        if (n < 0) { return -1; }

        t = mav_sema_bul(mesajlar[0].msgid);
        if (t != NULL)
        {
            sema_boyut = t->yuk_boyutu;
            if (sema_boyut > mesajlar[0].yuk_boyutu)
            {
                int32_t i;
                for (i = mesajlar[0].yuk_boyutu; i < sema_boyut; i++)
                {
                    mesajlar[0].yuk[i] = 0u;
                }
                mesajlar[0].yuk_boyutu = sema_boyut;
            }
        }

        *adet_cikti = 1;
        return n + 1;
    }

    if (paket[0] == (uint8_t)LINK_TUR_TOPLU)
    {
        uint32_t msgid;
        int32_t  kayit;
        int32_t  yuk_uzunluk;
        const mesaj_tanimi *t;
        int32_t  kanal;
        int32_t  sonuc;
        int32_t  r;

        if (boyut < LINK_TOPLU_BAS) { return -1; }

        msgid = (uint32_t)paket[1] | ((uint32_t)paket[2] << 8)
                | ((uint32_t)paket[3] << 16) | ((uint32_t)paket[4] << 24);
        kayit = (int32_t)((uint32_t)paket[5] | ((uint32_t)paket[6] << 8));
        yuk_uzunluk = (int32_t)((uint32_t)paket[7] | ((uint32_t)paket[8] << 8));

        t = mav_sema_bul(msgid);
        if ((t == NULL) || (kayit <= 0) || (kayit > maks_mesaj)) { return -1; }
        if ((yuk_uzunluk <= 0) || ((LINK_TOPLU_BAS + yuk_uzunluk) > boyut))
        {
            return -1;
        }

        kanal = mav_kanal_sayisi(t);
        if ((kayit * kanal) > v->coz_tampon_kap) { return -1; }

        sonuc = elbari_cerceve_oku(&paket[LINK_TOPLU_BAS], yuk_uzunluk, kanal,
                                   v->elbari_calisma, v->elbari_calisma_kap,
                                   v->coz_tampon, kayit * kanal, NULL, NULL);
        if (sonuc != ELBARI_TAMAM) { return -1; }

        for (r = 0; r < kayit; r++)
        {
            mesajlar[r].msgid       = msgid;
            /* seq korunamaz (bkz. mav.h sira numarasi uyarisi). */
            mesajlar[r].sira        = 0u;
            mesajlar[r].sistem      = 1u;
            mesajlar[r].bilesen     = 1u;
            mesajlar[r].yuk_boyutu  = t->yuk_boyutu;
            (void)memset(mesajlar[r].yuk, 0, (size_t)t->yuk_boyutu);
            mav_yuk_yaz(t, &v->coz_tampon[r * kanal], v->kuantala,
                        mesajlar[r].yuk);
        }

        *adet_cikti = kayit;
        return LINK_TOPLU_BAS + yuk_uzunluk;
    }

    return -1;
}
