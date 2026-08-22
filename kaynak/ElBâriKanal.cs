#nullable enable

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ KANAL KATMANI — ÇOK KANALLI TELEMETRİ
    // =================================================================
    //
    // Telif Hakkı (c) 2025 İmran Kağan. Tüm Hakları Saklıdır.
    //
    // NEDEN VAR:
    // Gerçek telemetri tek bir sayı akışı değil, bir KAYIT akışıdır:
    //   [lat, lon, alt, roll, pitch, yaw, lat, lon, alt, ...]
    // Bu akış olduğu gibi ElKâbıd'a verilirse ardışık farklar kanallar
    // arasında zıplar (lat->lon farkı milyonlarca birim olur), aykırı oranı
    // %100'e çıkar ve veri "sıkıştırılamaz" diye REDDEDİLİR.
    //
    // Bu katman akışı önce kanallara ayırır (deinterleave), her kanalı
    // KENDİ İÇİNDE sıkıştırır, sonra tekrar birleştirir.
    //
    // İKİ DERECELİ FARK (delta-of-delta):
    // Sabit hızla ilerleyen bir kanalda (GPS enlemi gibi) ardışık farklar
    // neredeyse sabittir (ör. hep ~135). Bu kanal önce kendi fark akışına
    // çevrilirse ElKâbıd içeride bir kez daha fark aldığı için net etki
    // ikinci derece fark olur ve değerler ~±3'e iner. Buna karşılık gürültülü
    // kanallarda ikinci derece fark varyansı BÜYÜTÜR. Bu yüzden karar
    // kanal başına, veriye bakılarak verilir (bkz. IkinciDereceDahaIyiMi).
    //
    // VERİ KAYBI YOK:
    // Bir kanal sıkıştırılamazsa (ElKâbıd -1 döndürürse veya kazanç yoksa)
    // o kanal HAM olarak yazılır ve bayrağı işaretlenir. Kayıpsızlık her
    // durumda korunur; "reddedildi" diye veri düşmez.
    //
    // TAHSİSAT YOK:
    // Çalışma alanı çağıran tarafından verilir (GerekliCalismaAlani).
    // Bu katman heap üzerinde hiçbir tahsisat yapmaz.
    //
    // BİÇİM (bayt düzeni):
    //   [0]                : kanal sayısı K            (1 bayt)
    //   [1 .. 1+B)         : ikinci-derece bayrakları  (kanal başına 1 bit)
    //   [1+B .. 1+2B)      : ham-geçiş bayrakları      (kanal başına 1 bit)
    //   [1+2B]             : referans bloğu bayrağı    (1 bayt)
    //   [1+2B+1 .. )       : referans bloğu (K değer, biçim sürümü 4)
    //   B = ceil(K/8) TÜRETİLİR, taşınmaz.
    //   Kanal yük boyutları TAŞINMAZ: çekirdek kendi tüketimini bildirir.
    //   sonrası            : kanal yükleri, sırayla
    // =================================================================
    public static class ElBâriKanal
    {
        /// <summary>Desteklenen en fazla kanal sayısı (başlıkta 1 bayt ile tutulur).</summary>
        public const int MAKS_KANAL = 255;

        /// <summary>
        /// Çıktı tamponunun güvenli en kötü durum boyutu.
        /// Bit paketleme patolojik veride ham boyutu bir miktar aşabildiği için
        /// pay bırakılmıştır.
        /// </summary>
        public static int EnKotuDurumCiktiBoyutu(int elemanSayisi, int kanalSayisi)
        {
            int bayrakBayt = (kanalSayisi + 7) / 8;
            // Biçim sürümü 4: kanal başına 4 baytlık uzunluk tablosu KALDIRILDI
            // (kanallar ardışık çözülür, çekirdek kendi tüketimini bildirir) ve
            // bayrakBayt alanı da kaldırıldı — kanalSayisi'ndan türetilebilir.
            // +1 referans bloğu bayrağı, + referans bloğu (en kötü: kanal*4)
            int baslik = 1 + 2 * bayrakBayt + 1 + kanalSayisi * 4;
            // eleman*4 (ham) + eleman/2 (paketleme payı) + kanal başına referans/pay
            return baslik + elemanSayisi * 4 + elemanSayisi / 2 + kanalSayisi * 68 + 64;
        }

        /// <summary>
        /// Gereken çalışma alanı (int cinsinden): en uzun kanalın eleman sayısı.
        /// </summary>
        public static int GerekliCalismaAlani(int elemanSayisi, int kanalSayisi)
            // Kanal başına en uzun dizi + REFERANS BLOĞU için kanalSayisi kadar
            // ek yer (biçim sürümü 4).
            => kanalSayisi <= 0 ? 0 : (elemanSayisi + kanalSayisi - 1) / kanalSayisi + kanalSayisi;

        /// <summary>c numaralı kanalın kaç eleman içerdiği (eksik kayıtlara toleranslı).</summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static int KanalUzunlugu(int toplam, int kanalSayisi, int c)
            => c >= toplam ? 0 : (toplam - c + kanalSayisi - 1) / kanalSayisi;

        // =================================================================
        // KODLAYICI
        // =================================================================
        /// <summary>
        /// İç içe geçmiş çok kanallı veriyi kanal kanal sıkıştırır.
        /// </summary>
        /// <param name="hamVeri">İç içe kayıt akışı: [k0,k1,..,kN-1, k0,k1,..]</param>
        /// <param name="kanalSayisi">Kayıt başına alan sayısı (1..255)</param>
        /// <param name="calismaAlani">Çağıranın verdiği geçici alan; en az GerekliCalismaAlani kadar</param>
        /// <param name="cikti">Çıktı tamponu; en az EnKotuDurumCiktiBoyutu kadar</param>
        /// <returns>Yazılan bayt sayısı. Boş girdide 0.</returns>
        // Çözücünün tüketmeden bırakabileceği en fazla bayt (C ile aynı: 0).
        private const int ARTIK_TOLERANSI = 0;

        public static int ElKâbıdKanal(
            scoped ReadOnlySpan<int> hamVeri,
            int kanalSayisi,
            scoped Span<int> calismaAlani,
            scoped Span<byte> cikti)
        {
            if (kanalSayisi < 1 || kanalSayisi > MAKS_KANAL)
            {
                throw new ArgumentOutOfRangeException(
                    nameof(kanalSayisi), kanalSayisi, $"Kanal sayısı 1 ile {MAKS_KANAL} arasında olmalı.");
            }

            if (hamVeri.IsEmpty) return 0;

            int toplam = hamVeri.Length;
            int gerekliCalisma = GerekliCalismaAlani(toplam, kanalSayisi);
            if (calismaAlani.Length < gerekliCalisma)
            {
                throw new ArgumentException(
                    $"Çalışma alanı çok küçük. En az {gerekliCalisma} int gerekli, {calismaAlani.Length} verildi.",
                    nameof(calismaAlani));
            }

            int bayrakBayt = (kanalSayisi + 7) / 8;
            // +1: referans bloğu bayrağı
            int baslikBoyu = 1 + 2 * bayrakBayt + 1;
            if (cikti.Length < baslikBoyu + kanalSayisi * 4)
            {
                throw new ArgumentException(
                    $"Çıktı tamponu başlık için bile yetersiz. En az {baslikBoyu} bayt gerekli.", nameof(cikti));
            }

            cikti[0] = (byte)kanalSayisi;

            Span<byte> ikinciDereceBayraklari = cikti.Slice(1, bayrakBayt);
            Span<byte> hamGecisBayraklari = cikti.Slice(1 + bayrakBayt, bayrakBayt);

            ikinciDereceBayraklari.Clear();
            hamGecisBayraklari.Clear();

            int yazmaKonumu = baslikBoyu;

            // -------------------------------------------------------------
            // REFERANS BLOĞU (biçim sürümü 4)
            // Her kanalın akışı bir MUTLAK REFERANSLA başlar; bunlar kanal
            // başına 4 bayt tutar ve küçük bir çerçevede toplam boyutun
            // neredeyse yarısını yiyordu. Oysa bu K değer AKIŞIN İLK
            // KAYDIDIR ve kanalların ilk değerleri genellikle birbirine
            // yakındır. Hepsi tek blokta sıkıştırılır.
            // Bayrak: 1 = blok sıkıştırıldı (kendini sınırlar), 0 = ham K x 4.
            // C sürümüyle BİREBİR aynı kural: c/src/elbari_kanal.c
            // -------------------------------------------------------------
            for (int i = 0; i < kanalSayisi; i++)
            {
                calismaAlani[i] = (i < toplam) ? hamVeri[i] : 0;
            }

            int refBoyut = -1;
            if (kanalSayisi >= 3)
            {
                refBoyut = ElBâri.ElKâbıd(calismaAlani.Slice(0, kanalSayisi),
                                          cikti.Slice(yazmaKonumu));
            }

            if (refBoyut > 0 && refBoyut < kanalSayisi * 4)
            {
                cikti[baslikBoyu - 1] = 1;
                yazmaKonumu += refBoyut;
            }
            else
            {
                cikti[baslikBoyu - 1] = 0;
                for (int i = 0; i < kanalSayisi; i++)
                {
                    int r = calismaAlani[i];
                    MemoryMarshal.Write(cikti.Slice(yazmaKonumu + i * 4, 4), in r);
                }
                yazmaKonumu += kanalSayisi * 4;
            }

            for (int c = 0; c < kanalSayisi; c++)
            {
                int uzunluk = KanalUzunlugu(toplam, kanalSayisi, c);
                if (uzunluk == 0) continue;

                // 1) Kanalı topla (deinterleave)
                Span<int> kanal = calismaAlani.Slice(0, uzunluk);
                for (int i = 0; i < uzunluk; i++)
                {
                    kanal[i] = hamVeri[c + i * kanalSayisi];
                }

                // 2) Bu kanal için ikinci derece fark daha mı iyi?
                //
                // ÖNEMLİ: İkinci dereceye geçerken mutlak ilk değer akışın İÇİNDE
                // bırakılmaz. Bırakılsaydı akış [x0, d1, d2, ...] olurdu; x0 mutlak
                // (ör. 1.7 milyar), d1 ise minik bir fark olduğu için 0->1 geçişinde
                // YAPAY ve devasa bir sıçrama oluşurdu. Bu sıçrama hem bir aykırı
                // değer harcar hem de ElKâbıd'ın hızlı tarama istatistiklerini
                // bozarak veriyi gereksiz yere reddettirebilir. Bu yüzden ilk değer
                // yükün başına ayrı bir alan olarak yazılır ve akışta yalnızca
                // farklar kalır.
                bool ikinciDerece = uzunluk >= 3 && IkinciDereceDahaIyiMi(kanal);

                int ilkDeger = 0;
                scoped Span<int> yuk;

                if (ikinciDerece)
                {
                    // Mutlak ilk değer REFERANS BLOĞUNDA; ayrıca yazılmaz.
                    for (int i = 0; i < uzunluk - 1; i++)
                    {
                        kanal[i] = unchecked(kanal[i + 1] - kanal[i]);
                    }
                    yuk = kanal.Slice(0, uzunluk - 1);
                    ikinciDereceBayraklari[c >> 3] |= (byte)(1 << (c & 7));
                }
                else
                {
                    yuk = kanal;
                }

                _ = ilkDeger;
                // Referanstan SONRAKİ değer sayısı her iki derecede de aynı.
                int yukUzunlugu = uzunluk - 1;
                int hamBayt = yukUzunlugu * sizeof(int);
                int yukKonumu = yazmaKonumu;

                // 3) Sıkıştırmayı dene. Birinci derecede referans DIŞARIDA
                //    (blokta) olduğu için ElKâbıdRef kullanılır.
                int sonuc = -1;
                if (yukUzunlugu > 0 && cikti.Length - yukKonumu >= hamBayt + 64)
                {
                    sonuc = ikinciDerece
                          ? ElBâri.ElKâbıd(yuk, cikti.Slice(yukKonumu))
                          : ElBâri.ElKâbıdRef(kanal, cikti.Slice(yukKonumu));
                }

                if (sonuc > 0 && sonuc < hamBayt)
                {
                    // Sıkıştırma kazançlı. Boyut YAZILMAZ: çözücü çekirdeğin
                    // bildirdiği tüketimle ilerler (biçim sürümü 4).
                    yazmaKonumu = yukKonumu + sonuc;
                }
                else
                {
                    // 4) HAM GEÇİŞ: kazanç yok ya da reddedildi -> veriyi ham yaz.
                    // Kayıpsızlık her koşulda korunur.
                    if (yukKonumu + hamBayt > cikti.Length)
                    {
                        throw new ArgumentException(
                            $"Çıktı tamponu çok küçük. EnKotuDurumCiktiBoyutu({toplam}, {kanalSayisi}) kullanın.",
                            nameof(cikti));
                    }

                    // Birinci derecede ilk değer referans bloğunda; [1..m-1]
                    // yazılır. İkinci derecede fark dizisinin tamamı yazılır.
                    for (int i = 0; i < yukUzunlugu; i++)
                    {
                        int deger = ikinciDerece ? kanal[i] : kanal[i + 1];
                        MemoryMarshal.Write(cikti.Slice(yukKonumu + i * 4, 4), in deger);
                    }

                    // Ham geçişin boyutu hesaplanabilir: onEk + eleman*4
                    hamGecisBayraklari[c >> 3] |= (byte)(1 << (c & 7));
                    yazmaKonumu = yukKonumu + hamBayt;
                }
            }

            return yazmaKonumu;
        }

        // =================================================================
        // ÇÖZÜCÜ
        // =================================================================
        /// <summary>
        /// ElKâbıdKanal ile üretilmiş veriyi çözer ve iç içe düzene geri koyar.
        /// Kanal sayısı başlıktan okunur.
        /// </summary>
        /// <param name="girdi">Sıkıştırılmış veri</param>
        /// <param name="calismaAlani">En az GerekliCalismaAlani kadar geçici alan</param>
        /// <param name="cikti">Orijinal eleman sayısı kadar uzunlukta olmalı</param>
        public static void ElBâsıtKanal(
            scoped ReadOnlySpan<byte> girdi,
            scoped Span<int> calismaAlani,
            scoped Span<int> cikti)
        {
            if (cikti.IsEmpty) return;

            if (girdi.Length < 2)
            {
                throw new ArgumentException("Girdi tamponu başlık için çok küçük.", nameof(girdi));
            }

            int kanalSayisi = girdi[0];
            if (kanalSayisi < 1)
            {
                throw new ArgumentException("Girdi başlığı bozuk (kanal sayısı geçersiz).", nameof(girdi));
            }
            // bayrakBayt TAŞINMAZ: kanal sayısından türetilir (biçim sürümü 4).
            int bayrakBayt = (kanalSayisi + 7) / 8;

            // +1: referans bloğu bayrağı
            int baslikBoyu = 1 + 2 * bayrakBayt + 1;
            if (girdi.Length < baslikBoyu)
            {
                throw new ArgumentException("Girdi tamponu başlık için çok küçük.", nameof(girdi));
            }

            int toplam = cikti.Length;
            int gerekliCalisma = GerekliCalismaAlani(toplam, kanalSayisi);
            if (calismaAlani.Length < gerekliCalisma)
            {
                throw new ArgumentException(
                    $"Çalışma alanı çok küçük. En az {gerekliCalisma} int gerekli, {calismaAlani.Length} verildi.",
                    nameof(calismaAlani));
            }

            ReadOnlySpan<byte> ikinciDereceBayraklari = girdi.Slice(1, bayrakBayt);
            ReadOnlySpan<byte> hamGecisBayraklari = girdi.Slice(1 + bayrakBayt, bayrakBayt);
            int okumaKonumu = baslikBoyu;

            // REFERANS BLOĞU (biçim sürümü 4) — bkz. kodlayıcıdaki açıklama.
            // Kanal referansları çalışma alanının SONUNDA tutulur; kanal
            // çözümü için kullanılan baş kısmıyla çakışmaz.
            Span<int> referanslar = calismaAlani.Slice(calismaAlani.Length - kanalSayisi, kanalSayisi);

            if (girdi[baslikBoyu - 1] == 0)
            {
                if (okumaKonumu + kanalSayisi * 4 > girdi.Length)
                {
                    throw new ArgumentException("Girdi bozuk: referans bloğu eksik.", nameof(girdi));
                }
                for (int c2 = 0; c2 < kanalSayisi; c2++)
                {
                    referanslar[c2] = MemoryMarshal.Read<int>(girdi.Slice(okumaKonumu + c2 * 4, 4));
                }
                okumaKonumu += kanalSayisi * 4;
            }
            else
            {
                okumaKonumu += ElBâri.ElBâsıtAkis(girdi.Slice(okumaKonumu), referanslar);
            }

            for (int c = 0; c < kanalSayisi; c++)
            {
                int uzunluk = KanalUzunlugu(toplam, kanalSayisi, c);
                if (uzunluk == 0) continue;

                bool hamGecis = (hamGecisBayraklari[c >> 3] & (1 << (c & 7))) != 0;
                bool ikinciDerece = (ikinciDereceBayraklari[c >> 3] & (1 << (c & 7))) != 0;

                Span<int> kanal = calismaAlani.Slice(0, uzunluk);

                // İkinci derece kanallarda yükün başında mutlak ilk değer bulunur.
                // Mutlak ilk değer REFERANS BLOĞUNDAN gelir (biçim sürümü 4).
                int ilkDeger = referanslar[c];
                int yukKonumu = okumaKonumu;
                int icBoyut = girdi.Length - yukKonumu;
                // Referanstan sonraki değer sayısı her iki derecede de aynı.
                int hedefUzunluk = uzunluk - 1;
                int yukBoyutu = 0;

                if (hedefUzunluk > 0)
                {
                    if (hamGecis)
                    {
                        if (icBoyut < hedefUzunluk * 4)
                        {
                            throw new ArgumentException($"Girdi bozuk: kanal {c} ham yükü eksik.", nameof(girdi));
                        }
                        for (int i = 0; i < hedefUzunluk; i++)
                        {
                            int hedefI = ikinciDerece ? i : (i + 1);
                            kanal[hedefI] = MemoryMarshal.Read<int>(girdi.Slice(yukKonumu + i * 4, 4));
                        }
                        yukBoyutu = hedefUzunluk * 4;
                    }
                    else if (ikinciDerece)
                    {
                        // Fark akışı kendi referansını taşır.
                        yukBoyutu = ElBâri.ElBâsıtAkis(girdi.Slice(yukKonumu, icBoyut),
                                                       kanal.Slice(0, hedefUzunluk));
                    }
                    else
                    {
                        // Birinci derece: referans blokta; çözücüye dışarıdan
                        // verilir ve kanal[0] oradan dolar.
                        yukBoyutu = ElBâri.ElBâsıtRefAkis(girdi.Slice(yukKonumu, icBoyut),
                                                          ilkDeger, kanal.Slice(0, uzunluk));
                    }
                }

                // Ham geçiş / boş kanal yolunda birinci derecenin ilk değeri
                // çözücüden gelmez; referanstan konur.
                if (!ikinciDerece && (hamGecis || hedefUzunluk == 0))
                {
                    kanal[0] = ilkDeger;
                }

                okumaKonumu = yukKonumu + yukBoyutu;

                if (ikinciDerece)
                {
                    // Fark akışını sağa kaydırıp mutlak ilk değeri başa koy, sonra önek toplam al
                    for (int i = uzunluk - 1; i >= 1; i--)
                    {
                        kanal[i] = kanal[i - 1];
                    }
                    kanal[0] = ilkDeger;
                    FarktanGeriDonYerinde(kanal);
                }

                // Kanalı iç içe düzene geri yerleştir
                for (int i = 0; i < uzunluk; i++)
                {
                    cikti[c + i * kanalSayisi] = kanal[i];
                }
            }

            // YAPISAL DOĞRULAMA - kanal katmanı düzeyinde.
            // Uzunluk tablosu kalktığı için çekirdek tek tek artık kontrolü
            // yapmaz; kontrol burada TOPLU yapılır. Geçerli bir akışta tüm
            // kanallar bittiğinde girdinin tamamı tüketilmiş olmalıdır.
            if ((girdi.Length - okumaKonumu) > ARTIK_TOLERANSI)
            {
#if !EMBEDDED_MODE
                throw new ArgumentException(
                    $"Girdi bu kodlayıcıdan çıkmamış görünüyor: {girdi.Length - okumaKonumu} bayt " +
                    "tüketilmeden kaldı.", nameof(girdi));
#endif
            }
        }

        // =================================================================
        // YARDIMCILAR
        // =================================================================

        /// <summary>
        /// Bu kanalda ikinci derece fark birinci dereceden daha mı iyi?
        /// Ardışık farkların toplam mutlak büyüklüğü ile farkların farkının
        /// toplam mutlak büyüklüğü karşılaştırılır; küçük olan kazanır.
        /// Düzgün/doğrusal sinyallerde (sabit hızlı GPS) ikinci derece kazanır,
        /// gürültülü/durağan sinyallerde birinci derece kazanır.
        /// </summary>
        private static bool IkinciDereceDahaIyiMi(scoped ReadOnlySpan<int> kanal)
        {
            if (kanal.Length < 3) return false;

            // Baştan sınırlı bir örneklem yeterli — karar ucuz olmalı
            int ornek = kanal.Length < 512 ? kanal.Length : 512;

            long birinciToplam = 0;
            long ikinciToplam = 0;

            int oncekiFark = unchecked(kanal[1] - kanal[0]);
            birinciToplam += Mutlak(oncekiFark);

            for (int i = 2; i < ornek; i++)
            {
                int fark = unchecked(kanal[i] - kanal[i - 1]);
                birinciToplam += Mutlak(fark);
                ikinciToplam += Mutlak(unchecked(fark - oncekiFark));
                oncekiFark = fark;
            }

            return ikinciToplam < birinciToplam;
        }

        /// <summary>Taşma-güvenli mutlak değer (long'a genişleterek).</summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static long Mutlak(int deger)
        {
            long g = deger;
            return g < 0 ? -g : g;
        }

        /// <summary>Fark akışından orijinali geri kurar (önek toplam, yerinde).</summary>
        private static void FarktanGeriDonYerinde(scoped Span<int> kanal)
        {
            for (int i = 1; i < kanal.Length; i++)
            {
                kanal[i] = unchecked(kanal[i] + kanal[i - 1]);
            }
        }
    }
}
