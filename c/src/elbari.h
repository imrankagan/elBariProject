/* =====================================================================
 * ELBARI - Telemetri Sikistirma Motoru (C surumu)
 * ---------------------------------------------------------------------
 * Telif Hakki (c) 2025 Imran Kagan. Tum Haklari Saklidir.
 * Ticari yazilim - lisans gereklidir.
 * ---------------------------------------------------------------------
 * TASARIM KURALLARI
 *   - Kaynak C99 uyumludur; C17 ile derlenir. C11 ozellikleri yalnizca
 *     #if korumasi arkasinda, isteges bagli ek denetim olarak kullanilir.
 *   - Dinamik bellek YOKTUR. Tum tamponlari cagiran verir.
 *   - Ozyineleme (recursion) YOKTUR. Yigin derinligi sabittir.
 *   - Tum donguler sinirlidir. Sonsuz dongu olusamaz.
 *   - Istisna yoktur; hatalar donus kodu ile bildirilir.
 *   - Harici bagimlilik yoktur (yalnizca <stdint.h>, <string.h>).
 * ---------------------------------------------------------------------
 * UC KATMAN
 *   1) Cekirdek : elbari_kabid / elbari_basit
 *        Tek bir tamsayi akisini fark + uyarlanabilir bit paketleme ile
 *        sikistirir.
 *   2) Kanal    : elbari_kanal_kabid / elbari_kanal_basit
 *        Cok kanalli kayit akisini kanallara ayirip her kanali kendi
 *        icinde sikistirir.
 *   3) Cerceve  : elbari_cerceve_yaz / elbari_cerceve_oku
 *        Akisi bagimsiz cozulebilir, sira numarali, CRC32 korumali
 *        cercevelere boler. Paket kaybina dayaniklilik buradan gelir.
 * ---------------------------------------------------------------------
 * BAYT DUZENI
 *   Tum cok baytli alanlar little-endian yazilir/okunur. Bu, .NET
 *   surumuyle ikili uyumlulugu garanti eder.
 * ===================================================================== */

#ifndef ELBARI_H
#define ELBARI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * DONUS KODLARI
 * ===================================================================== */

/** Islem basarili. */
#define ELBARI_TAMAM                    (0)

/**
 * Veri sikistirilamaz olarak degerlendirildi (gercek dunya verisi degil,
 * ya da sikistirma kazanci yok). Hata degildir; cagiran veriyi ham
 * gondermeyi secmelidir.
 */
#define ELBARI_SIKISTIRILAMAZ           (-1)

/** Verilen tampon gerekli boyuttan kucuk. */
#define ELBARI_HATA_TAMPON_KUCUK        (-2)

/** Gecersiz parametre (NULL isaretci, aralik disi kanal sayisi vb.). */
#define ELBARI_HATA_PARAMETRE           (-3)

/** Girdi verisi bozuk ya da beklenen bicimde degil. */
#define ELBARI_HATA_BOZUK_GIRDI         (-4)

/* =====================================================================
 * SABITLER
 * ===================================================================== */

/** Sikistirma blogundaki eleman sayisi. */
#define ELBARI_BLOK_BOYUTU              (8)

/** Bu esigin ustundeki mutlak fark "aykiri deger" sayilir. */
#define ELBARI_AYKIRI_ESIK              (32767)

/** Aykiri degerlerin kodlandigi bit genisligi. */
#define ELBARI_AYKIRI_BIT_GENISLIGI     (32)

/** Akisin basindaki mutlak referans degerin bayt boyutu. */
#define ELBARI_REFERANS_BOYUTU          (4)

/** Desteklenen en fazla kanal sayisi (baslikta 1 bayt ile tutulur). */
#define ELBARI_MAKS_KANAL               (255)

/**
 * Tek bir cagrida islenebilecek en fazla eleman sayisi.
 *
 * NEDEN SINIR VAR:
 * Boyut hesaplari 32 bit tamsayi ile yapilir (eleman_sayisi * 4 + pay).
 * Bu sinir olmasaydi cok buyuk bir eleman_sayisi degeri carpma sirasinda
 * tasar, negatif ya da kucuk bir "gerekli boyut" uretir ve cagiran
 * yetersiz bir tampon ayirirdi. Sinir, tum ic hesaplarin INT32 araliginda
 * kalmasini garanti eder.
 *
 * 200.000.000 eleman = 800 MB ham veri; pratikte fazlasiyla yeterlidir.
 * Daha buyuk veri parca parca islenmelidir (zaten cerceve katmaninin
 * onerdigi kullanim bicimi budur).
 */
#define ELBARI_MAKS_ELEMAN              (200000000)

/** Cerceve basliginin bayt uzunlugu. */
/* Cerceve basligi (bicim surumu 4). Surum 3'te 16 bayttı:
 *   2 sihirli sayi + 1 surum + 1 ayrilmis + 4 CRC + 4 sira + 4 kayit
 * Kucuk cercevede bu sabit maliyet amorti edilemiyordu, o yuzden
 * daraltildi:
 *   [0]      sihirli sayi (1 bayt yeter; asil dogrulamayi CRC yapar)
 *   [1]      surum
 *   [2..5]   CRC32
 *   [6..7]   sira no      (uint16, sarar - kayip tespitine yeter)
 *   [8..9]   kayit sayisi (uint16)
 * Ayrilmis bayt kaldirildi. Kazanc: cerceve basina 6 bayt. */
#define ELBARI_CERCEVE_BASLIK_BOYUTU    (10)

/** Cerceve basliginda sira no ve kayit sayisinin ust siniri. */
#define ELBARI_CERCEVE_MAKS_ALAN        (65535)

/* =====================================================================
 * KATMANLI BUTUNLUK MODELI - GUVENLIK NOTU
 * =====================================================================
 *
 * Butunluk kontrolu (saglama toplami) YALNIZCA cerceve katmanindadir.
 *
 *   Cekirdek (elbari_basit)      : yapisal tuketim kontrolu (saglama toplami YOK)
 *   Kanal    (elbari_kanal_basit): yalnizca baslik tutarlilik kontrolu
 *   Cerceve  (elbari_cerceve_oku): CRC32 ile korunur
 *
 * Bu bilincli bir tasarim tercihidir: alt katmanlar sicak yolda calisir
 * ve zaten dogrulanmis veri uzerinde islem yapmalari beklenir. Saglama
 * toplamini her katmanda tekrarlamak gereksiz maliyet olurdu.
 *
 * SONUCU SUDUR:
 * Cekirdek cozucuye rastgele/bozuk bayt verilirse HATA DONDURMEYEBILIR;
 * bit akisini oldugu gibi yorumlar ve ANLAMSIZ VERI uretir. Bu bir
 * guvenlik acigi degildir (tampon tasmasi olusmaz, surec cokmez), ancak
 * sessizce yanlis veri uretir.
 *
 * KURAL:
 *   GUVENILMEYEN kaynaktan (telsiz linki, ag, disk) gelen veri
 *   DAIMA cerceve katmanindan gecirilmelidir. elbari_basit ve
 *   elbari_kanal_basit dogrudan guvenilmeyen veriye uygulanmamalidir.
 *
 * YAPISAL TUKETIM KONTROLU:
 * Cekirdek cozucu, saglama toplami olmasa da ucuz bir yapisal kontrol
 * uygular: gecerli bir akis girdinin TAMAMINI tuketir. Geriye artik
 * kalmissa girdi reddedilir. Bu sayede rastgele verinin buyuk kismi
 * elenir (fuzz'da kabul edilen cop girdi 88.963'ten 17'ye dustu).
 *
 * DIKKAT: Bu kontrol, girdi_boyutu degerinin sikistirilmis verinin TAM
 * boyutu olmasini gerektirir. Daha buyuk bir tampon verilirse akis
 * reddedilir; cozucu verinin nerede bittigini kendi basina bilemez.
 *
 * Fuzz testi sonucu (400.000 tur, kanarya korumali tamponlar):
 *   - bozulmus cerceveler: %100 reddedildi (99.790/99.790)
 *   - kanal katmani      : rastgele girdinin tamami reddedildi
 *   - cekirdek           : 100.568 red / 17 kabul (yapisal kontrol sonrasi)
 *   - tampon tasmasi     : 0
 * ===================================================================== */

/* =====================================================================
 * KATMAN 1 - CEKIRDEK
 * ===================================================================== */

/**
 * Bir tamsayi dizisini sikistirir.
 *
 * @param ham_veri         Sikistirilacak eleman dizisi (NULL olamaz).
 * @param eleman_sayisi    ham_veri icindeki eleman sayisi (>= 0).
 * @param cikti            Cikti tamponu (NULL olamaz).
 * @param cikti_kapasitesi cikti tamponunun bayt kapasitesi.
 *
 * @return  > 0  : yazilan bayt sayisi
 *          = 0  : girdi bostu
 *          ELBARI_SIKISTIRILAMAZ : veri sikistirmaya uygun degil
 *          ELBARI_HATA_*         : hata
 *
 * @note En kotu durumda gereken kapasite:
 *       elbari_cekirdek_en_kotu_durum_boyutu(eleman_sayisi)
 */
int32_t elbari_kabid(const int32_t *ham_veri,
                     int32_t        eleman_sayisi,
                     uint8_t       *cikti,
                     int32_t        cikti_kapasitesi);

/**
 * elbari_kabid ile uretilmis veriyi acar.
 *
 * @param girdi         Sikistirilmis veri (NULL olamaz).
 * @param girdi_boyutu  girdi icindeki bayt sayisi.
 * @param cikti         Cozulen elemanlarin yazilacagi dizi (NULL olamaz).
 * @param eleman_sayisi Orijinal eleman sayisi. Cagiran bu degeri bilmek
 *                      zorundadir; bicim icinde tasinmaz.
 *
 * @return ELBARI_TAMAM veya ELBARI_HATA_*
 */
int32_t elbari_basit(const uint8_t *girdi,
                     int32_t        girdi_boyutu,
                     int32_t       *cikti,
                     int32_t        eleman_sayisi);

/** Cekirdek katman icin guvenli en kotu durum cikti boyutu (bayt). */
int32_t elbari_cekirdek_en_kotu_durum_boyutu(int32_t eleman_sayisi);

/**
 * elbari_basit ile ayni cozmeyi yapar; ek olarak TUKETILEN bayt sayisini
 * bildirir ve artik (residue) kontrolunu YAPMAZ.
 *
 * NEDEN VAR: kanal katmani birden cok kanalin akisini TEK tamponda ardisik
 * tutar. Her kanalin nerede bittigini ayri bir uzunluk tablosuyla tasimak
 * yerine cozucunun kendi tuketimini bildirmesi yeterlidir - tablo boylece
 * tamamen kalkar (bicim surumu 4). Butunluk kontrolu cagirana aittir;
 * cerceve katmani bunu CRC ile zaten yapar.
 *
 * @param tuketilen_cikti  okunan bayt sayisi (NULL olamaz)
 * @return ELBARI_TAMAM veya ELBARI_HATA_*
 */
int32_t elbari_basit_akis(const uint8_t *girdi,
                          int32_t        girdi_boyutu,
                          int32_t       *cikti,
                          int32_t        eleman_sayisi,
                          int32_t       *tuketilen_cikti);

/* ---------------------------------------------------------------------
 * REFERANSI DISARIDA TUTAN VARYANTLAR  (bicim surumu 4)
 * ---------------------------------------------------------------------
 * elbari_kabid akisin basina 4 baytlik MUTLAK REFERANS yazar. Cok kanalli
 * bir cercevede bu, kanal basina 4 bayt demektir - 8 kanalli 25 kayitlik
 * bir cercevede toplam boyutun neredeyse YARISI.
 *
 * Oysa kanallarin ilk degerleri genellikle BIRBIRINE YAKINDIR (8 RC
 * kanalinin hepsi ~1500). Kanal katmani bu K degeri tek bir referans
 * blogunda toplayip birlikte sikistirir; her kanalin akisi da referansini
 * disaridan alir.
 * ------------------------------------------------------------------- */

/** elbari_kabid ile ayni; mutlak referansi YAZMAZ. */
int32_t elbari_kabid_ref(const int32_t *ham_veri,
                         int32_t        eleman_sayisi,
                         uint8_t       *cikti,
                         int32_t        cikti_kapasitesi);

/** elbari_basit_akis ile ayni; ilk degeri DISARIDAN alir. */
int32_t elbari_basit_ref_akis(const uint8_t *girdi,
                              int32_t        girdi_boyutu,
                              int32_t        ilk_deger,
                              int32_t       *cikti,
                              int32_t        eleman_sayisi,
                              int32_t       *tuketilen_cikti);

/* =====================================================================
 * KATMAN 2 - KANAL (cok kanalli telemetri)
 * ===================================================================== */

/**
 * Ic ice gecmis cok kanalli veriyi kanal kanal sikistirir.
 *
 * Ornek akis (kanal_sayisi = 3):
 *   [enlem0, boylam0, zaman0, enlem1, boylam1, zaman1, ...]
 *
 * @param ham_veri            Ic ice kayit akisi (NULL olamaz).
 * @param eleman_sayisi       Toplam eleman sayisi.
 * @param kanal_sayisi        Kayit basina alan sayisi (1..ELBARI_MAKS_KANAL).
 * @param calisma_alani       Gecici alan (NULL olamaz).
 * @param calisma_kapasitesi  calisma_alani icindeki eleman kapasitesi.
 * @param cikti               Cikti tamponu (NULL olamaz).
 * @param cikti_kapasitesi    cikti tamponunun bayt kapasitesi.
 *
 * @return  >= 0 : yazilan bayt sayisi, ELBARI_HATA_* : hata
 *
 * @note Bir kanal sikistirilamazsa o kanal HAM yazilir ve bayragi
 *       isaretlenir. Kayipsizlik her kosulda korunur; veri dusmez.
 */
int32_t elbari_kanal_kabid(const int32_t *ham_veri,
                           int32_t        eleman_sayisi,
                           int32_t        kanal_sayisi,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           uint8_t       *cikti,
                           int32_t        cikti_kapasitesi);

/**
 * elbari_kanal_kabid ile uretilmis veriyi acar ve ic ice duzene koyar.
 * Kanal sayisi baslikdan okunur.
 *
 * @param eleman_sayisi Orijinal toplam eleman sayisi.
 * @return ELBARI_TAMAM veya ELBARI_HATA_*
 */
int32_t elbari_kanal_basit(const uint8_t *girdi,
                           int32_t        girdi_boyutu,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           int32_t       *cikti,
                           int32_t        eleman_sayisi);

/** Kanal katmani icin guvenli en kotu durum cikti boyutu (bayt). */
int32_t elbari_kanal_en_kotu_durum_boyutu(int32_t eleman_sayisi,
                                          int32_t kanal_sayisi);

/** Kanal katmani icin gereken calisma alani (eleman cinsinden). */
int32_t elbari_kanal_gerekli_calisma_alani(int32_t eleman_sayisi,
                                           int32_t kanal_sayisi);

/* =====================================================================
 * KATMAN 3 - CERCEVE (paket kaybina dayaniklilik)
 * =====================================================================
 *
 * NEDEN VAR:
 * Fark kodlamanin zayifligi zincirleme bagimliliktir: her deger bir
 * oncekine dayanir. Kayipli bir telsiz linkinde tek bir paket duserse
 * ondan sonraki TUM veri cozulemez hale gelir.
 *
 * Cerceve katmani akisi, her biri KENDI mutlak referansini tasiyan,
 * sira numarali ve CRC32 korumali bagimsiz cercevelere boler. Boylece
 * hata yayilimi tek cerceve ile sinirlanir.
 *
 * CERCEVE BICIMI (baslik 16 bayt):
 *   [0..1]   : sihirli sayi 0xEB 0x71
 *   [2]      : surum (4)
 *   [3]      : ayrilmis (0 olmali)
 *   [4..7]   : CRC32  ([8..son] araligi uzerinden)
 *   [8..11]  : cerceve sira numarasi (uint32)
 *   [12..15] : bu cercevedeki KAYIT sayisi (int32)
 *   [16..]   : kanal katmani yuku
 * ===================================================================== */

/**
 * Tek bir bagimsiz cerceve yazar. Cagiran bu cerceveyi tek bir pakette
 * gonderir.
 *
 * @param kayitlar      Ic ice kayit akisi. Uzunlugu kanal_sayisi'nin
 *                      tam kati olmalidir (cerceveler kayit sinirinda
 *                      bolunur).
 * @param eleman_sayisi kayitlar icindeki eleman sayisi.
 * @param kanal_sayisi  Kayit basina alan sayisi.
 * @param sira_no       Cerceve sira numarasi (her cercevede artirilir).
 *
 * @return >= 0 : yazilan bayt sayisi, ELBARI_HATA_* : hata
 */
int32_t elbari_cerceve_yaz(const int32_t *kayitlar,
                           int32_t        eleman_sayisi,
                           int32_t        kanal_sayisi,
                           uint32_t       sira_no,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           uint8_t       *cikti,
                           int32_t        cikti_kapasitesi);

/**
 * Tek bir cerceveyi bagimsiz olarak cozer. Diger cercevelere ihtiyac
 * duymaz; cerceveler sirasiz gelebilir.
 *
 * @param sira_no_cikti      Cozulen cercevenin sira numarasi (NULL olabilir).
 * @param kayit_sayisi_cikti Cozulen kayit sayisi (NULL olabilir).
 *
 * @return ELBARI_TAMAM        : cerceve gecerli ve cozuldu
 *         ELBARI_HATA_BOZUK_GIRDI : cerceve bozuk/eksik; cagiran bu paketi
 *                                   atmali ve kayip olarak saymalidir
 *         ELBARI_HATA_*       : diger hatalar
 */
int32_t elbari_cerceve_oku(const uint8_t *cerceve,
                           int32_t        cerceve_boyutu,
                           int32_t        kanal_sayisi,
                           int32_t       *calisma_alani,
                           int32_t        calisma_kapasitesi,
                           int32_t       *cikti,
                           int32_t        cikti_kapasitesi,
                           uint32_t      *sira_no_cikti,
                           int32_t       *kayit_sayisi_cikti);

/**
 * Cerceve saglam mi? (sihirli sayi + surum + ayrilmis bayt + CRC)
 * Bozuk paket sessizce kabul edilmez.
 *
 * @return 1 gecerli, 0 gecersiz
 */
int32_t elbari_cerceve_gecerli_mi(const uint8_t *cerceve,
                                  int32_t        cerceve_boyutu);

/** Cercevenin sira numarasini okur (dogrulama yapmadan). */
uint32_t elbari_cerceve_sira_no(const uint8_t *cerceve);

/** Cercevedeki kayit sayisini okur (dogrulama yapmadan). */
int32_t elbari_cerceve_kayit_sayisi(const uint8_t *cerceve);

/** Cerceve katmani icin guvenli en kotu durum boyut (bayt). */
int32_t elbari_cerceve_en_kotu_durum_boyutu(int32_t kayit_sayisi,
                                            int32_t kanal_sayisi);

/** Cerceve katmani icin gereken calisma alani (eleman cinsinden). */
int32_t elbari_cerceve_gerekli_calisma_alani(int32_t kayit_sayisi,
                                             int32_t kanal_sayisi);

/**
 * CRC-32 (IEEE 802.3). Bozulma tespiti icindir; guvenlik amacli DEGILDIR.
 */
uint32_t elbari_crc32(const uint8_t *veri, int32_t boyut);

/* =====================================================================
 * FLOAT KUANTALAMA (istege bagli on/son isleme)
 * =====================================================================
 *
 * Cekirdek motor tamsayi uzerinde calisir. Ondalikli telemetri (yonelim,
 * hiz, batarya gerilimi, quaternion) bu katmanla istenen HASSASIYETE
 * gore tamsayiya cevrilir ve mevcut boru hattina verilir. BICIM DEGISMEZ.
 *
 * !!! BU KATMAN KAYIPLIDIR !!!
 * Secilen hassasiyetin altindaki kisim atilir. Telemetri icin genellikle
 * istenen davranistir (0.001 radyan hassasiyet fazlasiyla yeterlidir),
 * ancak tam degerin korunmasi gereken veriler bu katmandan
 * GECIRILMEMELIDIR. Kayipsiz float sikistirma (XOR tabanli) bu surumde
 * YOKTUR.
 *
 * Olcekler bicim icinde TASINMAZ: gonderici ve alici ayni olcek dizisini
 * kullanmak zorundadir (telemetri semasinin parcasi olarak, bant disi).
 * ===================================================================== */

/**
 * Ondalikli degerleri olcekleyip tamsayiya cevirir (KAYIPLI).
 *
 * @param olcek  1 / istenen_hassasiyet. Ornek: 0.001 hassasiyet -> 1000.
 * @return ELBARI_TAMAM, ya da tasma/NaN/gecersiz olcek durumunda
 *         ELBARI_HATA_PARAMETRE (sessizce yanlis deger uretilmez).
 */
int32_t elbari_float_kuantala(const float *girdi,
                              int32_t      adet,
                              float        olcek,
                              int32_t     *cikti);

/** Kuantalamanin tersi: tamsayidan ondalikliya. */
int32_t elbari_float_coz(const int32_t *girdi,
                         int32_t        adet,
                         float          olcek,
                         float         *cikti);

/**
 * Cok kanalli surum: her kanalin kendi olcegi vardir.
 * Bir yonelim acisi ile batarya gerilimi ayni hassasiyeti gerektirmez.
 *
 * @param olcekler kanal_sayisi uzunlugunda olcek dizisi
 */
int32_t elbari_float_kuantala_kanalli(const float *girdi,
                                      int32_t      eleman_sayisi,
                                      int32_t      kanal_sayisi,
                                      const float *olcekler,
                                      int32_t     *cikti);

/** Cok kanalli kuantalamanin tersi. */
int32_t elbari_float_coz_kanalli(const int32_t *girdi,
                                 int32_t        eleman_sayisi,
                                 int32_t        kanal_sayisi,
                                 const float   *olcekler,
                                 float         *cikti);

/** Istenen hassasiyet icin olcek degeri (1 / hassasiyet). */
float elbari_float_olcek_oner(float hassasiyet);

/** Iki dizi arasindaki en buyuk mutlak fark (kuantalama hatasi olcumu). */
float elbari_float_maks_hata(const float *orijinal,
                             const float *geri,
                             int32_t      adet);

/* =====================================================================
 * KAYIPSIZ FLOAT SIKISTIRMA (XOR tabanli)
 * =====================================================================
 *
 * Kuantalama KAYIPLIDIR. Tam degerin korunmasi gereken durumlar icin
 * (ham sensor kaydi, ucus sonrasi analiz, adli inceleme) bu katman
 * kullanilir.
 *
 * Ardisik float'larin bit desenleri XOR'lanir; birbirine yakin
 * degerlerde XOR sonucunun basinda ve sonunda cok sayida sifir bulunur
 * ve yalnizca ortadaki anlamli bitler yazilir. Deger hic degismemisse
 * tek bit yeter. Literaturde Gorilla / Chimp olarak bilinir.
 *
 * DURUST UYARI:
 * Kayipsiz float sikistirma GURULTULU sensor verisinde az kazandirir
 * (tipik %10-40), cunku gurultu mantisin alt bitlerini surekli degistirir
 * ve bu bitler tanimi geregi sikistirilamaz. Tam deger gerekmiyorsa
 * KUANTALAMA kat kat iyi sonuc verir. Bu katman "mecbur kalinca" icindir.
 * ===================================================================== */

/** Tek akis icin guvenli en kotu durum cikti boyutu (bayt). */
int32_t elbari_float_xor_en_kotu_durum_boyutu(int32_t adet);

/** Float dizisini KAYIPSIZ sikistirir. Donus: yazilan bayt, ya da hata. */
int32_t elbari_float_xor_kabid(const float *ham_veri,
                               int32_t      adet,
                               uint8_t     *cikti,
                               int32_t      cikti_kapasitesi);

/** elbari_float_xor_kabid ciktisini acar. */
int32_t elbari_float_xor_basit(const uint8_t *girdi,
                               int32_t        girdi_boyutu,
                               float         *cikti,
                               int32_t        adet);

/** Cok kanalli surum icin en kotu durum cikti boyutu (bayt). */
int32_t elbari_float_xor_kanal_en_kotu_durum_boyutu(int32_t eleman_sayisi,
                                                    int32_t kanal_sayisi);

/**
 * Cok kanalli float akisini kanallara ayirip her kanali KAYIPSIZ sikistirir.
 * @param calisma_alani en az ceil(eleman_sayisi / kanal_sayisi) float
 */
int32_t elbari_float_xor_kanal_kabid(const float *ham_veri,
                                     int32_t      eleman_sayisi,
                                     int32_t      kanal_sayisi,
                                     float       *calisma_alani,
                                     int32_t      calisma_kapasitesi,
                                     uint8_t     *cikti,
                                     int32_t      cikti_kapasitesi);

/** Cok kanalli kayipsiz float ciktisini acar. */
int32_t elbari_float_xor_kanal_basit(const uint8_t *girdi,
                                     int32_t        girdi_boyutu,
                                     float         *calisma_alani,
                                     int32_t        calisma_kapasitesi,
                                     float         *cikti,
                                     int32_t        eleman_sayisi);

#ifdef __cplusplus
}
#endif

#endif /* ELBARI_H */
