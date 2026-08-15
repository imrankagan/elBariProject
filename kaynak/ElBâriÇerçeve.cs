#nullable enable

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ ÇERÇEVE KATMANI — PAKET KAYBINA DAYANIKLILIK
    // =================================================================
    //
    // Telif Hakkı (c) 2025 İmran Kağan. Tüm Hakları Saklıdır.
    //
    // NEDEN VAR:
    // Fark (delta) kodlamanın ölümcül zayıflığı zincirleme bağımlılıktır:
    // her değer bir öncekine dayanır. Tek bir paket düşerse ondan SONRAKİ
    // TÜM veriler çözülemez hale gelir. Standart sıkıştırıcılar bunu
    // çözmez, çünkü altlarında kayıpsız bir taşıma (TCP) varsayarlar.
    //
    // İHA telemetrisi ise kayıplı bir telsiz linki üzerinden gider.
    // Paket düşmesi istisna değil, NORMAL çalışma koşuludur.
    //
    // ÇÖZÜM:
    // Akış, her biri BAĞIMSIZ ÇÖZÜLEBİLİR çerçevelere bölünür:
    //   - Her çerçeve kendi mutlak referansını taşır (zincir kırılır).
    //   - Her çerçevede sıra numarası vardır (hangi kayıtlar eksik, bilinir).
    //   - Her çerçeve CRC32 ile korunur (bozulma sessizce geçmez).
    //   - Çerçeveler sırasız gelebilir, tek tek doğrulanıp çözülebilir.
    //
    // Sonuç: N. çerçeve kaybolursa SADECE o çerçevenin kayıtları kaybolur.
    // Hata yayılımı bir çerçeve ile SINIRLIDIR.
    //
    // KULLANIM:
    // Çağıran, link MTU'suna göre çerçeve başına kayıt sayısını seçer ve
    // her çerçeveyi ayrı bir pakette gönderir. Alıcı gelen her paketi
    // ÇerçeveOku ile bağımsızca çözer; eksik sıra numaraları kayıptır.
    //
    // BİÇİM (çerçeve başlığı 16 bayt):
    //   [0..1]   : sihirli sayı 0xEB 0x71
    //   [2]      : sürüm (1)
    //   [3]      : bayraklar (ileride kullanım için 0)
    //   [4..7]   : CRC32  ([8..son] aralığı üzerinden)
    //   [8..11]  : çerçeve sıra numarası (uint32)
    //   [12..15] : bu çerçevedeki KAYIT sayısı (int32)
    //   [16..]   : ElBâriKanal yükü (kendi başlığı dahil)
    // =================================================================
    public static class ElBâriÇerçeve
    {
        /// <summary>Çerçeve başlığının bayt uzunluğu.</summary>
        public const int BASLIK_BOYUTU = 16;

        private const byte SIHIR_0 = 0xEB;
        private const byte SIHIR_1 = 0x71;
        private const byte SURUM = 2;

        /// <summary>
        /// Bir çerçeve için güvenli en kötü durum çıktı boyutu.
        /// </summary>
        public static int EnKotuDurumCerceveBoyutu(int kayitSayisi, int kanalSayisi)
            => BASLIK_BOYUTU + ElBâriKanal.EnKotuDurumCiktiBoyutu(kayitSayisi * kanalSayisi, kanalSayisi);

        /// <summary>Çerçeve yazarken/okurken gereken çalışma alanı (int cinsinden).</summary>
        public static int GerekliCalismaAlani(int kayitSayisi, int kanalSayisi)
            => ElBâriKanal.GerekliCalismaAlani(kayitSayisi * kanalSayisi, kanalSayisi);

        // =================================================================
        // YAZMA
        // =================================================================
        /// <summary>
        /// Tek bir bağımsız çerçeve yazar. Çağıran bu çerçeveyi tek bir pakette gönderir.
        /// </summary>
        /// <param name="kayitlar">İç içe kayıt akışı; uzunluğu kanalSayisi'nın katı olmalı</param>
        /// <param name="kanalSayisi">Kayıt başına alan sayısı</param>
        /// <param name="siraNo">Çerçeve sıra numarası (her çerçevede birer artırılır)</param>
        /// <param name="calismaAlani">En az GerekliCalismaAlani kadar</param>
        /// <param name="cikti">En az EnKotuDurumCerceveBoyutu kadar</param>
        /// <returns>Yazılan bayt sayısı</returns>
        public static int CerceveYaz(
            scoped ReadOnlySpan<int> kayitlar,
            int kanalSayisi,
            uint siraNo,
            scoped Span<int> calismaAlani,
            scoped Span<byte> cikti)
        {
            if (kanalSayisi < 1 || kanalSayisi > ElBâriKanal.MAKS_KANAL)
            {
                throw new ArgumentOutOfRangeException(nameof(kanalSayisi), kanalSayisi,
                    $"Kanal sayısı 1 ile {ElBâriKanal.MAKS_KANAL} arasında olmalı.");
            }

            if (kayitlar.Length % kanalSayisi != 0)
            {
                throw new ArgumentException(
                    $"Kayıt akışının uzunluğu ({kayitlar.Length}) kanal sayısının ({kanalSayisi}) katı olmalı. " +
                    "Çerçeveler tam kayıt sınırında bölünmelidir.", nameof(kayitlar));
            }

            if (cikti.Length < BASLIK_BOYUTU)
            {
                throw new ArgumentException(
                    $"Çıktı tamponu başlık için yetersiz ({BASLIK_BOYUTU} bayt gerekli).", nameof(cikti));
            }

            int kayitSayisi = kayitlar.Length / kanalSayisi;

            // Yük: kanal katmanı ile sıkıştır
            int yukBoyutu = ElBâriKanal.ElKâbıdKanal(
                kayitlar, kanalSayisi, calismaAlani, cikti.Slice(BASLIK_BOYUTU));

            // Başlık
            cikti[0] = SIHIR_0;
            cikti[1] = SIHIR_1;
            cikti[2] = SURUM;
            cikti[3] = 0;
            MemoryMarshal.Write(cikti.Slice(8, 4), in siraNo);
            MemoryMarshal.Write(cikti.Slice(12, 4), in kayitSayisi);

            // CRC: [8..son] (sıra no + kayıt sayısı + yük)
            int toplam = BASLIK_BOYUTU + yukBoyutu;
            uint crc = Crc32(cikti.Slice(8, toplam - 8));
            MemoryMarshal.Write(cikti.Slice(4, 4), in crc);

            return toplam;
        }

        // =================================================================
        // OKUMA
        // =================================================================

        /// <summary>
        /// Çerçeve sağlam mı? (sihirli sayı + sürüm + CRC). Bozuk paket sessizce geçmez.
        /// </summary>
        public static bool CerceveGecerliMi(scoped ReadOnlySpan<byte> cerceve)
        {
            if (cerceve.Length < BASLIK_BOYUTU) return false;
            if (cerceve[0] != SIHIR_0 || cerceve[1] != SIHIR_1) return false;
            if (cerceve[2] != SURUM) return false;
            // [3] ayrılmış alan: CRC kapsamı dışında olduğu için burada doğrulanır,
            // aksi halde bu bayta düşen bir bit bozulması fark edilmeden geçerdi.
            if (cerceve[3] != 0) return false;

            uint beklenen = MemoryMarshal.Read<uint>(cerceve.Slice(4, 4));
            uint hesaplanan = Crc32(cerceve.Slice(8));
            return beklenen == hesaplanan;
        }

        /// <summary>Çerçevenin sıra numarasını okur (doğrulama yapmadan).</summary>
        public static uint CerceveSiraNo(scoped ReadOnlySpan<byte> cerceve)
            => MemoryMarshal.Read<uint>(cerceve.Slice(8, 4));

        /// <summary>Çerçevedeki kayıt sayısını okur (doğrulama yapmadan).</summary>
        public static int CerceveKayitSayisi(scoped ReadOnlySpan<byte> cerceve)
            => MemoryMarshal.Read<int>(cerceve.Slice(12, 4));

        /// <summary>
        /// Tek bir çerçeveyi bağımsız olarak çözer. Diğer çerçevelere ihtiyaç duymaz.
        /// </summary>
        /// <returns>
        /// true: çerçeve geçerli ve çözüldü.
        /// false: çerçeve bozuk/eksik — çağıran bu paketi atmalı (kayıt kaybı olarak sayar).
        /// </returns>
        public static bool CerceveOku(
            scoped ReadOnlySpan<byte> cerceve,
            int kanalSayisi,
            scoped Span<int> calismaAlani,
            scoped Span<int> cikti,
            out uint siraNo,
            out int kayitSayisi)
        {
            siraNo = 0;
            kayitSayisi = 0;

            if (!CerceveGecerliMi(cerceve)) return false;

            siraNo = CerceveSiraNo(cerceve);
            kayitSayisi = CerceveKayitSayisi(cerceve);

            if (kayitSayisi < 0) { kayitSayisi = 0; return false; }

            int elemanSayisi = kayitSayisi * kanalSayisi;
            if (elemanSayisi > cikti.Length) return false;

            if (elemanSayisi == 0) return true;

            try
            {
                ElBâriKanal.ElBâsıtKanal(
                    cerceve.Slice(BASLIK_BOYUTU), calismaAlani, cikti.Slice(0, elemanSayisi));
            }
            catch (ArgumentException)
            {
                // Yük bozuk: CRC geçmiş olsa bile savunmacı davran
                return false;
            }
            catch (InvalidOperationException)
            {
                return false;
            }

            return true;
        }

        // =================================================================
        // CRC32 (IEEE 802.3) — bağımlılıksız, tablo tabanlı
        // =================================================================
        private static readonly uint[] s_crcTablosu = CrcTablosuOlustur();

        private static uint[] CrcTablosuOlustur()
        {
            uint[] tablo = new uint[256];
            for (uint i = 0; i < 256; i++)
            {
                uint c = i;
                for (int k = 0; k < 8; k++)
                {
                    c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                }
                tablo[i] = c;
            }
            return tablo;
        }

        /// <summary>CRC-32 (IEEE). Bozulma tespiti içindir, güvenlik amaçlı değildir.</summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static uint Crc32(scoped ReadOnlySpan<byte> veri)
        {
            uint crc = 0xFFFFFFFFu;
            uint[] tablo = s_crcTablosu;
            for (int i = 0; i < veri.Length; i++)
            {
                crc = tablo[(crc ^ veri[i]) & 0xFF] ^ (crc >> 8);
            }
            return crc ^ 0xFFFFFFFFu;
        }
    }
}
