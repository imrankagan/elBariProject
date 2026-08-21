#nullable enable

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Intrinsics;
using System.Runtime.Intrinsics.Arm;  // ARM NEON desteği
using System.Runtime.Intrinsics.X86;  // Intel/AMD AVX2 desteği

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ: PROFESYONEL SIKIŞTIRMA MOTORU
    // =================================================================
    // 
    // Telif Hakkı (c) 2025 İmran Kağan. Tüm Hakları Saklıdır.
    // 
    // ⚠️ TİCARİ YAZILIM - LİSANS GEREKLİDİR
    // 
    // Bu kapalı kaynak kodlu ticari bir yazılımdır.
    // İzinsiz kullanım, kopyalama veya değiştirme yasaktır.
    // 
    // Lisans için iletişim:
    // Website: https://github.com/imrankagan/elBariProject
    // Fiyatlandırma: Yılda $2,000'dan başlayan fiyatlar
    // 
    // DERLEME: Native AOT (PublishAot=true)
    // - JIT ısınma süresi gerektirmez
    // - Yerel makine kodu performansı
    // - Deterministik çalışma süresi
    // - ARM/x64 çapraz derleme desteği
    // 
    // PATENT VE FİKRİ MÜLKİYET NOTU:
    // Bu implementasyon, halka açık ve patentsiz algoritmik tekniklerin
    // (delta encoding, bit packing, variable bit-width) özgün bir
    // kombinasyonunu kullanır. Bilinen hiçbir patent ihlali içermez.
    // 
    // SORUMLULUK REDDİ:
    // Bu yazılım "OLDUĞU GİBİ" sağlanmaktadır, açık veya zımni HİÇBİR
    // GARANTİ verilmez. Kritik sistemlerde kullanmadan önce kapsamlı
    // testler yapılması tavsiye edilir.
    // =================================================================
    public static class ElBâri
    {
        public const int BLOK_BOYUTU = 8;

        // -----------------------------------------------------------------
        // BLOK-ÜSTÜ SIFIR KOŞUSU  (biçim sürümü 3)
        // -----------------------------------------------------------------
        // Sıfır blok (mod 0) veri biti yazmaz ama ETİKETİNİ yine yazar. Bu,
        // biçime değer başına 0,5 bitlik bir taban ve 32 bitlik değerler için
        // 64x'lik sert bir TAVAN koyar — veri ne kadar sabit olursa olsun.
        //
        // Ölçüldü: gerçek bir kumanda girişi (RCIN) kanalında blokların
        // %94,5'i sıfır blok ve bunların %93'ü uzun koşular hâlinde.
        //
        // KAÇIŞ KODU: etiket "mod 0 + aykırı_var 1" ardından 8 bitlik aykırı
        // maskesi 0x00, sürüm 2 kodlayıcısında ULAŞILAMAZ bir birleşimdir
        // (aykırı_var ancak maske sıfır DEĞİLKEN kurulur). Bu yüzden sürüm 2
        // akışları sürüm 3 çözücüsünde aynen çalışır.
        //
        // MALİYET: kaçış 4 + 8 + 16 = 28 bit; düz kodlama koşu başına 4 bit.
        //          Başabaş nokta 7 bloktur, bu yüzden eşik 8'dir.
        //
        // C sürümüyle BİREBİR aynı kural: c/src/elbari.c
        private const int SIFIR_KOSU_ESIGI = 8;
        private const int SIFIR_KOSU_MAKS = 65535;
        private const int SIFIR_KOSU_BITI = 16;

        // Sihirli Sayı Sabitleri (Okunabilirlik ve Bakım İçin)
        private const int AYKIRI_ESIK = 32767;
        private const int MAKS_BIT_GENISLIGI = 16;
        private const int MIN_BIT_GENISLIGI = 1;

        // BIT GENİŞLİĞİ TABLOSU (biçim sürümü 2)
        //
        // Etiketteki 'mod' alanı 3 bittir, yani 8 değer tutabilir. Sürüm 1'de
        // bunun yalnızca 4'ü kullanılıyordu (2/4/8/16); kalan 4 yuva boştaydı.
        // Bu, 5 bit gereken bir farkın 8 bitle, 10 bit gerekenin 16 bitle
        // yazılması demekti.
        //
        // Boş yuvalara ara genişlikler eklendi. Etiket alanı BÜYÜMEDİ —
        // yalnızca zaten var olan bitler değerlendirildi.
        //
        // Küme, gerçek veri üzerinde kaba kuvvet aramasıyla seçildi: 16 zorunlu
        // (aykırı eşiği 32767), kalan 7 yuva için 1..15 arasındaki tüm
        // kombinasyonlar denendi ve iki veri setinde birden en iyi sonucu veren
        // küme alındı. Ölçülen kazanç: gerçek GPS %25.6, İHA telemetrisi %24.0.
        // mod = 0 -> SIFIR BLOK: aykırı olmayan tüm farklar sıfırdır ve
        // HİÇBİR veri biti yazılmaz. Bu bir en iyileme değil, bilgi taşımayan
        // bitlerin kaldırılmasıdır: mod 0 yalnızca M <= 0 iken seçilir, yani
        // aykırı olmayan farkların hepsi tam olarak sıfırdır. Eskiden her biri
        // için garantili sıfır olan 1 bit yazılıyordu — sıfır bilgi, sekiz bit.
        // Ölçülen kazanç: gerçek GPS %5.4, İHA telemetrisi %3.5.
        private const int BIT_GENISLIGI_0 = 0;    // M <= 0  (veri biti yok)
        private const int BIT_GENISLIGI_1 = 2;    // M <= 1
        private const int BIT_GENISLIGI_2 = 3;    // M <= 3
        private const int BIT_GENISLIGI_3 = 4;    // M <= 7
        private const int BIT_GENISLIGI_4 = 5;    // M <= 15
        private const int BIT_GENISLIGI_5 = 8;    // M <= 127
        private const int BIT_GENISLIGI_6 = 10;   // M <= 511
        private const int BIT_GENISLIGI_7 = 16;   // M <= 32767
        private const int AYKIRI_BIT_GENISLIGI = 32;
        private const long BAYT_MASKESI = 0xFF;
        private const int ETIKET_MASKESI = 0x0F;
        private const int REFERANS_BOYUTU = 4;
        private const long UINT32_MASKESI = 0xFFFFFFFF; // 32-bit unsigned int maskesi

        // Erken-İptal ve HızlıTarama Eşikleri
        // ERKEN_IPTAL_ESIGI: Sıkıştırma oranı bu değerin altındaysa işlem iptal edilir
        // Örnek: 1.5x = Ham veri 1.5 kat veya daha az küçülüyorsa, sıkıştırma değmez
        private const float ERKEN_IPTAL_ESIGI = 1.5f;

        // MAKS_AYKIRI_ORANI: Veri içindeki aykırı değer oranı bu eşiği aşarsa
        // veri "sıkıştırılamaz" olarak işaretlenir (gerçek dünya verisi değil, rastgele veri)
        // Örnek: %30 = Her 10 elemandan 3'ü aykırı ise, veri uygun değil
        private const float MAKS_AYKIRI_ORANI = 0.30f;

        private const int HIZLI_TARAMA_ORNEKLEM_BOYUTU = 1000;

        // ARTIK_TOLERANSI: Çözücünün tüketmeden bırakabileceği en fazla bayt sayısı.
        // Geçerli bir akışta bu değer 0'dır; kodlayıcı ne yazdıysa çözücü onu okur.
        // Küçük bir tolerans, ileride biçime hizalama/dolgu eklenirse kırılma
        // olmaması için bırakılmıştır.
        private const int ARTIK_TOLERANSI = 0;

        // NOT: EMBEDDED_MODE için compile-time switch kullanılıyor
        // Aktivasyon: Project dosyasında <DefineConstants>EMBEDDED_MODE</DefineConstants>
        // veya build komutunda: dotnet build -p:DefineConstants=EMBEDDED_MODE
        // 
        // EMBEDDED_MODE Özellikleri:
        // - Exception'lar devre dışı (sessiz hata)
        // - Buffer overflow durumunda erken çıkış (crash yerine)
        // - Real-time constraint'lere uyumlu
        // - İHA, askeri sistem, kritik embedded uygulamalar için
        // 
        // Varsayılan: Normal mod (exception'lar aktif, debug kolay)

        // =================================================================
        // YARDIMCI METOTLAR - HOT PATH OPTİMİZASYONU
        // =================================================================

        /// <summary>
        /// Bir referansın belirtilen bayt sınırına hizalı olup olmadığını kontrol eder.
        /// ARM NEON 16-bayt hizalamayı, AVX2 ise 32-bayt hizalamayı tercih eder.
        /// IL seviyesi işaretçi dönüşümü kullanır (Unsafe sınıfı ile unsafe anahtar kelimesi gerekmez).
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool HizaliMi<T>(ref T referans, int hizalama) where T : struct
        {
            // Unsafe.AsPointer kullan - IL güvenli ve unsafe bağlamı gerektirmez
            unsafe
            {
                nuint adres = (nuint)Unsafe.AsPointer(ref referans);
                return (adres & (uint)(hizalama - 1)) == 0;
            }
        }

        /// <summary>
        /// Bit manipülasyonu kullanarak taşma-güvenli mutlak değer.
        /// int.MinValue değerini doğru şekilde işler (OverflowException atmaz).
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static int MutlakDeger(int deger)
        {
            // Bit manipülasyon hilesi: (deger XOR işaret-biti) - işaret-biti
            // Pozitif için: (deger XOR 0) - 0 = deger
            // Negatif için: (deger XOR -1) - (-1) = ~deger + 1 = -deger
            // int.MinValue için: doğru şekilde int.MaxValue + 1 döndürür (sarar, ama güvenli)
            int maske = deger >> 31;
            return (deger ^ maske) - maske;
        }

        /// <summary>
        /// HızlıTarama: Veriyi hızlıca tarayarak sıkıştırılabilir olup olmadığını kontrol eder.
        /// Gerçek dünya verisi (sensör, GPS, telemetri) vs anlamsız veri (tamamen rastgele) ayrımı.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool SikistirilabirVeriMi(scoped ReadOnlySpan<int> hamVeri)
        {
            int uzunluk = hamVeri.Length;
            if (uzunluk < 2) return true; // Çok küçük veri, kabul et

            // İlk %10'unu veya max HIZLI_TARAMA_ORNEKLEM_BOYUTU elemanı tara (hızlı örnekleme)
            int orneklemBoyutu = Math.Min(HIZLI_TARAMA_ORNEKLEM_BOYUTU, uzunluk / 10);
            if (orneklemBoyutu < 10) orneklemBoyutu = Math.Min(uzunluk, 100);

            // Delta'ları hesapla ve istatistik topla
            long deltaMutlakToplam = 0;
            int aykiriSayisi = 0;
            int maksDelta = 0;

            for (int i = 1; i < orneklemBoyutu; i++)
            {
                int delta = hamVeri[i] - hamVeri[i - 1];
                int mutlakDelta = MutlakDeger(delta);

                deltaMutlakToplam += mutlakDelta;

                if (mutlakDelta > AYKIRI_ESIK)
                {
                    aykiriSayisi++;
                }

                if (mutlakDelta > maksDelta)
                {
                    maksDelta = mutlakDelta;
                }
            }

            // Kriter 1: Aykırı oran çok yüksekse (>%30) → kötü veri
            float aykiriOrani = (float)aykiriSayisi / (orneklemBoyutu - 1);
            if (aykiriOrani > MAKS_AYKIRI_ORANI)
            {
                return false; // MAKS_AYKIRI_ORANI+ sıçrama → sıkıştırılamaz
            }

            // Kriter 2: Ortalama delta çok büyükse → kötü veri
            long ortalamaDelta = deltaMutlakToplam / Math.Max(1, orneklemBoyutu - 1);
            if (ortalamaDelta > int.MaxValue / 4)
            {
                return false; // Ortalama delta çok büyük → tamamen rastgele
            }

            // Kriter 3: Maks delta tam-aralık kullanıyorsa → şüpheli
            if (maksDelta > int.MaxValue / 2 && aykiriOrani > MAKS_AYKIRI_ORANI / 3)
            {
                return false; // Hem büyük delta hem aykırı → kötü kombinasyon
            }

            // Geçti - sıkıştırılabilir veri
            return true;
        }

        /// <summary>
        /// Delta değerlerini işleyerek aykırı maske ve maksMutlak hesaplar - Genel yardımcı
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void DeltalariIsle(scoped ReadOnlySpan<int> deltalar, ref int maksMutlak, ref byte aykiriMaske, int kayma = 0)
        {
            for (int j = 0; j < deltalar.Length; j++)
            {
                // Büyüklük 64 BİTTE ölçülür. 32 bitlik mutlak değer
                // int.MinValue için negatif kalır (sarar); o durumda
                // aşağıdaki iki karşılaştırma da sessizce başarısız olur:
                // fark ne aykırı işaretlenir ne de blok bit genişliğini
                // yükseltir, sonra dar maskeyle paketlenip ÜST BİTİNİ
                // KAYBEDER. Tam olarak 2^31'lik bir fark — örn. işareti
                // değişen ama büyüklüğü aynı kalan bir float bit deseni
                // (+0.001f -> -0.001f) — kayıpsızlığı bu yolla bozuyordu.
                // C sürümündeki elbari.c ile birebir aynı düzeltme.
                long m = Math.Abs((long)deltalar[j]);
                if (m > AYKIRI_ESIK)
                {
                    aykiriMaske |= (byte)(1 << (j + kayma));
                }
                else if (m > maksMutlak)
                {
                    // Bu dalda m <= AYKIRI_ESIK; daralma güvenli.
                    maksMutlak = (int)m;
                }
            }
        }

        /// <summary>
        /// Bit tamponundan bayt boşaltma işlemi - Agresif Satıriçi
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void BitTamponuBosalt(ref long bitTamponu, ref int bitSayisi, scoped Span<byte> cikti, ref int baytIndeksi)
        {
            while (bitSayisi >= 8)
            {
                if (baytIndeksi >= cikti.Length)
                {
#if EMBEDDED_MODE
                    // Gömülü sistem: Sessiz hata, veri kaybı yerine kesme
                    return;
#else
                    throw new InvalidOperationException(
                        $"Çıktı tamponu taştı. İndeks: {baytIndeksi}, Boyut: {cikti.Length}");
#endif
                }

                cikti[baytIndeksi++] = (byte)(bitTamponu & BAYT_MASKESI);
                bitTamponu >>= 8;
                bitSayisi -= 8;
            }
        }

        /// <summary>
        /// Bit tamponuna veri yükleme - Agresif Satıriçi
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void BitTamponuYukle(ref long bitTamponu, ref int bitSayisi, scoped ReadOnlySpan<byte> girdi, ref int baytIndeksi, int gerekliBitler)
        {
            while (bitSayisi < gerekliBitler)
            {
                if (baytIndeksi >= girdi.Length)
                {
#if EMBEDDED_MODE
                    // Gömülü sistem: Sessiz hata
                    return;
#else
                    throw new InvalidOperationException(
                        $"Girdi tamponu sonuna ulaşıldı. İndeks: {baytIndeksi}, Boyut: {girdi.Length}");
#endif
                }
                bitTamponu |= ((long)girdi[baytIndeksi++] << bitSayisi);
                bitSayisi += 8;
            }
        }

        /// <summary>
        /// Önek toplam (prefix sum) ile delta'ları geri ekleyerek orijinal veriyi yeniden inşa eder.
        /// Manuel açılmış döngü ile SIMD performansını korur.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void OnEkToplamYenidenInsaEt(ref int ciktiRef, int ciktiIndeksi, scoped Span<int> gecici)
        {
            int onceki = Unsafe.Add(ref ciktiRef, ciktiIndeksi - 1);

            // Manuel açılmış döngü - 8 elemanlı blok için optimize
            int deger0 = onceki + gecici[0];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi) = deger0;

            int deger1 = deger0 + gecici[1];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 1) = deger1;

            int deger2 = deger1 + gecici[2];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 2) = deger2;

            int deger3 = deger2 + gecici[3];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 3) = deger3;

            int deger4 = deger3 + gecici[4];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 4) = deger4;

            int deger5 = deger4 + gecici[5];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 5) = deger5;

            int deger6 = deger5 + gecici[6];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 6) = deger6;

            int deger7 = deger6 + gecici[7];
            Unsafe.Add(ref ciktiRef, ciktiIndeksi + 7) = deger7;
        }

        /// <summary>
        /// hamVeri[i .. i+7] aralığındaki TÜM farklar sıfır mı?
        /// Çağıran, i+7'nin sınırlar içinde olduğunu garanti etmelidir.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool SifirBlokMu(scoped ReadOnlySpan<int> hamVeri, int i)
        {
            for (int j = 0; j < BLOK_BOYUTU; j++)
            {
                if (hamVeri[i + j] != hamVeri[i + j - 1]) { return false; }
            }
            return true;
        }

        // =================================================================
        // ELKÂBID (KODLAYICI) – %100 YİĞİNSİZ & AYKIRI HARİTALI
        // PERFORMANS: Agresif Satıriçi + Sıcak Yol Optimizasyonu
        // =================================================================
        [MethodImpl(MethodImplOptions.AggressiveInlining | MethodImplOptions.AggressiveOptimization)]
        public static int ElKâbıd(scoped ReadOnlySpan<int> hamVeri, scoped Span<byte> cikti)
        {
            if (hamVeri.IsEmpty) return 0;

            // HIZLI TARAMA: Veri sıkıştırılabilir mi? (Gerçek dünya verisi mi?)
            // Anlamsız/rastgele veriyi erken reddet - CPU zamanı koru
            if (!SikistirilabirVeriMi(hamVeri))
            {
                // Veri sıkıştırılabilir değil - red
                return -1; // Negatif dönüş = "Bu veri bizim için değil"
            }

            // GÜVENLİK KONTROLÜ: Çıktı tamponu yeterli mi?
            int minCiktiBoyu = REFERANS_BOYUTU + (hamVeri.Length * sizeof(int)); // En kötü durum tahmini
            if (cikti.Length < minCiktiBoyu)
            {
                throw new ArgumentException(
                    $"Çıktı tamponu çok küçük. Minimum {minCiktiBoyu} bayt gerekli, {cikti.Length} bayt verildi.", 
                    nameof(cikti));
            }

            int referans = hamVeri[0];
            MemoryMarshal.Write(cikti, in referans);

            int baytIndeksi = REFERANS_BOYUTU;
            long bitTamponu = 0;
            int bitSayisi = 0;
            int veriIndeksi = 1;
            bool erkenIptalKontrolEdildi = false; // Bayrak: erken-iptal kontrolü yapıldı mı?

            // Stackalloc'ları döngü dışına taşı (CA2014 uyarısı için)
            Span<int> geciciTampon = stackalloc int[BLOK_BOYUTU];

            while (veriIndeksi < hamVeri.Length)
            {
                int kalan = hamVeri.Length - veriIndeksi;
                int blokBoyu = kalan < BLOK_BOYUTU ? kalan : BLOK_BOYUTU;

                // 0) BLOK-ÜSTÜ SIFIR KOŞUSU (biçim sürümü 3).
                //    İleriye bakılır; yeterince uzun bir tam-sıfır blok dizisi
                //    varsa hepsi TEK bir kaçışla yazılır. Kısa koşularda kaçış
                //    zarar edeceğinden eşik altında düz kodlama sürer.
                if (blokBoyu == BLOK_BOYUTU)
                {
                    int kosu = 0;
                    int ileri = veriIndeksi;

                    while (hamVeri.Length - ileri >= BLOK_BOYUTU &&
                           kosu < SIFIR_KOSU_MAKS &&
                           SifirBlokMu(hamVeri, ileri))
                    {
                        kosu++;
                        ileri += BLOK_BOYUTU;
                    }

                    if (kosu >= SIFIR_KOSU_ESIGI)
                    {
                        // Etiket: mod 0, aykırı_var 1 -> ham değer 1
                        bitTamponu |= 1L << bitSayisi;
                        bitSayisi += 4;
                        BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);

                        // Aykırı maskesi 0x00: kaçış işareti. Tamponun bu
                        // bitleri zaten sıfırdır, yalnızca sayaç ilerletilir.
                        bitSayisi += 8;
                        BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);

                        // Koşu uzunluğu (16 bit)
                        bitTamponu |= (long)(uint)kosu << bitSayisi;
                        bitSayisi += SIFIR_KOSU_BITI;
                        BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);

                        veriIndeksi = ileri;
                        continue;
                    }
                }

                int maksMutlak = 0;
                byte aykiriMaske = 0;

                // ÇOK MİMARİLİ SIMD OPTİMİZASYONU
                // Intel/AMD için AVX2, ARM için NEON, yoksa skaler geri dönüş

                // INTEL/AMD: AVX2 ile 8x32-bit paralel işlem
                if (Avx2.IsSupported && blokBoyu == BLOK_BOYUTU)
                {
                    ref int temelRef = ref MemoryMarshal.GetReference(hamVeri);
                    ref int guncelRef = ref Unsafe.Add(ref temelRef, veriIndeksi);
                    ref int oncekiRef = ref Unsafe.Add(ref temelRef, veriIndeksi - 1);

                    // AVX2 SIMD: LoadUnsafe hizalanmamış erişimi güvenli şekilde işler (potansiyel performans maliyeti ile)
                    // Garantili hizalama için, hizalanmış tamponlar kullanmayı düşünün
                    Vector256<int> guncel = Vector256.LoadUnsafe(ref guncelRef);
                    Vector256<int> onceki = Vector256.LoadUnsafe(ref oncekiRef);

                    Vector256<int> deltalar = Avx2.Subtract(guncel, onceki);
                    Vector256<int> mutlakDelta = Avx2.Abs(deltalar).AsInt32();

                    mutlakDelta.CopyTo(geciciTampon);
                    DeltalariIsle(geciciTampon.Slice(0, BLOK_BOYUTU), ref maksMutlak, ref aykiriMaske);
                }
                // ARM: NEON ile 4x32-bit paralel işlem (İHA/Gömülü Sistemler)
                else if (AdvSimd.IsSupported && blokBoyu >= 4)
                {
                    ref int temelRef = ref MemoryMarshal.GetReference(hamVeri);

                    // ARM NEON: Optimal performans için 16-bayt hizalama kontrolü
                    ref int guncelRef1 = ref Unsafe.Add(ref temelRef, veriIndeksi);
                    bool hizaliMi = HizaliMi(ref guncelRef1, 16);

                    // Hizalı yol: SIMD kullan (ARM'da daha hızlı)
                    if (hizaliMi)
                    {
                        // İlk 4 eleman için NEON - Hizalı yükleme
                        ref int oncekiRef1 = ref Unsafe.Add(ref temelRef, veriIndeksi - 1);

                        Vector128<int> guncel1 = Vector128.LoadUnsafe(ref guncelRef1);
                        Vector128<int> onceki1 = Vector128.LoadUnsafe(ref oncekiRef1);

                        Vector128<int> deltalar1 = AdvSimd.Subtract(guncel1, onceki1);
                        Vector128<int> mutlakDelta1 = AdvSimd.Abs(deltalar1).AsInt32();

                        mutlakDelta1.CopyTo(geciciTampon.Slice(0, 4));
                        DeltalariIsle(geciciTampon.Slice(0, 4), ref maksMutlak, ref aykiriMaske);

                        // Son 4 eleman için (eğer blokBoyu == 8 ise)
                        if (blokBoyu == BLOK_BOYUTU)
                        {
                            ref int guncelRef2 = ref Unsafe.Add(ref temelRef, veriIndeksi + 4);
                            ref int oncekiRef2 = ref Unsafe.Add(ref temelRef, veriIndeksi + 3);

                            Vector128<int> guncel2 = Vector128.LoadUnsafe(ref guncelRef2);
                            Vector128<int> onceki2 = Vector128.LoadUnsafe(ref oncekiRef2);

                            Vector128<int> deltalar2 = AdvSimd.Subtract(guncel2, onceki2);
                            Vector128<int> mutlakDelta2 = AdvSimd.Abs(deltalar2).AsInt32();

                            mutlakDelta2.CopyTo(geciciTampon.Slice(4, 4));
                            DeltalariIsle(geciciTampon.Slice(4, 4), ref maksMutlak, ref aykiriMaske, kayma: 4);
                        }
                    }
                    else
                    {
                        // Hizalanmamış yol: Skaler işleme geri dön (ARM'da daha güvenli)
                        Span<int> skalerDeltalar = geciciTampon.Slice(0, blokBoyu);
                        for (int j = 0; j < blokBoyu; j++)
                        {
                            skalerDeltalar[j] = hamVeri[veriIndeksi + j] - hamVeri[veriIndeksi + j - 1];
                        }
                        DeltalariIsle(skalerDeltalar, ref maksMutlak, ref aykiriMaske);
                    }
                }
                // GERİ DÖNÜŞ: Skaler işlem (Eski işlemciler, SIMD desteği yok)
                else
                {
                    Span<int> skalerDeltalar = geciciTampon.Slice(0, blokBoyu);
                    for (int j = 0; j < blokBoyu; j++)
                    {
                        skalerDeltalar[j] = hamVeri[veriIndeksi + j] - hamVeri[veriIndeksi + j - 1];
                    }
                    DeltalariIsle(skalerDeltalar, ref maksMutlak, ref aykiriMaske);
                }

                bool aykiriVar = aykiriMaske != 0;
                int bitGenisligi;

                int mod;
                if (maksMutlak <= 0)        { bitGenisligi = BIT_GENISLIGI_0; mod = 0; }
                else if (maksMutlak <= 1)   { bitGenisligi = BIT_GENISLIGI_1; mod = 1; }
                else if (maksMutlak <= 3)   { bitGenisligi = BIT_GENISLIGI_2; mod = 2; }
                else if (maksMutlak <= 7)   { bitGenisligi = BIT_GENISLIGI_3; mod = 3; }
                else if (maksMutlak <= 15)  { bitGenisligi = BIT_GENISLIGI_4; mod = 4; }
                else if (maksMutlak <= 127) { bitGenisligi = BIT_GENISLIGI_5; mod = 5; }
                else if (maksMutlak <= 511) { bitGenisligi = BIT_GENISLIGI_6; mod = 6; }
                else                        { bitGenisligi = BIT_GENISLIGI_7; mod = 7; }

                int etiket = (mod << 1) | (aykiriVar ? 1 : 0);
                bitTamponu |= ((long)etiket << bitSayisi);
                bitSayisi += 4;

                BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);

                if (aykiriVar)
                {
                    bitTamponu |= ((long)aykiriMaske << bitSayisi);
                    bitSayisi += 8;

                    BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);
                }

                // SIFIR BLOK: mod 0 ise aykırı olmayan farkların hepsi sıfırdır;
                // veri biti yazılmaz (bkz. BIT_GENISLIGI_0 açıklaması).
                if (bitGenisligi > 0)
                {
                    long maske = (1L << bitGenisligi) - 1;

                    for (int j = 0; j < blokBoyu; j++)
                    {
                        if (aykiriVar && (aykiriMaske & (1 << j)) != 0)
                        {
                            continue;
                        }

                        int delta = hamVeri[veriIndeksi + j] - hamVeri[veriIndeksi + j - 1];
                        long d = delta & maske;

                        bitTamponu |= (d << bitSayisi);
                        bitSayisi += bitGenisligi;

                        BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);
                    }
                }

                if (aykiriVar)
                {
                    for (int j = 0; j < blokBoyu; j++)
                    {
                        if ((aykiriMaske & (1 << j)) != 0)
                        {
                            int delta = hamVeri[veriIndeksi + j] - hamVeri[veriIndeksi + j - 1];
                            bitTamponu |= ((long)(uint)delta << bitSayisi);
                            bitSayisi += AYKIRI_BIT_GENISLIGI;

                            BitTamponuBosalt(ref bitTamponu, ref bitSayisi, cikti, ref baytIndeksi);
                        }
                    }
                }

                veriIndeksi += blokBoyu;

                // ERKEN-İPTAL: İlk blok bittikten sonra SADECE BİR KERE kontrol et
                // Eğer sıkıştırma kazancı yok ise (oran < 1.5x), iptal et
                if (!erkenIptalKontrolEdildi && veriIndeksi >= Math.Min(64, hamVeri.Length))
                {
                    erkenIptalKontrolEdildi = true; // Bir kere kontrol et

                    // İlk 64 eleman (veya tüm veri) işlendi, sıkıştırma oranı kontrol et
                    int islenenBaytlar = veriIndeksi * sizeof(int);
                    int sikistirilmisBaytlar = baytIndeksi;
                    float sikistirmaOrani = (float)islenenBaytlar / sikistirilmisBaytlar;

                    // Eşik: ERKEN_IPTAL_ESIGI (yani yeterli kazanç yoksa iptal)
                    if (sikistirmaOrani < ERKEN_IPTAL_ESIGI && veriIndeksi < hamVeri.Length)
                    {
                        // Sıkıştırma kazancı yok - iptal et
                        return -1; // Negatif dönüş = "Sıkıştırma başarısız"
                    }
                    // Eğer oran iyiyse veya burası son bloksa devam et
                }
            }

            if (bitSayisi > 0)
            {
                if (baytIndeksi >= cikti.Length)
                {
                    throw new InvalidOperationException(
                        $"Çıktı tamponu taştı (son boşaltma). İndeks: {baytIndeksi}, Boyut: {cikti.Length}");
                }
                cikti[baytIndeksi++] = (byte)(bitTamponu & BAYT_MASKESI);
            }

            return baytIndeksi;
        }

        // =================================================================
        // ELBÂSIT (ÇÖZÜCÜ) – %100 YİĞİNSİZ & YİĞİNAYIRMA KORUMALI
        // PERFORMANS: Agresif Satıriçi + Sıcak Yol Optimizasyonu
        // =================================================================
        [MethodImpl(MethodImplOptions.AggressiveInlining | MethodImplOptions.AggressiveOptimization)]
        public static void ElBâsıt(scoped ReadOnlySpan<byte> girdi, scoped Span<int> cikti)
        {
            // GÜVENLİK KONTROLÜ: Girdi en az referans boyutu içermeli
            if (girdi.Length < REFERANS_BOYUTU)
            {
                throw new ArgumentException(
                    $"Girdi tamponu çok küçük. Minimum {REFERANS_BOYUTU} bayt gerekli, {girdi.Length} bayt verildi.", 
                    nameof(girdi));
            }

            if (cikti.IsEmpty)
            {
                throw new ArgumentException("Çıktı tamponu boş olamaz.", nameof(cikti));
            }

            int referans = MemoryMarshal.Read<int>(girdi.Slice(0, REFERANS_BOYUTU));
            cikti[0] = referans;

            int baytIndeksi = REFERANS_BOYUTU;
            long bitTamponu = 0;
            int bitSayisi = 0;
            int ciktiIndeksi = 1;

            Span<int> gecici = stackalloc int[BLOK_BOYUTU];

            while (ciktiIndeksi < cikti.Length)
            {
                BitTamponuYukle(ref bitTamponu, ref bitSayisi, girdi, ref baytIndeksi, 4);

                int etiket = (int)(bitTamponu & ETIKET_MASKESI);
                bitTamponu >>= 4;
                bitSayisi -= 4;

                int mod = etiket >> 1;
                bool aykiriVar = (etiket & 1) != 0;

                int bitGenisligi = mod switch
                {
                    0 => BIT_GENISLIGI_0,
                    1 => BIT_GENISLIGI_1,
                    2 => BIT_GENISLIGI_2,
                    3 => BIT_GENISLIGI_3,
                    4 => BIT_GENISLIGI_4,
                    5 => BIT_GENISLIGI_5,
                    6 => BIT_GENISLIGI_6,
                    _ => BIT_GENISLIGI_7
                };

                int kalan = cikti.Length - ciktiIndeksi;
                int blokBoyu = kalan < BLOK_BOYUTU ? kalan : BLOK_BOYUTU;
                long maske = bitGenisligi > 0 ? (1L << bitGenisligi) - 1 : 0;

                int aykiriMaske = 0;
                if (aykiriVar)
                {
                    BitTamponuYukle(ref bitTamponu, ref bitSayisi, girdi, ref baytIndeksi, 8);
                    aykiriMaske = (int)(bitTamponu & BAYT_MASKESI);
                    bitTamponu >>= 8;
                    bitSayisi -= 8;

                    // BLOK-ÜSTÜ SIFIR KOŞUSU (biçim sürümü 3).
                    // Maske 0x00, sürüm 2 kodlayıcısının ÜRETEMEYECEĞİ bir
                    // değerdir; kaçış işareti olarak kullanılır. Sürüm 2
                    // akışları bu daldan hiç geçmez ve aynen çözülür.
                    if (mod == 0 && aykiriMaske == 0)
                    {
                        BitTamponuYukle(ref bitTamponu, ref bitSayisi, girdi, ref baytIndeksi, SIFIR_KOSU_BITI);
                        int kosu = (int)(bitTamponu & 0xFFFF);
                        bitTamponu >>= SIFIR_KOSU_BITI;
                        bitSayisi -= SIFIR_KOSU_BITI;

                        // Kodlayıcı sıfır uzunlukta koşu yazmaz.
                        if (kosu <= 0) { return; }

                        for (int r = 0; r < kosu; r++)
                        {
                            for (int j = 0; j < BLOK_BOYUTU; j++)
                            {
                                if (ciktiIndeksi >= cikti.Length) { return; }
                                cikti[ciktiIndeksi] = cikti[ciktiIndeksi - 1];
                                ciktiIndeksi++;
                            }
                        }
                        continue;
                    }
                }

                for (int j = 0; j < blokBoyu; j++)
                {
                    if (aykiriVar && (aykiriMaske & (1 << j)) != 0)
                    {
                        continue;
                    }

                    // SIFIR BLOK: veri biti yazılmamıştı, fark sıfırdır.
                    if (bitGenisligi == 0)
                    {
                        gecici[j] = 0;
                        continue;
                    }

                    BitTamponuYukle(ref bitTamponu, ref bitSayisi, girdi, ref baytIndeksi, bitGenisligi);

                    long d = bitTamponu & maske;
                    bitTamponu >>= bitGenisligi;
                    bitSayisi -= bitGenisligi;

                    int delta = (int)d;
                    if (bitGenisligi < AYKIRI_BIT_GENISLIGI && (delta & (1 << (bitGenisligi - 1))) != 0)
                        delta |= (int)~maske;

                    gecici[j] = delta;
                }

                if (aykiriVar)
                {
                    for (int j = 0; j < blokBoyu; j++)
                    {
                        if ((aykiriMaske & (1 << j)) != 0)
                        {
                            BitTamponuYukle(ref bitTamponu, ref bitSayisi, girdi, ref baytIndeksi, AYKIRI_BIT_GENISLIGI);

                            gecici[j] = (int)(bitTamponu & UINT32_MASKESI);
                            bitTamponu >>= AYKIRI_BIT_GENISLIGI;
                            bitSayisi -= AYKIRI_BIT_GENISLIGI;
                        }
                    }
                }

                // SIMD Optimizasyonu: Delta'ları geri ekleme (yeniden inşa)
                // AVX2 (Intel/AMD) ve NEON (ARM) desteği
                if (Avx2.IsSupported && blokBoyu == BLOK_BOYUTU)
                {
                    // AVX2: Önek toplam (birikimli toplam) ile yeniden inşa
                    ref int ciktiRef = ref MemoryMarshal.GetReference(cikti);
                    OnEkToplamYenidenInsaEt(ref ciktiRef, ciktiIndeksi, gecici);
                    ciktiIndeksi += BLOK_BOYUTU;
                }
                else if (AdvSimd.IsSupported && blokBoyu == BLOK_BOYUTU)
                {
                    // ARM NEON: Önek toplam yeniden inşa
                    ref int ciktiRef = ref MemoryMarshal.GetReference(cikti);
                    OnEkToplamYenidenInsaEt(ref ciktiRef, ciktiIndeksi, gecici);
                    ciktiIndeksi += BLOK_BOYUTU;
                }
                else
                {
                    // Geri Dönüş: Standart döngü (eski işlemciler)
                    for (int j = 0; j < blokBoyu; j++)
                    {
                        cikti[ciktiIndeksi] = cikti[ciktiIndeksi - 1] + gecici[j];
                        ciktiIndeksi++;
                    }
                }
            }

            // YAPISAL DOĞRULAMA — tüketim kontrolü
            //
            // Geçerli bir sıkıştırılmış akış girdinin TAMAMINI tüketir: kodlayıcı
            // tam olarak gerektiği kadar bayt yazar, çözücü de tam olarak o kadarını
            // okur. Geriye artık kalmışsa girdi bu kodlayıcıdan çıkmamış demektir.
            //
            // Bu, sağlama toplamının yerini TUTMAZ; ancak maliyeti tek bir
            // karşılaştırmadır ve rastgele/bozuk verinin büyük kısmını eler.
            // Bütünlük garantisi için çerçeve katmanı (CRC32) kullanılmalıdır.
            //
            // NOT: girdi, sıkıştırılmış verinin TAM boyutunda olmalıdır. Daha büyük
            // bir dilim verilirse bu kontrol devreye girer.
            if ((girdi.Length - baytIndeksi) > ARTIK_TOLERANSI)
            {
#if EMBEDDED_MODE
                return;
#else
                throw new ArgumentException(
                    $"Girdi bu kodlayıcıdan çıkmamış görünüyor: {girdi.Length - baytIndeksi} bayt " +
                    "tüketilmeden kaldı. Güvenilmeyen veri için ElBâriÇerçeve katmanını kullanın.",
                    nameof(girdi));
#endif
            }
        }
    }
}
