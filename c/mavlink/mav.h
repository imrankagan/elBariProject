/* =====================================================================
 * MAVLINK IKI KADEMELI TELEMETRI VEKILI
 * ---------------------------------------------------------------------
 * NE YAPAR:
 *   Telsizin iki ucuna birer VEKIL (proxy) koyar. Otopilot ve yer
 *   istasyonu normal MAVLink konusur; arada, link uzerinde sikistirilmis
 *   akar. Yani MAVLink'in YERINE gecmez, altina girer.
 *
 *       [Otopilot] --MAVLink--> [vekil] ~~telsiz~~ [vekil] --MAVLink--> [YIS]
 *
 * NEDEN IKI KADEME:
 *   Sikistirma toplu is ister: N ornek birikmeden paket gonderilemez.
 *   Ama telemetrinin bir kismi BEKLEYEMEZ (kalp atisi, ucus modu,
 *   batarya alarmi). Bu yuzden akis ikiye ayrilir:
 *
 *     KADEME 1 (canli)  : hic beklemeden, ham MAVLink olarak gecer.
 *                         Gecikme = 0. Sikistirma yok.
 *     KADEME 2 (toplu)  : msgid'ye gore biriktirilir, ElBari ile
 *                         sikistirilir, tek pakette gonderilir.
 *                         Gecikme = tampon suresi. Kazanc buradan gelir.
 *
 * NEDEN CALISIR:
 *   MAVLink akisi heterojendir (ATTITUDE, GPS, RC_CHANNELS... hepsi
 *   farkli), ama HER MESAJ TIPININ ALAN DUZENI SABITTIR. msgid'ye gore
 *   ayirinca her grup, ElBari'nin bekledigi duzenli kayit matrisine
 *   donusur: satirlar = mesaj ornekleri, sutunlar = alanlar (kanallar).
 *
 * ---------------------------------------------------------------------
 * !!! KAPSAM VE SINIRLAR - OKUMADAN KULLANMAYIN !!!
 *
 *   1) SEMA TABLOSU ELLE YAZILMISTIR. Buradaki alan duzenleri
 *      common.xml'in yaygin surumune gore girilmistir. MAVLink v2
 *      "message extension" ozelligi mesajlara SONRADAN alan ekleyebilir.
 *      Kullanmadan once kendi dialect surumunuzle DOGRULAYIN.
 *
 *   2) CRC_EXTRA TABLOSU BOSTUR. Gercek MAVLink CRC'si, mesaj adi ve
 *      alan turlerinden turetilen bir CRC_EXTRA baytini gerektirir.
 *      Bu tabloyu uretilmis MAVLink basliklarindan doldurmadan gercek
 *      bir otopilotla KONUSAMAZSINIZ. Olcum icin onemsizdir: CRC her
 *      halukarda 2 bayttir, boyut hesaplari degismez.
 *
 *   3) Bilinmeyen msgid'ler KADEME 1'e dusurulur (ham gecer). Boylece
 *      semasi olmayan bir mesaj kaybolmaz, yalnizca sikismaz.
 * ===================================================================== */

#ifndef MAV_H
#define MAV_H

#include <stdint.h>
#include <stddef.h>

/* =====================================================================
 * MAVLINK v2 CERCEVE
 * =====================================================================
 * [0]      STX = 0xFD
 * [1]      yuk uzunlugu (kirpilmis)
 * [2]      uyumsuzluk bayraklari
 * [3]      uyumluluk bayraklari
 * [4]      sira no
 * [5]      sistem kimligi
 * [6]      bilesen kimligi
 * [7..9]   msgid (24 bit, little-endian)
 * [10..]   yuk
 * [son 2]  CRC-16/MCRF4XX
 *
 * v2 zorunlu olarak yukun SONUNDAKI SIFIR BAYTLARI KIRPAR. Bu, taban
 * cizgisi olcumunu dogrudan etkiler; kirpma uygulanmazsa sikistirmasiz
 * durum oldugundan buyuk gorunur ve kazanc abartilmis olur.
 * ===================================================================== */

#define MAV_STX            (0xFDu)
#define MAV_BASLIK         (10)
#define MAV_CRC            (2)
#define MAV_EK_YUK         (MAV_BASLIK + MAV_CRC)   /* = 12 */
#define MAV_MAKS_YUK       (255)
#define MAV_MAKS_CERCEVE   (MAV_EK_YUK + MAV_MAKS_YUK)

typedef struct
{
    uint32_t msgid;
    uint8_t  sira;
    uint8_t  sistem;
    uint8_t  bilesen;
    uint8_t  yuk[MAV_MAKS_YUK];
    int32_t  yuk_boyutu;      /* KIRPILMAMIS (sema) boyut */
} mav_mesaj;

/** CRC-16/MCRF4XX - MAVLink'in kullandigi saglama. */
uint16_t mav_crc16(const uint8_t *veri, int32_t boyut, uint16_t baslangic);

/**
 * Mesaji MAVLink v2 cercevesi olarak yazar (sondaki sifirlar kirpilir).
 * @return yazilan bayt, ya da < 0 hata
 */
int32_t mav_cerceve_yaz(const mav_mesaj *m, uint8_t *cikti, int32_t kapasite);

/**
 * MAVLink v2 cercevesini cozer. Kirpilmis yuk, sema boyutuna kadar
 * sifirla tamamlanir.
 * @param sema_yuk_boyutu  bu msgid'nin tam yuk boyutu (bilinmiyorsa
 *                         kirpilmis boyut kullanilir)
 * @return tuketilen bayt, ya da < 0 hata
 */
int32_t mav_cerceve_oku(const uint8_t *girdi, int32_t boyut,
                        mav_mesaj *cikti, int32_t sema_yuk_boyutu);

/* =====================================================================
 * SEMA - mesaj alan tanimlari
 * ===================================================================== */

typedef enum
{
    ALAN_U8 = 0,
    ALAN_I8,
    ALAN_U16,
    ALAN_I16,
    ALAN_U32,
    ALAN_I32,
    ALAN_U64,     /* iki kanala bolunur: alt 32 bit, ust 32 bit */
    ALAN_F32
} alan_turu;

typedef struct
{
    const char *ad;
    alan_turu   tur;
    int32_t     tekrar;   /* dizi alanlari icin eleman sayisi (1 = tekil) */
    float       olcek;    /* F32 icin kuantalama olcegi (1/hassasiyet) */
} alan_tanimi;

typedef enum
{
    KADEME_CANLI = 0,   /* beklemeden ham gecer */
    KADEME_TOPLU        /* biriktirilir ve sikistirilir */
} kademe;

typedef struct
{
    uint32_t           msgid;
    const char        *ad;
    int32_t            yuk_boyutu;
    const alan_tanimi *alanlar;
    int32_t            alan_sayisi;
    kademe             hangi_kademe;
    /**
     * 0  : canli akisa hic gitmez.
     * N>0: her N'inci ornek AYRICA ham olarak canli akisa da konur.
     *      Boylece operator harita/gosterge icin dusuk hizli canli veri
     *      gorur, tam hizli veri ise sikistirilmis olarak akar.
     */
    int32_t            canli_seyreltme;
    double             hz;   /* tipik yayin hizi (olcum senaryosu icin) */
} mesaj_tanimi;

/** Tablodaki mesaj tanimini bulur; bilinmiyorsa NULL. */
const mesaj_tanimi *mav_sema_bul(uint32_t msgid);

/** Sema tablosu ve uzunlugu. */
const mesaj_tanimi *mav_sema_tablosu(int32_t *adet_cikti);

/** Bu mesajin kac ElBari kanalina karsilik geldigi (U64 = 2 kanal). */
int32_t mav_kanal_sayisi(const mesaj_tanimi *t);

/**
 * Sema tutarliligini denetler: alan boyutlarinin toplami yuk boyutunu
 * tutmali, kanal sayisi siniri asmamalidir. Elle girilmis bir tabloda
 * bu denetim sessiz bir yazim hatasini yakalar.
 * @param bildir  hata basina cagrilir (NULL olabilir)
 * @return hatali mesaj sayisi (0 = temiz)
 */
int32_t mav_sema_dogrula(void (*bildir)(const char *ad, int32_t beklenen,
                                        int32_t bulunan));

/**
 * MAVLink yukunu int32 kanal degerlerine cozer.
 * @param kuantala 1 ise F32 alanlar olcekle carpilip tamsayiya cevrilir
 *                 (KAYIPLI); 0 ise bit deseni oldugu gibi tasinir.
 */
void mav_yuk_coz(const mesaj_tanimi *t, const uint8_t *yuk,
                 int32_t kuantala, int32_t *kanallar);

/** mav_yuk_coz'un tersi. */
void mav_yuk_yaz(const mesaj_tanimi *t, const int32_t *kanallar,
                 int32_t kuantala, uint8_t *yuk);

/* =====================================================================
 * LINK PAKETI - vekiller arasinda giden bicim
 * =====================================================================
 * [0] tur bayti:
 *      0x01 = canli  -> [1..] ham MAVLink cercevesi (uzunlugu kendinde)
 *      0x02 = toplu  -> [1..4] msgid, [5..6] kayit sayisi,
 *                       [7..8] yuk uzunlugu,
 *                       [9..]  elbari_cerceve_yaz ciktisi (CRC32 dahil)
 *
 * Uzunluk alani neden var: gercek bir telsizde her link paketi ayri
 * gonderilir ve uzunlugu tasima katmanindan gelir. Olcum ise paketleri
 * tek tampona ard arda yazar; sinirlari ayirt edebilmek icin uzunluk
 * gereklidir. 2 bayt olarak SAYILIR, kazanci sismesin.
 *
 * !!! SIRA NUMARASI UYARISI:
 * MAVLink sira numarasi (seq) tum akis icin tek sayacdir; msgid'ye gore
 * biriktirince korunamaz. Alici taraf seq'i YENIDEN URETIR. Yer
 * istasyonunun "paket dustu" sayaci bu yuzden vekil arkasinda anlamini
 * yitirir - kayip tespiti link katmanina (CRC32 + cerceve sira no)
 * tasinmistir.
 * ===================================================================== */

#define LINK_TUR_CANLI      (0x01u)
#define LINK_TUR_TOPLU      (0x02u)
/**
 * 0x03 = toplu kademedeki bir mesajin HAM gecisi.
 *
 * NEDEN VAR: kucuk toplu paketlerde sikistirma kazandirmayabilir -
 * cerceve basligi (16 bayt) + link basligi (9 bayt) kucuk bir yuku
 * asar. O durumda vekil sikistirmayi BIRAKIR ve kayitlari ham gonderir.
 * Boylece vekil hicbir kosulda taban cizgisinden KOTU olamaz.
 *
 * Ayri bir tur bayti gerekir: 0x01 ile karistirilirsa alici bunlari
 * seyreltme kopyasi sanip atar.
 */
#define LINK_TUR_HAM_TOPLU  (0x03u)
#define LINK_TOPLU_BAS      (9)

/* =====================================================================
 * IKI KADEMELI VEKIL
 * ===================================================================== */

#define MAV_MAKS_KAYIT   (512)    /* toplu pakette en fazla kayit */
#define MAV_MAKS_KANAL   (64)     /* mesaj basina en fazla kanal      */

typedef struct
{
    uint32_t msgid;
    int32_t  kayit_sayisi;
    int32_t  hedef_kayit;         /* dolunca gonderilir */
    int32_t  kanal_sayisi;
    int32_t *tampon;              /* hedef_kayit x kanal_sayisi */
    uint8_t  sistem;
    uint8_t  bilesen;
    uint8_t  ilk_sira;
} mav_biriktirici;

typedef struct
{
    /* Ayarlar */
    double   gecikme_saniye;  /* toplu kademe icin gecikme butcesi          */
    int32_t  kuantala;        /* 1 = F32 alanlar kuantalanir (KAYIPLI)      */
    /**
     * Link paketi bu boyutu asmamalidir (telsiz MTU'su).
     * Asarsa toplu paket, siğacak parcalara BOLUNUR. Boylece paket
     * boyutu gecikme butcesinden BAGIMSIZ kalir: gecikmeyi buyutmek
     * paketleri buyutmez, yalnizca daha cok parca uretir.
     */
    int32_t  mtu;

    /* Sayaclar - olcum icin */
    long     canli_bayt;
    long     toplu_bayt;
    long     canli_paket;
    long     toplu_paket;
    long     en_buyuk_paket;
    long     giren_mesaj;
    long     giren_bayt;      /* vekil olmasaydi linkte akacak bayt */

    /* Ic durum */
    mav_biriktirici *biriktiriciler;
    int32_t  biriktirici_adedi;
    int32_t *tampon_havuzu;
    uint32_t cerceve_sira;
    int32_t *elbari_calisma;
    int32_t  elbari_calisma_kap;
    int32_t *coz_tampon;          /* alici taraf - calisma aninda tahsisat yok */
    int32_t  coz_tampon_kap;
    uint8_t *kazan_tampon;        /* sikistirma denemesi icin gecici alan */
    int32_t  kazan_tampon_kap;
    uint8_t *ham_tampon;          /* ham geri dusme denemesi icin gecici alan */
    int32_t  ham_tampon_kap;
    int32_t  seyreltme_sayaci[64];

    /* Mesaj basina tani (biriktirici sirasinda) */
    long    *msg_link_bayt;       /* linkte gercekten harcanan            */
    long    *msg_ham_bayt;        /* vekil olmasaydi harcanacak           */
    long    *msg_geri_dusme;      /* kac kez ham gecise dusuldu           */
    long    *msg_paket;
} mav_vekil;

/**
 * Vekili kurar.
 * @param gecikme_saniye  Toplu kademenin gecikme butcesi. Her mesaj icin
 *                        biriktirilecek ornek sayisi bu butceden ve
 *                        mesajin yayin hizindan HESAPLANIR:
 *                            hedef = yuvarla(hz * gecikme_saniye)
 *                        Boylece 10 Hz'lik ATTITUDE ile 1 Hz'lik
 *                        BATTERY_STATUS ayni gecikmeyi gorur.
 * @param kuantala        1 = ondalikli alanlar kuantalanir (KAYIPLI,
 *                        cok daha iyi oran), 0 = bit deseni korunur.
 * @return 0 basarili, < 0 hata
 */
int32_t mav_vekil_kur(mav_vekil *v, double gecikme_saniye, int32_t kuantala,
                      int32_t mtu);

/** Vekilin ayirdigi belleği birakir. */
void mav_vekil_birak(mav_vekil *v);

/**
 * Bir MAVLink mesajini vekile verir. Uretilen link paketleri cikti
 * tamponuna yazilir (0, 1 ya da 2 paket olabilir).
 * @return yazilan toplam bayt, ya da < 0 hata
 */
int32_t mav_vekil_ver(mav_vekil *v, const mav_mesaj *m,
                      uint8_t *cikti, int32_t kapasite);

/**
 * Yarim kalmis tum biriktiricileri bosaltir (akis sonu / zaman asimi).
 * @return yazilan bayt
 */
int32_t mav_vekil_bosalt(mav_vekil *v, uint8_t *cikti, int32_t kapasite);

/**
 * Alici taraf: tek bir link paketini cozup icindeki MAVLink mesajlarini
 * geri uretir.
 * @param mesajlar     cikti dizisi
 * @param maks_mesaj   dizinin kapasitesi
 * @param adet_cikti   uretilen mesaj sayisi
 * @return tuketilen bayt, ya da < 0 hata
 */
int32_t mav_vekil_coz(mav_vekil *v, const uint8_t *paket, int32_t boyut,
                      mav_mesaj *mesajlar, int32_t maks_mesaj,
                      int32_t *adet_cikti);

/* =====================================================================
 * SENTETIK AKIS URETECI (olcum icin)
 * ===================================================================== */

typedef struct
{
    const int32_t *gps;          /* gercek GPS verisi: lat, lon, zaman */
    int32_t        gps_kayit;
    int32_t        gps_indeks;
    double         t;            /* saniye */
    uint32_t       rastgele;
    uint8_t        sira;
    double         sonraki[64];  /* mesaj basina bir sonraki yayin zamani */
} mav_uretici;

void mav_uretici_kur(mav_uretici *u, const int32_t *gps, int32_t gps_kayit);

/**
 * Bir sonraki mesaji uretir (zaman sirasinda).
 * @return 1 mesaj uretildi, 0 sure doldu
 */
int32_t mav_uretici_sonraki(mav_uretici *u, double sure_siniri, mav_mesaj *m);

#endif /* MAV_H */
