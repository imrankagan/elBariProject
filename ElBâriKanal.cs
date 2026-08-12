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
            int baslik = 2 + 2 * bayrakBayt + kanalSayisi * 4;
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
            int baslikBoyu = 2 + 2 * bayrakBayt + kanalSayisi * 4;
            if (cikti.Length < baslikBoyu)
            {
                throw new ArgumentException(
                    $"Çıktı tamponu başlık için bile yetersiz. En az {baslikBoyu} bayt gerekli.", nameof(cikti));
            }

            cikti[0] = (byte)kanalSayisi;
            cikti[1] = (byte)bayrakBayt;

            Span<byte> ikinciDereceBayraklari = cikti.Slice(2, bayrakBayt);
            Span<byte> hamGecisBayraklari = cikti.Slice(2 + bayrakBayt, bayrakBayt);
            Span<byte> boyutAlani = cikti.Slice(2 + 2 * bayrakBayt, kanalSayisi * 4);

            ikinciDereceBayraklari.Clear();
            hamGecisBayraklari.Clear();
            boyutAlani.Clear();

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
                bool ikinciDerece = IkinciDereceDahaIyiMi(kanal);
                if (ikinciDerece)
                {
                    FarkaCevirYerinde(kanal);
                    ikinciDereceBayraklari[c >> 3] |= (byte)(1 << (c & 7));
                }

                int hamBayt = uzunluk * sizeof(int);

                // 3) Sıkıştırmayı dene
                int sonuc = -1;
                if (cikti.Length - yazmaKonumu >= hamBayt + 64)
                {
                    sonuc = ElBâri.ElKâbıd(kanal, cikti.Slice(yazmaKonumu));
                }

                if (sonuc > 0 && sonuc < hamBayt)
                {
                    // Sıkıştırma kazançlı: olduğu gibi bırak
                    MemoryMarshal.Write(boyutAlani.Slice(c * 4, 4), in sonuc);
                    yazmaKonumu += sonuc;
                }
                else
                {
                    // 4) HAM GEÇİŞ: kazanç yok ya da reddedildi -> veriyi ham yaz.
                    // Not: kanal ikinci dereceye çevrildiyse fark akışı ham yazılır;
                    // bayrak zaten işaretli olduğu için çözücü doğru geri kurar.
                    if (yazmaKonumu + hamBayt > cikti.Length)
                    {
                        throw new ArgumentException(
                            $"Çıktı tamponu çok küçük. EnKotuDurumCiktiBoyutu({toplam}, {kanalSayisi}) kullanın.",
                            nameof(cikti));
                    }

                    for (int i = 0; i < uzunluk; i++)
                    {
                        int deger = kanal[i];
                        MemoryMarshal.Write(cikti.Slice(yazmaKonumu + i * 4, 4), in deger);
                    }

                    MemoryMarshal.Write(boyutAlani.Slice(c * 4, 4), in hamBayt);
                    hamGecisBayraklari[c >> 3] |= (byte)(1 << (c & 7));
                    yazmaKonumu += hamBayt;
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

            int baslikBoyu = 2 + 2 * bayrakBayt + kanalSayisi * 4;
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
            ReadOnlySpan<byte> boyutAlani = girdi.Slice(2 + 2 * bayrakBayt, kanalSayisi * 4);

            int okumaKonumu = baslikBoyu;

            for (int c = 0; c < kanalSayisi; c++)
            {
                int uzunluk = KanalUzunlugu(toplam, kanalSayisi, c);
                if (uzunluk == 0) continue;

                int yukBoyutu = MemoryMarshal.Read<int>(boyutAlani.Slice(c * 4, 4));
                if (yukBoyutu <= 0 || okumaKonumu + yukBoyutu > girdi.Length)
                {
                    throw new ArgumentException(
                        $"Girdi bozuk: kanal {c} yük boyutu geçersiz ({yukBoyutu}).", nameof(girdi));
                }

                bool hamGecis = (hamGecisBayraklari[c >> 3] & (1 << (c & 7))) != 0;
                bool ikinciDerece = (ikinciDereceBayraklari[c >> 3] & (1 << (c & 7))) != 0;

                Span<int> kanal = calismaAlani.Slice(0, uzunluk);

                if (hamGecis)
                {
                    for (int i = 0; i < uzunluk; i++)
                    {
                        kanal[i] = MemoryMarshal.Read<int>(girdi.Slice(okumaKonumu + i * 4, 4));
                    }
                }
                else
                {
                    ElBâri.ElBâsıt(girdi.Slice(okumaKonumu, yukBoyutu), kanal);
                }

                okumaKonumu += yukBoyutu;

                if (ikinciDerece)
                {
                    FarktanGeriDonYerinde(kanal);
                }

                // Kanalı iç içe düzene geri yerleştir
                for (int i = 0; i < uzunluk; i++)
                {
                    cikti[c + i * kanalSayisi] = kanal[i];
                }
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

        /// <summary>[a,b,c,d] -> [a, b-a, c-b, d-c] (yerinde, ilk eleman mutlak referans).</summary>
        private static void FarkaCevirYerinde(scoped Span<int> kanal)
        {
            for (int i = kanal.Length - 1; i >= 1; i--)
            {
                kanal[i] = unchecked(kanal[i] - kanal[i - 1]);
            }
        }

        /// <summary>FarkaCevirYerinde işleminin tersi (önek toplam).</summary>
        private static void FarktanGeriDonYerinde(scoped Span<int> kanal)
        {
            for (int i = 1; i < kanal.Length; i++)
            {
                kanal[i] = unchecked(kanal[i] + kanal[i - 1]);
            }
        }
    }
}
