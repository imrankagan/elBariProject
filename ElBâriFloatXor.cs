#nullable enable

using System;
using System.Runtime.InteropServices;

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ KAYIPSIZ FLOAT SIKIŞTIRMA (XOR tabanlı)
    // =================================================================
    //
    // Telif Hakkı (c) 2025 İmran Kağan. Tüm Hakları Saklıdır.
    //
    // NEDEN VAR:
    // Kuantalama katmanı ([ElBâriFloat]) KAYIPLIDIR. Tam değerin korunması
    // gereken durumlar için (ham sensör kaydı, uçuş sonrası analiz, adli
    // inceleme, kriptografik malzeme) kayıpsız bir yol gerekir.
    //
    // NASIL ÇALIŞIR:
    // Ardışık iki float'ın BİT DESENİ XOR'lanır. Birbirine yakın değerlerde
    // işaret, üstel kısım ve mantisin üst bitleri aynıdır; dolayısıyla XOR
    // sonucunun başında ve sonunda çok sayıda sıfır bulunur. Yalnızca
    // ortadaki "anlamlı" bitler yazılır.
    //
    // Özel durum: değer hiç değişmemişse XOR sıfırdır ve tek bir bit yeter.
    // Durağan telemetride (batarya, sabit irtifa) bu çok sık görülür.
    //
    // Literatürde Gorilla (Facebook, 2015) ve Chimp olarak bilinir.
    //
    // -----------------------------------------------------------------
    // ⚠️ BEKLENTİ YÖNETİMİ — DÜRÜST UYARI
    // -----------------------------------------------------------------
    // Kayıpsız float sıkıştırma, GÜRÜLTÜLÜ sensör verisinde az kazandırır.
    // Sebep basit: gürültü mantisin alt bitlerini her örneklemde değiştirir
    // ve bu bitler tanımları gereği sıkıştırılamaz. Tipik kazanç %10-40.
    //
    // Buna karşılık KUANTALAMA aynı veride kat kat iyi sonuç verir, çünkü
    // gürültüyü baştan atar.
    //
    // KURAL: Tam değer gerekmiyorsa kuantalama kullanın. Bu katman
    // "mecbur kalınca" içindir.
    // -----------------------------------------------------------------
    //
    // BİT BİÇİMİ (bit akışı, düşük bitten yükseğe)
    //
    //   İlk değer : 32 bit ham (bit deseni olduğu gibi)
    //
    //   Sonraki her değer için:
    //     XOR == 0  ->  1 bit: 0
    //     XOR != 0  ->  1 bit: 1, ardından:
    //         önceki pencere kullanılabiliyorsa (BS >= öncekiBS ve
    //         SS >= öncekiSS):
    //             1 bit: 0
    //             (32 - öncekiBS - öncekiSS) bit: anlamlı bitler
    //         aksi hâlde:
    //             1 bit: 1
    //             5 bit: BS  (baştaki sıfır sayısı, 0..31)
    //             5 bit: anlamlı_uzunluk - 1  (0..31 => 1..32)
    //             anlamlı_uzunluk bit: anlamlı bitler
    // =================================================================
    public static class ElBâriFloatXor
    {
        /// <summary>Tek akış için güvenli en kötü durum çıktı boyutu (bayt).</summary>
        public static int EnKotuDurumBoyutu(int adet)
        {
            if (adet <= 0) return 8;
            // Değer başına en fazla 1+1+5+5+32 = 44 bit = 5.5 bayt; 6 ile pay bırakılır
            return (adet * 6) + 16;
        }

        /// <summary>Çok kanallı sürüm için en kötü durum çıktı boyutu (bayt).</summary>
        public static int KanalEnKotuDurumBoyutu(int elemanSayisi, int kanalSayisi)
        {
            int bayrakBayt = (kanalSayisi + 7) / 8;
            int baslik = 2 + bayrakBayt + (kanalSayisi * 4);
            return baslik + (elemanSayisi * 6) + (kanalSayisi * 16) + 64;
        }

        // =================================================================
        // BİT SAYMA
        // =================================================================

        /// <summary>Baştaki (en anlamlı taraftaki) sıfır sayısı. Sıfır için 32.</summary>
        private static int BastaSifir(uint deger)
        {
            if (deger == 0u) return 32;
            int n = 0;
            uint maske = 0x80000000u;
            while ((deger & maske) == 0u) { n++; maske >>= 1; }
            return n;
        }

        /// <summary>Sondaki (en az anlamlı taraftaki) sıfır sayısı. Sıfır için 32.</summary>
        private static int SondaSifir(uint deger)
        {
            if (deger == 0u) return 32;
            int n = 0;
            while ((deger & 1u) == 0u) { n++; deger >>= 1; }
            return n;
        }

        // =================================================================
        // KODLAYICI
        // =================================================================

        /// <summary>
        /// Bit yazıcı. `ref struct` olmak zorunda: içinde Span tutuyor ve
        /// yerel fonksiyonlar Span parametresi yakalayamıyor (CS9108).
        /// </summary>
        private ref struct BitYazici
        {
            private readonly Span<byte> _cikti;
            private ulong _tampon;
            private int _bitSayisi;
            private int _baytIndeksi;

            public BitYazici(Span<byte> cikti)
            {
                _cikti = cikti;
                _tampon = 0;
                _bitSayisi = 0;
                _baytIndeksi = 0;
            }

            public readonly int BaytSayisi => _baytIndeksi;

            private void Bosalt()
            {
                while (_bitSayisi >= 8)
                {
                    if (_baytIndeksi >= _cikti.Length)
                    {
                        throw new ArgumentException(
                            "Çıktı tamponu çok küçük. EnKotuDurumBoyutu kullanın.");
                    }
                    _cikti[_baytIndeksi++] = (byte)(_tampon & 0xFF);
                    _tampon >>= 8;
                    _bitSayisi -= 8;
                }
            }

            public void Yaz(uint deger, int adet)
            {
                if (adet <= 0) return;
                uint maske = adet >= 32 ? 0xFFFFFFFFu : ((1u << adet) - 1u);
                _tampon |= ((ulong)(deger & maske)) << _bitSayisi;
                _bitSayisi += adet;
                Bosalt();
            }

            /// <summary>Kalan bitleri son bir bayta yazar ve toplam boyutu döndürür.</summary>
            public int Kapat()
            {
                if (_bitSayisi > 0)
                {
                    if (_baytIndeksi >= _cikti.Length)
                    {
                        throw new ArgumentException("Çıktı tamponu çok küçük (son boşaltma).");
                    }
                    _cikti[_baytIndeksi++] = (byte)(_tampon & 0xFF);
                }
                return _baytIndeksi;
            }
        }

        /// <summary>Bit okuyucu. Yazıcıyla aynı nedenle `ref struct`.</summary>
        private ref struct BitOkuyucu
        {
            private readonly ReadOnlySpan<byte> _girdi;
            private ulong _tampon;
            private int _bitSayisi;
            private int _baytIndeksi;

            public BitOkuyucu(ReadOnlySpan<byte> girdi)
            {
                _girdi = girdi;
                _tampon = 0;
                _bitSayisi = 0;
                _baytIndeksi = 0;
            }

            public uint Oku(int adet)
            {
                if (adet <= 0) return 0u;

                while (_bitSayisi < adet)
                {
                    if (_baytIndeksi >= _girdi.Length)
                    {
                        throw new ArgumentException("Girdi tamponu sonuna ulaşıldı — veri bozuk.");
                    }
                    _tampon |= ((ulong)_girdi[_baytIndeksi++]) << _bitSayisi;
                    _bitSayisi += 8;
                }

                uint maske = adet >= 32 ? 0xFFFFFFFFu : ((1u << adet) - 1u);
                uint sonuc = (uint)(_tampon & maske);
                _tampon >>= adet;
                _bitSayisi -= adet;
                return sonuc;
            }
        }

        /// <summary>
        /// Float dizisini KAYIPSIZ sıkıştırır.
        /// </summary>
        /// <returns>Yazılan bayt sayısı. Boş girdide 0.</returns>
        public static int ElKâbıdXor(scoped ReadOnlySpan<float> hamVeri,
                                     scoped Span<byte> cikti)
        {
            if (hamVeri.IsEmpty) return 0;

            BitYazici y = new BitYazici(cikti);

            uint oncekiBit = BitConverter.SingleToUInt32Bits(hamVeri[0]);
            int oncekiBs = 32;   // 32 = "geçerli pencere yok"
            int oncekiSs = 32;

            y.Yaz(oncekiBit, 32);

            for (int i = 1; i < hamVeri.Length; i++)
            {
                uint simdikiBit = BitConverter.SingleToUInt32Bits(hamVeri[i]);
                uint fark = simdikiBit ^ oncekiBit;

                if (fark == 0u)
                {
                    y.Yaz(0u, 1);
                }
                else
                {
                    int bs = BastaSifir(fark);
                    int ss = SondaSifir(fark);

                    y.Yaz(1u, 1);

                    if (bs >= oncekiBs && ss >= oncekiSs)
                    {
                        // Önceki pencere yeterli: uzunluk bilgisi tekrar yazılmaz
                        int uzunluk = 32 - oncekiBs - oncekiSs;
                        y.Yaz(0u, 1);
                        y.Yaz(fark >> oncekiSs, uzunluk);
                    }
                    else
                    {
                        int uzunluk = 32 - bs - ss;
                        y.Yaz(1u, 1);
                        y.Yaz((uint)bs, 5);
                        y.Yaz((uint)(uzunluk - 1), 5);
                        y.Yaz(fark >> ss, uzunluk);

                        oncekiBs = bs;
                        oncekiSs = ss;
                    }
                }

                oncekiBit = simdikiBit;
            }

            return y.Kapat();
        }

        // =================================================================
        // ÇÖZÜCÜ
        // =================================================================

        /// <summary>ElKâbıdXor çıktısını açar.</summary>
        public static void ElBâsıtXor(scoped ReadOnlySpan<byte> girdi,
                                      scoped Span<float> cikti)
        {
            if (cikti.IsEmpty) return;
            if (girdi.Length < 4)
            {
                throw new ArgumentException("Girdi tamponu çok küçük.", nameof(girdi));
            }

            BitOkuyucu o = new BitOkuyucu(girdi);

            uint oncekiBit = o.Oku(32);
            int oncekiBs = 32;
            int oncekiSs = 32;
            cikti[0] = BitConverter.UInt32BitsToSingle(oncekiBit);

            for (int i = 1; i < cikti.Length; i++)
            {
                uint bayrak = o.Oku(1);
                uint simdikiBit;

                if (bayrak == 0u)
                {
                    simdikiBit = oncekiBit;
                }
                else
                {
                    uint pencere = o.Oku(1);

                    if (pencere == 0u)
                    {
                        if (oncekiBs >= 32 || oncekiSs >= 32)
                        {
                            throw new ArgumentException(
                                "Girdi bozuk: geçerli pencere yokken pencere tekrarı istendi.",
                                nameof(girdi));
                        }
                        int uzunluk = 32 - oncekiBs - oncekiSs;
                        uint anlamli = o.Oku(uzunluk);
                        simdikiBit = oncekiBit ^ (anlamli << oncekiSs);
                    }
                    else
                    {
                        int bs = (int)o.Oku(5);
                        int uz = (int)o.Oku(5) + 1;
                        int ss = 32 - bs - uz;

                        if (ss < 0 || bs > 31)
                        {
                            throw new ArgumentException(
                                "Girdi bozuk: geçersiz pencere tanımı.", nameof(girdi));
                        }

                        uint anlamli = o.Oku(uz);
                        simdikiBit = oncekiBit ^ (anlamli << ss);

                        oncekiBs = bs;
                        oncekiSs = ss;
                    }
                }

                cikti[i] = BitConverter.UInt32BitsToSingle(simdikiBit);
                oncekiBit = simdikiBit;
            }
        }

        // =================================================================
        // ÇOK KANALLI SARMALAYICI
        // =================================================================
        //
        // İç içe geçmiş float akışını kanallara ayırıp her kanalı kendi
        // içinde XOR ile sıkıştırır. Aynı kanalın ardışık değerleri
        // birbirine benzer; karışık akışta ise her adımda bambaşka bir
        // büyüklüğe geçiyor olurduk ve XOR neredeyse hiç sıfır üretmezdi.
        //
        // BİÇİM:
        //   [0]              : kanal sayısı K (1 bayt)
        //   [1]              : bayrak bayt sayısı B = ceil(K/8)
        //   [2 .. 2+B)       : ham-geçiş bayrakları (kanal başına 1 bit)
        //   [2+B .. 2+B+4K)  : kanal başına yük boyutu (int32, little-endian)
        //   sonrası          : kanal yükleri, sırayla
        // =================================================================

        /// <summary>Çok kanallı float akışını kanal kanal KAYIPSIZ sıkıştırır.</summary>
        public static int ElKâbıdXorKanal(scoped ReadOnlySpan<float> hamVeri,
                                          int kanalSayisi,
                                          scoped Span<float> calismaAlani,
                                          scoped Span<byte> cikti)
        {
            if (kanalSayisi < 1 || kanalSayisi > ElBâriKanal.MAKS_KANAL)
            {
                throw new ArgumentOutOfRangeException(nameof(kanalSayisi), kanalSayisi,
                    $"Kanal sayısı 1 ile {ElBâriKanal.MAKS_KANAL} arasında olmalı.");
            }
            if (hamVeri.IsEmpty) return 0;

            int toplam = hamVeri.Length;
            int gerekliCalisma = (toplam + kanalSayisi - 1) / kanalSayisi;
            if (calismaAlani.Length < gerekliCalisma)
            {
                throw new ArgumentException(
                    $"Çalışma alanı çok küçük. En az {gerekliCalisma} float gerekli.",
                    nameof(calismaAlani));
            }

            int bayrakBayt = (kanalSayisi + 7) / 8;
            int baslikBoyu = 2 + bayrakBayt + (kanalSayisi * 4);
            if (cikti.Length < baslikBoyu)
            {
                throw new ArgumentException("Çıktı tamponu başlık için yetersiz.", nameof(cikti));
            }

            cikti[0] = (byte)kanalSayisi;
            cikti[1] = (byte)bayrakBayt;

            Span<byte> hamBayraklari = cikti.Slice(2, bayrakBayt);
            Span<byte> boyutAlani = cikti.Slice(2 + bayrakBayt, kanalSayisi * 4);
            hamBayraklari.Clear();
            boyutAlani.Clear();

            int yazmaKonumu = baslikBoyu;

            for (int c = 0; c < kanalSayisi; c++)
            {
                if (c >= toplam) continue;
                int uzunluk = (toplam - c + kanalSayisi - 1) / kanalSayisi;

                Span<float> kanal = calismaAlani.Slice(0, uzunluk);
                for (int i = 0; i < uzunluk; i++)
                {
                    kanal[i] = hamVeri[c + (i * kanalSayisi)];
                }

                int hamBayt = uzunluk * 4;
                int sonuc = -1;

                if (cikti.Length - yazmaKonumu >= hamBayt + 64)
                {
                    sonuc = ElKâbıdXor(kanal, cikti.Slice(yazmaKonumu));
                }

                if (sonuc > 0 && sonuc < hamBayt)
                {
                    MemoryMarshal.Write(boyutAlani.Slice(c * 4, 4), in sonuc);
                    yazmaKonumu += sonuc;
                }
                else
                {
                    // Kazanç yok: ham yaz (kayıpsızlık her koşulda korunur)
                    if (yazmaKonumu + hamBayt > cikti.Length)
                    {
                        throw new ArgumentException(
                            "Çıktı tamponu çok küçük. KanalEnKotuDurumBoyutu kullanın.", nameof(cikti));
                    }
                    for (int i = 0; i < uzunluk; i++)
                    {
                        uint bitDeseni = BitConverter.SingleToUInt32Bits(kanal[i]);
                        MemoryMarshal.Write(cikti.Slice(yazmaKonumu + (i * 4), 4), in bitDeseni);
                    }
                    MemoryMarshal.Write(boyutAlani.Slice(c * 4, 4), in hamBayt);
                    hamBayraklari[c >> 3] |= (byte)(1 << (c & 7));
                    yazmaKonumu += hamBayt;
                }
            }

            return yazmaKonumu;
        }

        /// <summary>Çok kanallı kayıpsız float çıktısını açar.</summary>
        public static void ElBâsıtXorKanal(scoped ReadOnlySpan<byte> girdi,
                                           scoped Span<float> calismaAlani,
                                           scoped Span<float> cikti)
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
                throw new ArgumentException("Girdi başlığı bozuk.", nameof(girdi));
            }

            int baslikBoyu = 2 + bayrakBayt + (kanalSayisi * 4);
            if (girdi.Length < baslikBoyu)
            {
                throw new ArgumentException("Girdi tamponu başlık için çok küçük.", nameof(girdi));
            }

            int toplam = cikti.Length;
            int gerekliCalisma = (toplam + kanalSayisi - 1) / kanalSayisi;
            if (calismaAlani.Length < gerekliCalisma)
            {
                throw new ArgumentException(
                    $"Çalışma alanı çok küçük. En az {gerekliCalisma} float gerekli.",
                    nameof(calismaAlani));
            }

            ReadOnlySpan<byte> hamBayraklari = girdi.Slice(2, bayrakBayt);
            ReadOnlySpan<byte> boyutAlani = girdi.Slice(2 + bayrakBayt, kanalSayisi * 4);
            int okumaKonumu = baslikBoyu;

            for (int c = 0; c < kanalSayisi; c++)
            {
                if (c >= toplam) continue;
                int uzunluk = (toplam - c + kanalSayisi - 1) / kanalSayisi;

                int yukBoyutu = MemoryMarshal.Read<int>(boyutAlani.Slice(c * 4, 4));
                if (yukBoyutu <= 0 || okumaKonumu + yukBoyutu > girdi.Length)
                {
                    throw new ArgumentException(
                        $"Girdi bozuk: kanal {c} yük boyutu geçersiz ({yukBoyutu}).", nameof(girdi));
                }

                bool hamGecis = (hamBayraklari[c >> 3] & (1 << (c & 7))) != 0;
                Span<float> kanal = calismaAlani.Slice(0, uzunluk);

                if (hamGecis)
                {
                    if (yukBoyutu < uzunluk * 4)
                    {
                        throw new ArgumentException($"Girdi bozuk: kanal {c} ham yükü eksik.", nameof(girdi));
                    }
                    for (int i = 0; i < uzunluk; i++)
                    {
                        uint bitDeseni = MemoryMarshal.Read<uint>(girdi.Slice(okumaKonumu + (i * 4), 4));
                        kanal[i] = BitConverter.UInt32BitsToSingle(bitDeseni);
                    }
                }
                else
                {
                    ElBâsıtXor(girdi.Slice(okumaKonumu, yukBoyutu), kanal);
                }

                okumaKonumu += yukBoyutu;

                for (int i = 0; i < uzunluk; i++)
                {
                    cikti[c + (i * kanalSayisi)] = kanal[i];
                }
            }
        }
    }
}
