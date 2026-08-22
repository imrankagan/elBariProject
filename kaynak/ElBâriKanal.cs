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
    //   [1]                : bayrak bayt sayısı B      (1 bayt, B = ceil(K/8))
    //   [2 .. 2+B)         : ikinci-derece bayrakları  (kanal başına 1 bit)
    //   [2+B .. 2+2B)      : ham-geçiş bayrakları      (kanal başına 1 bit)
    //   [2+2B .. 2+2B+4K)  : kanal başına yük boyutu   (int32)
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
            // Biçim sürümü 4: kanal başına 4 baytlık uzunluk tablosu KALDIRILDI.
            // Kanallar ardışık çözülür ve çekirdek kendi tüketimini bildirir.
            int baslik = 2 + 2 * bayrakBayt;
            // eleman*4 (ham) + eleman/2 (paketleme payı) + kanal başına referans/pay
            return baslik + elemanSayisi * 4 + elemanSayisi / 2 + kanalSayisi * 68 + 64;
        }

        /// <summary>
        /// Gereken çalışma alanı (int cinsinden): en uzun kanalın eleman sayısı.
        /// </summary>
        public static int GerekliCalismaAlani(int elemanSayisi, int kanalSayisi)
            => kanalSayisi <= 0 ? 0 : (elemanSayisi + kanalSayisi - 1) / kanalSayisi;

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
            int baslikBoyu = 2 + 2 * bayrakBayt;
            if (cikti.Length < baslikBoyu)
            {
                throw new ArgumentException(
                    $"Çıktı tamponu başlık için bile yetersiz. En az {baslikBoyu} bayt gerekli.", nameof(cikti));
            }

            cikti[0] = (byte)kanalSayisi;
            cikti[1] = (byte)bayrakBayt;

            Span<byte> ikinciDereceBayraklari = cikti.Slice(2, bayrakBayt);
            Span<byte> hamGecisBayraklari = cikti.Slice(2 + bayrakBayt, bayrakBayt);

            ikinciDereceBayraklari.Clear();
            hamGecisBayraklari.Clear();


            int yazmaKonumu = baslikBoyu;

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
                    ilkDeger = kanal[0];
                    // Yerinde sola kaydırarak fark akışı üret: [d1, d2, ..., d(m-1)]
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

                int onEkBoyu = ikinciDerece ? 4 : 0;
                int hamBayt = yuk.Length * sizeof(int);

                if (yazmaKonumu + onEkBoyu > cikti.Length)
                {
                    throw new ArgumentException(
                        $"Çıktı tamponu çok küçük. EnKotuDurumCiktiBoyutu({toplam}, {kanalSayisi}) kullanın.",
                        nameof(cikti));
                }

                if (ikinciDerece)
                {
                    MemoryMarshal.Write(cikti.Slice(yazmaKonumu, 4), in ilkDeger);
                }

                int yukKonumu = yazmaKonumu + onEkBoyu;

                // 3) Sıkıştırmayı dene
                int sonuc = -1;
                if (yuk.Length > 0 && cikti.Length - yukKonumu >= hamBayt + 64)
                {
                    sonuc = ElBâri.ElKâbıd(yuk, cikti.Slice(yukKonumu));
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

                    for (int i = 0; i < yuk.Length; i++)
                    {
                        int deger = yuk[i];
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
            int bayrakBayt = girdi[1];

            if (kanalSayisi < 1 || bayrakBayt != (kanalSayisi + 7) / 8)
            {
                throw new ArgumentException("Girdi başlığı bozuk (kanal sayısı/bayrak boyutu tutarsız).", nameof(girdi));
            }

            int baslikBoyu = 2 + 2 * bayrakBayt;
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

            ReadOnlySpan<byte> ikinciDereceBayraklari = girdi.Slice(2, bayrakBayt);
            ReadOnlySpan<byte> hamGecisBayraklari = girdi.Slice(2 + bayrakBayt, bayrakBayt);
            int okumaKonumu = baslikBoyu;

            for (int c = 0; c < kanalSayisi; c++)
            {
                int uzunluk = KanalUzunlugu(toplam, kanalSayisi, c);
                if (uzunluk == 0) continue;

                bool hamGecis = (hamGecisBayraklari[c >> 3] & (1 << (c & 7))) != 0;
                bool ikinciDerece = (ikinciDereceBayraklari[c >> 3] & (1 << (c & 7))) != 0;

                Span<int> kanal = calismaAlani.Slice(0, uzunluk);

                // İkinci derece kanallarda yükün başında mutlak ilk değer bulunur.
                int onEkBoyu = ikinciDerece ? 4 : 0;
                if (okumaKonumu + onEkBoyu > girdi.Length)
                {
                    throw new ArgumentException($"Girdi bozuk: kanal {c} yükü eksik.", nameof(girdi));
                }

                int ilkDeger = 0;
                if (ikinciDerece)
                {
                    ilkDeger = MemoryMarshal.Read<int>(girdi.Slice(okumaKonumu, 4));
                }

                int yukKonumu = okumaKonumu + onEkBoyu;
                int icBoyut = girdi.Length - yukKonumu;
                int hedefUzunluk = ikinciDerece ? uzunluk - 1 : uzunluk;
                int yukBoyutu = 0;

                if (hedefUzunluk > 0)
                {
                    Span<int> hedef = kanal.Slice(0, hedefUzunluk);

                    if (hamGecis)
                    {
                        // Ham geçişin boyutu hesaplanabilir; tabloya gerek yok.
                        if (icBoyut < hedefUzunluk * 4)
                        {
                            throw new ArgumentException($"Girdi bozuk: kanal {c} ham yükü eksik.", nameof(girdi));
                        }
                        for (int i = 0; i < hedefUzunluk; i++)
                        {
                            hedef[i] = MemoryMarshal.Read<int>(girdi.Slice(yukKonumu + i * 4, 4));
                        }
                        yukBoyutu = hedefUzunluk * 4;
                    }
                    else
                    {
                        // Sıkıştırılmış kanal: çekirdek kaç bayt tükettiğini
                        // bildirir, böylece bir sonraki kanalın başlangıcı
                        // uzunluk tablosu olmadan bulunur (biçim sürümü 4).
                        yukBoyutu = ElBâri.ElBâsıtAkis(girdi.Slice(yukKonumu, icBoyut), hedef);
                    }
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
