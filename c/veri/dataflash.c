/* =====================================================================
 * ARDUPILOT DATAFLASH (.bin) LOG OKUYUCUSU - uygulama
 * ===================================================================== */

#include <string.h>
#include "dataflash.h"

/* ---------------------------------------------------------------------
 * BICIM KARAKTERLERI
 * ---------------------------------------------------------------------
 * ArduPilot'un log bicim tablosu. Olcekli tipler (c/C/e/E) tam sayi
 * olarak saklanip 100'e bolunur; L enlem/boylamdir ve 1e-7 derece
 * olceginde int32 tutulur - MAVLink'teki gosterimle AYNI, bu yuzden
 * donusum gerektirmez.
 * ------------------------------------------------------------------- */

int32_t df_bicim_boyutu(char c)
{
    switch (c)
    {
    case 'b': case 'B': case 'M':            return 1;
    case 'h': case 'H': case 'c': case 'C':  return 2;
    case 'i': case 'I': case 'f': case 'e':
    case 'E': case 'L':                      return 4;
    case 'd': case 'q': case 'Q':            return 8;
    case 'n':                                return 4;
    case 'N':                                return 16;
    case 'Z':                                return 64;
    case 'a':                                return 64;  /* int16_t[32] */
    default:                                 return 0;
    }
}

/* ---------------------------------------------------------------------
 * KUCUK YARDIMCILAR
 * ------------------------------------------------------------------- */

static uint16_t oku16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t oku32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t oku64(const uint8_t *p)
{
    return (uint64_t)oku32(p) | ((uint64_t)oku32(&p[4]) << 32);
}

/** Sabit uzunluklu, sifirla dolgulu olabilen alani C metnine kopyalar. */
static void metin_kopyala(char *hedef, int32_t hedef_boyut,
                          const uint8_t *kaynak, int32_t kaynak_boyut)
{
    int32_t i;
    int32_t n = (kaynak_boyut < (hedef_boyut - 1)) ? kaynak_boyut
                                                   : (hedef_boyut - 1);
    for (i = 0; i < n; i++)
    {
        if (kaynak[i] == 0u) { break; }
        hedef[i] = (char)kaynak[i];
    }
    hedef[i] = '\0';
}

/* ---------------------------------------------------------------------
 * KURULUM
 * ------------------------------------------------------------------- */

void df_kur(df_okuyucu *o, const uint8_t *veri, int64_t boyut)
{
    (void)memset(o, 0, sizeof(*o));
    o->veri  = veri;
    o->boyut = boyut;
    o->konum = 0;
}

/* ---------------------------------------------------------------------
 * FMT KAYDINI ISLE
 * ---------------------------------------------------------------------
 * FMT, baska bir tipin yapisini tanimlar. Bicim dizesindeki her
 * karakter bir alani, etiket dizesindeki her virgullu parca o alanin
 * adini verir. Ikisinin sayisi tutmazsa tanim GUVENILMEZ sayilir ve
 * kaydedilmez - sessizce yanlis alan okumaktansa hic okumamak yeglenir.
 * ------------------------------------------------------------------- */

static void fmt_isle(df_okuyucu *o, const uint8_t *k)
{
    df_tanim t;
    char     bicim[20];
    char     etiketler[80];
    int32_t  ofset = 3;    /* baslik: 0xA3 0x95 <tip> */
    int32_t  i;
    int32_t  etiket_adedi = 0;
    int32_t  p = 0;

    (void)memset(&t, 0, sizeof(t));

    t.tip     = k[3];
    t.uzunluk = k[4];
    metin_kopyala(t.ad, (int32_t)sizeof(t.ad), &k[5], 4);
    metin_kopyala(bicim, (int32_t)sizeof(bicim), &k[9], 16);
    metin_kopyala(etiketler, (int32_t)sizeof(etiketler), &k[25], 64);

    /* Alan ofsetleri: baslik 3 bayt, sonra bicim sirasiyla */
    t.alan_sayisi = 0;
    for (i = 0; (bicim[i] != '\0') && (i < DF_MAKS_ALAN); i++)
    {
        int32_t boy = df_bicim_boyutu(bicim[i]);
        if (boy == 0) { return; }          /* bilinmeyen tip -> tanimi atla */

        t.alan_bicimi[t.alan_sayisi] = bicim[i];
        t.alan_ofseti[t.alan_sayisi] = ofset;
        ofset += boy;
        t.alan_sayisi++;
    }

    /* Etiketleri virgulden ayir */
    for (i = 0; ; i++)
    {
        char c = etiketler[i];

        if ((c == ',') || (c == '\0'))
        {
            if (etiket_adedi < DF_MAKS_ALAN)
            {
                t.alan_adi[etiket_adedi][p] = '\0';
            }
            etiket_adedi++;
            p = 0;
            if (c == '\0') { break; }
        }
        else if ((etiket_adedi < DF_MAKS_ALAN) && (p < 18))
        {
            t.alan_adi[etiket_adedi][p] = c;
            p++;
        }
        else
        {
            /* tasma: yoksay */
        }
    }

    /* Tutarlilik: bicim ve etiket sayisi ayni olmali. */
    if (etiket_adedi != t.alan_sayisi) { return; }

    o->tanimlar[t.tip] = t;
    o->tanimli[t.tip]  = 1;
}

/* ---------------------------------------------------------------------
 * SONRAKI KAYIT
 * ------------------------------------------------------------------- */

int32_t df_sonraki(df_okuyucu *o, const df_tanim **tanim_cikti,
                   const uint8_t **govde_cikti)
{
    while ((o->konum + 3) <= o->boyut)
    {
        const uint8_t *k = &o->veri[o->konum];
        uint8_t tip;

        /* Senkron ara. Log dosyalarinda bozuk/eksik blok olabilir;
         * bu durumda bayt bayt ilerleyip yeniden yakalariz. */
        if ((k[0] != DF_BAS1) || (k[1] != DF_BAS2))
        {
            o->konum++;
            o->atlanan_bayt++;
            continue;
        }

        tip = k[2];

        if (tip == DF_FMT_TIP)
        {
            if ((o->konum + DF_FMT_UZUNLUK) > o->boyut) { return 0; }
            fmt_isle(o, k);
            o->konum += DF_FMT_UZUNLUK;
            continue;
        }

        if (o->tanimli[tip] == 0)
        {
            /* Tanimi gorulmemis tip: uzunlugunu bilemeyiz, senkron ara. */
            o->konum++;
            o->atlanan_bayt++;
            continue;
        }

        {
            const df_tanim *t = &o->tanimlar[tip];

            if ((o->konum + (int64_t)t->uzunluk) > o->boyut) { return 0; }

            *tanim_cikti = t;
            *govde_cikti = k;
            o->konum += (int64_t)t->uzunluk;
            return 1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------
 * ARAMA
 * ------------------------------------------------------------------- */

const df_tanim *df_tanim_bul(const df_okuyucu *o, const char *ad)
{
    int32_t i;

    for (i = 0; i < 256; i++)
    {
        if ((o->tanimli[i] != 0) && (strcmp(o->tanimlar[i].ad, ad) == 0))
        {
            return &o->tanimlar[i];
        }
    }
    return NULL;
}

int32_t df_alan_bul(const df_tanim *t, const char *etiket)
{
    int32_t i;

    if (t == NULL) { return -1; }
    for (i = 0; i < t->alan_sayisi; i++)
    {
        if (strcmp(t->alan_adi[i], etiket) == 0) { return i; }
    }
    return -1;
}

/* ---------------------------------------------------------------------
 * DEGER OKUMA
 * ------------------------------------------------------------------- */

int64_t df_tamsayi(const df_tanim *t, const uint8_t *govde, int32_t alan)
{
    const uint8_t *p;
    char c;

    if ((t == NULL) || (alan < 0) || (alan >= t->alan_sayisi)) { return 0; }

    p = &govde[t->alan_ofseti[alan]];
    c = t->alan_bicimi[alan];

    switch (c)
    {
    case 'b':            return (int64_t)(int8_t)p[0];
    case 'B': case 'M':  return (int64_t)p[0];
    case 'h': case 'c':  return (int64_t)(int16_t)oku16(p);
    case 'H': case 'C':  return (int64_t)oku16(p);
    case 'i': case 'e': case 'L': return (int64_t)(int32_t)oku32(p);
    case 'I': case 'E':  return (int64_t)oku32(p);
    case 'q':            return (int64_t)oku64(p);
    case 'Q':            return (int64_t)oku64(p);
    case 'f':
        {
            float f;
            uint32_t ham = oku32(p);
            (void)memcpy(&f, &ham, sizeof(f));
            return (int64_t)f;
        }
    case 'd':
        {
            double d;
            uint64_t ham = oku64(p);
            (void)memcpy(&d, &ham, sizeof(d));
            return (int64_t)d;
        }
    default:
        return 0;
    }
}

double df_ondalik(const df_tanim *t, const uint8_t *govde, int32_t alan)
{
    const uint8_t *p;
    char c;

    if ((t == NULL) || (alan < 0) || (alan >= t->alan_sayisi)) { return 0.0; }

    p = &govde[t->alan_ofseti[alan]];
    c = t->alan_bicimi[alan];

    switch (c)
    {
    case 'f':
        {
            float f;
            uint32_t ham = oku32(p);
            (void)memcpy(&f, &ham, sizeof(f));
            return (double)f;
        }
    case 'd':
        {
            double d;
            uint64_t ham = oku64(p);
            (void)memcpy(&d, &ham, sizeof(d));
            return d;
        }
    /* Olcekli tam sayilar: 100'e bolunur */
    case 'c': return (double)(int16_t)oku16(p) / 100.0;
    case 'C': return (double)oku16(p) / 100.0;
    case 'e': return (double)(int32_t)oku32(p) / 100.0;
    case 'E': return (double)oku32(p) / 100.0;
    default:  return (double)df_tamsayi(t, govde, alan);
    }
}
