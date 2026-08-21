/* =====================================================================
 * KIYAS - Tamsayi kodek ailesi ile karsilastirma catisi
 * ---------------------------------------------------------------------
 * AMAC:
 *   ElBari kendi belgelerinde "bu bir PFOR-Delta uygulamasidir" diyor.
 *   Buna ragmen simdiye kadar yalnizca GENEL AMACLI bayt sikistiricilar
 *   (zstd/LZ4/Brotli/Deflate) ile olculmustu. Bir kodegi kendi ailesiyle
 *   kiyaslamamak, karsilastirmanin en zayif noktasidir.
 *
 *   Bu catida ElBari, ait oldugu ailenin uyeleriyle olculur:
 *     - VByte (LEB128)      : varint temel cizgisi
 *     - StreamVByte         : Lemire & Kurz, 2017
 *     - Simple8b            : Anh & Moffat, 2010
 *     - BP128               : ikili paketleme (FastPFor kutuphanesindeki
 *                             BinaryPacking bicimi), Lemire & Boytsov 2015
 *     - PFOR-yamali (OptPFD): Zukowski ve ark. 2006 / Yan, Ding, Suel 2009
 *     - Sprintz-Delta       : Blalock ve ark., 2018 (cok kanalli, gomulu)
 *
 * ---------------------------------------------------------------------
 * !!! METODOLOJIK UYARI - BUNU OKUMADAN SAYILARI KULLANMAYIN !!!
 *
 *   Bu dosyalardaki rakip kodekler, YAZARLARININ KUTUPHANELERI DEGILDIR.
 *   Yayinlanmis bicim tanimlarindan yeniden yazilmis skaler C
 *   uygulamalaridir. Bunun iki ayri sonucu vardir:
 *
 *   1) ORAN (sikistirma orani) TASINABILIR.
 *      Bir bicimin urettigi bayt sayisi bicim tanimindan gelir,
 *      uygulamanin kalitesinden degil. Simple8b'yi dogru yazan herkes
 *      ayni boyutu uretir. Bu yuzden oran sutunu literaturle
 *      karsilastirilabilir ve teze girebilir.
 *
 *   2) HIZ TASINABILIR DEGILDIR.
 *      Ustteki kutuphanelerin SIMD surumleri vardir ve buradaki skaler
 *      surumlerden kat kat hizlidir. Buradaki hiz sutunu rakipler icin
 *      bir ALT SINIRDIR, gercek performanslari degildir.
 *
 *      Buna karsilik karsilastirma sinifi ADILDIR: ElBari'nin C surumu de
 *      skalerdir (SIMD yok), ayni derleyici, ayni bayraklar, ayni makine,
 *      ayni veri. Yani "skaler C uygulamalari arasinda" gecerli bir
 *      siralamadir. SIMD'li surumlerle kiyas icin yazarlarin kutuphaneleri
 *      baglanmalidir - bu bir sonraki adimdir.
 *
 * ---------------------------------------------------------------------
 * KAPSAM NOTU:
 *   Bu klasor OLCUM kodudur, kutuphane degildir. MISRA C:2012 uyum
 *   iddiasi c/src/ icin gecerlidir; burasi (c/test/ gibi) kapsam disidir.
 * ===================================================================== */

#ifndef KIYAS_H
#define KIYAS_H

#include <stdint.h>
#include <stddef.h>

/* =====================================================================
 * ORTAK YARDIMCILAR - on isleme
 * ===================================================================== */

/** Zigzag: isaretli tamsayiyi kucuk isaretsiz tamsayiya esler. */
uint32_t kiyas_zigzag(int32_t v);

/** Zigzag'in tersi. */
int32_t kiyas_zigzag_ters(uint32_t u);

/**
 * Ic ice gecmis kayit akisindan tek bir kanali ceker.
 * @return kanala dusen eleman sayisi
 */
int32_t kiyas_kanal_cek(const int32_t *ic_ice, int32_t eleman_sayisi,
                        int32_t kanal_sayisi, int32_t kanal_no,
                        int32_t *cikti);

/** Cekilmis kanali ic ice akista yerine yazar. */
void kiyas_kanal_koy(const int32_t *kanal_verisi, int32_t adet,
                     int32_t kanal_sayisi, int32_t kanal_no,
                     int32_t *ic_ice);

/**
 * Fark + zigzag. cikti[0] = zigzag(veri[0]), sonrasi ardisik fark.
 * Fark hesabi isaretsiz aritmetikle yapilir (tanimsiz davranis yok).
 */
void kiyas_delta_zigzag(const int32_t *veri, int32_t adet, uint32_t *cikti);

/** kiyas_delta_zigzag'in tersi. */
void kiyas_delta_zigzag_ters(const uint32_t *veri, int32_t adet, int32_t *cikti);

/** Bir isaretsiz degeri tasimak icin gereken bit sayisi (0 icin 0 doner). */
int32_t kiyas_bit_genisligi(uint32_t v);

/* =====================================================================
 * BIT AKISI
 * ===================================================================== */

typedef struct
{
    uint8_t *tampon;
    int32_t  kapasite;
    int32_t  bayt_konum;
    uint64_t birikim;
    int32_t  birikim_bit;
    int32_t  tasti;        /* 1 ise kapasite asildi */
} kiyas_bit_yazici;

typedef struct
{
    const uint8_t *tampon;
    int32_t        boyut;
    int32_t        bayt_konum;
    uint64_t       birikim;
    int32_t        birikim_bit;
} kiyas_bit_okuyucu;

void    kiyas_yazici_kur(kiyas_bit_yazici *y, uint8_t *tampon, int32_t kapasite);
void    kiyas_bit_yaz(kiyas_bit_yazici *y, uint32_t deger, int32_t genislik);
int32_t kiyas_yazici_bitir(kiyas_bit_yazici *y);   /* < 0 : tasma */

void     kiyas_okuyucu_kur(kiyas_bit_okuyucu *o, const uint8_t *tampon, int32_t boyut);
uint32_t kiyas_bit_oku(kiyas_bit_okuyucu *o, int32_t genislik);
int32_t  kiyas_okuyucu_bayt(const kiyas_bit_okuyucu *o);

/* =====================================================================
 * AILE A - TEK AKIS TAMSAYI KODEKLERI
 * ---------------------------------------------------------------------
 * Girdi: isaretsiz tamsayi dizisi (on isleme disarida yapilir).
 * Donus: kodla -> yazilan bayt (<0 hata), coz -> 0 basarili (<0 hata).
 * ===================================================================== */

typedef int32_t (*kiyas_kodla_fn)(const uint32_t *girdi, int32_t adet,
                                  uint8_t *cikti, int32_t kapasite);

typedef int32_t (*kiyas_coz_fn)(const uint8_t *girdi, int32_t girdi_boyutu,
                                uint32_t *cikti, int32_t adet);

int32_t kiyas_vbyte_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap);
int32_t kiyas_vbyte_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet);

int32_t kiyas_streamvbyte_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap);
int32_t kiyas_streamvbyte_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet);

int32_t kiyas_simple8b_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap);
int32_t kiyas_simple8b_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet);

int32_t kiyas_bp128_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap);
int32_t kiyas_bp128_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet);

int32_t kiyas_optpfd_kodla(const uint32_t *g, int32_t adet, uint8_t *c, int32_t kap);
int32_t kiyas_optpfd_coz(const uint8_t *g, int32_t boyut, uint32_t *c, int32_t adet);

/* =====================================================================
 * AILE B - COK KANALLI TELEMETRI KODEKLERI
 * ---------------------------------------------------------------------
 * Girdi: ic ice gecmis isaretli kayit akisi. On islemeyi kendi yapar.
 * ===================================================================== */

int32_t kiyas_sprintz_kodla(const int32_t *kayitlar, int32_t eleman_sayisi,
                            int32_t kanal_sayisi, uint8_t *cikti,
                            int32_t kapasite);

int32_t kiyas_sprintz_coz(const uint8_t *girdi, int32_t girdi_boyutu,
                          int32_t eleman_sayisi, int32_t kanal_sayisi,
                          int32_t *cikti);

#endif /* KIYAS_H */
