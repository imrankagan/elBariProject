using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Intrinsics;
using System.Runtime.Intrinsics.Arm;  // ARM NEON desteği
using System.Runtime.Intrinsics.X86;  // Intel AVX2 desteği

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ: EVRENSEL ADAPTİF SIKIŞTIRMA PROTOKOLÜ
    // =================================================================
    // 
    // Copyright (c) 2025 ElBâri Project
    // License: MIT (Ticari kullanım için ücretsiz)
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

        // Magic Number Constants (Okunabilirlik ve Bakım İçin)
        private const int OUTLIER_ESIK = 32767;
        private const int MAX_BIT_WIDTH = 16;
        private const int MIN_BIT_WIDTH = 2;
        private const int OUTLIER_BIT_WIDTH = 32;
        private const long BYTE_MASK = 0xFF;
        private const int TAG_MASK = 0x0F;
        private const int REFERENCE_SIZE = 4;

        // NOT: EMBEDDED_MODE için compile-time switch kullanılıyor
        // #define EMBEDDED_MODE → Gömülü sistem modu (exception-free)
        // Varsayılan: Normal mod (exception'lar aktif)

        // =================================================================
        // YARDIMCI METOTLAR - HOT PATH OPTİMİZASYONU
        // =================================================================

        /// <summary>
        /// Bit buffer'dan byte flush işlemi - Aggressive Inline
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void FlushBitBuffer(ref long bitBuffer, ref int bitCount, Span<byte> output, ref int byteIndex)
        {
            while (bitCount >= 8)
            {
                if (byteIndex >= output.Length)
                {
#if EMBEDDED_MODE
                    // Gömülü sistem: Silent fail, veri kaybı yerine kesme
                    return;
#else
                    throw new InvalidOperationException(
                        $"Output buffer taştı. İndeks: {byteIndex}, Boyut: {output.Length}");
#endif
                }

                output[byteIndex++] = (byte)(bitBuffer & BYTE_MASK);
                bitBuffer >>= 8;
                bitCount -= 8;
            }
        }

        /// <summary>
        /// Bit buffer'a veri yükleme - Aggressive Inline
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void LoadBitBuffer(ref long bitBuffer, ref int bitCount, ReadOnlySpan<byte> input, ref int byteIndex, int requiredBits)
        {
            while (bitCount < requiredBits)
            {
                if (byteIndex >= input.Length)
                {
#if EMBEDDED_MODE
                    // Gömülü sistem: Silent fail
                    return;
#else
                    throw new InvalidOperationException(
                        $"Input buffer sonuna ulaşıldı. İndeks: {byteIndex}, Boyut: {input.Length}");
#endif
                }
                bitBuffer |= ((long)input[byteIndex++] << bitCount);
                bitCount += 8;
            }
        }

        // =================================================================
        // ELKÂBID (ENCODER) – %100 HEAPSİZ & OUTLIER HARİTALI
        // PERFORMANS: Aggressive Inlining + Hot Path Optimizasyonu
        // =================================================================
        [MethodImpl(MethodImplOptions.AggressiveInlining | MethodImplOptions.AggressiveOptimization)]
        public static int ElKâbıd(ReadOnlySpan<int> rawData, Span<byte> output)
        {
            if (rawData.IsEmpty) return 0;

            // GÜVENLİK KONTROLÜ: Output buffer yeterli mi?
            int minOutputSize = REFERENCE_SIZE + (rawData.Length * sizeof(int)); // Worst-case tahmin
            if (output.Length < minOutputSize)
            {
                throw new ArgumentException(
                    $"Output buffer çok küçük. Minimum {minOutputSize} byte gerekli, {output.Length} byte verildi.", 
                    nameof(output));
            }

            int reference = rawData[0];
            MemoryMarshal.Write(output, in reference);

            int byteIndex = REFERENCE_SIZE;
            long bitBuffer = 0;
            int bitCount = 0;
            int dataIndex = 1;

            // Stackalloc'ları döngü dışına taşı (CA2014 uyarısı için)
            Span<int> tempBuffer = stackalloc int[BLOK_BOYUTU];

            while (dataIndex < rawData.Length)
            {
                int kalan = rawData.Length - dataIndex;
                int blokSize = kalan < BLOK_BOYUTU ? kalan : BLOK_BOYUTU;

                int maxAbs = 0;
                byte outlierMask = 0;

                // ÇOK MİMARİLİ SIMD OPTİMİZASYONU
                // Intel/AMD için AVX2, ARM için NEON, yoksa scalar fallback

                // INTEL/AMD: AVX2 ile 8x32-bit paralel işlem
                if (Avx2.IsSupported && blokSize == BLOK_BOYUTU)
                {
                    ref int baseRef = ref MemoryMarshal.GetReference(rawData);
                    Vector256<int> current = Vector256.Create(
                        Unsafe.Add(ref baseRef, dataIndex),
                        Unsafe.Add(ref baseRef, dataIndex + 1),
                        Unsafe.Add(ref baseRef, dataIndex + 2),
                        Unsafe.Add(ref baseRef, dataIndex + 3),
                        Unsafe.Add(ref baseRef, dataIndex + 4),
                        Unsafe.Add(ref baseRef, dataIndex + 5),
                        Unsafe.Add(ref baseRef, dataIndex + 6),
                        Unsafe.Add(ref baseRef, dataIndex + 7)
                    );

                    Vector256<int> previous = Vector256.Create(
                        Unsafe.Add(ref baseRef, dataIndex - 1),
                        Unsafe.Add(ref baseRef, dataIndex),
                        Unsafe.Add(ref baseRef, dataIndex + 1),
                        Unsafe.Add(ref baseRef, dataIndex + 2),
                        Unsafe.Add(ref baseRef, dataIndex + 3),
                        Unsafe.Add(ref baseRef, dataIndex + 4),
                        Unsafe.Add(ref baseRef, dataIndex + 5),
                        Unsafe.Add(ref baseRef, dataIndex + 6)
                    );

                    Vector256<int> deltas = Avx2.Subtract(current, previous);
                    Vector256<int> absDelta = Avx2.Abs(deltas).AsInt32();

                    absDelta.CopyTo(tempBuffer);

                    for (int j = 0; j < BLOK_BOYUTU; j++)
                    {
                        int a = tempBuffer[j];
                        if (a > OUTLIER_ESIK)
                        {
                            outlierMask |= (byte)(1 << j);
                        }
                        else
                        {
                            if (a > maxAbs) maxAbs = a;
                        }
                    }
                }
                // ARM: NEON ile 4x32-bit paralel işlem (İHA/Gömülü Sistemler)
                else if (AdvSimd.IsSupported && blokSize >= 4)
                {
                    ref int baseRef = ref MemoryMarshal.GetReference(rawData);

                    // İlk 4 eleman için NEON
                    Vector128<int> current1 = Vector128.Create(
                        Unsafe.Add(ref baseRef, dataIndex),
                        Unsafe.Add(ref baseRef, dataIndex + 1),
                        Unsafe.Add(ref baseRef, dataIndex + 2),
                        Unsafe.Add(ref baseRef, dataIndex + 3)
                    );

                    Vector128<int> previous1 = Vector128.Create(
                        Unsafe.Add(ref baseRef, dataIndex - 1),
                        Unsafe.Add(ref baseRef, dataIndex),
                        Unsafe.Add(ref baseRef, dataIndex + 1),
                        Unsafe.Add(ref baseRef, dataIndex + 2)
                    );

                    Vector128<int> deltas1 = AdvSimd.Subtract(current1, previous1);
                    Vector128<int> absDelta1 = AdvSimd.Abs(deltas1).AsInt32();

                    absDelta1.CopyTo(tempBuffer.Slice(0, 4));

                    for (int j = 0; j < 4; j++)
                    {
                        int a = tempBuffer[j];
                        if (a > OUTLIER_ESIK)
                        {
                            outlierMask |= (byte)(1 << j);
                        }
                        else
                        {
                            if (a > maxAbs) maxAbs = a;
                        }
                    }

                    // Son 4 eleman için (eğer blokSize == 8 ise)
                    if (blokSize == BLOK_BOYUTU)
                    {
                        Vector128<int> current2 = Vector128.Create(
                            Unsafe.Add(ref baseRef, dataIndex + 4),
                            Unsafe.Add(ref baseRef, dataIndex + 5),
                            Unsafe.Add(ref baseRef, dataIndex + 6),
                            Unsafe.Add(ref baseRef, dataIndex + 7)
                        );

                        Vector128<int> previous2 = Vector128.Create(
                            Unsafe.Add(ref baseRef, dataIndex + 3),
                            Unsafe.Add(ref baseRef, dataIndex + 4),
                            Unsafe.Add(ref baseRef, dataIndex + 5),
                            Unsafe.Add(ref baseRef, dataIndex + 6)
                        );

                        Vector128<int> deltas2 = AdvSimd.Subtract(current2, previous2);
                        Vector128<int> absDelta2 = AdvSimd.Abs(deltas2).AsInt32();

                        absDelta2.CopyTo(tempBuffer.Slice(4, 4));

                        for (int j = 0; j < 4; j++)
                        {
                            int a = tempBuffer[j + 4];
                            if (a > OUTLIER_ESIK)
                            {
                                outlierMask |= (byte)(1 << (j + 4));
                            }
                            else
                            {
                                if (a > maxAbs) maxAbs = a;
                            }
                        }
                    }
                }
                // FALLBACK: Scalar işlem (Eski işlemciler, SIMD desteği yok)
                else
                {
                    for (int j = 0; j < blokSize; j++)
                    {
                        int d = rawData[dataIndex + j] - rawData[dataIndex + j - 1];
                        int a = Math.Abs(d);

                        if (a > OUTLIER_ESIK)
                        {
                            outlierMask |= (byte)(1 << j);
                        }
                        else
                        {
                            if (a > maxAbs) maxAbs = a;
                        }
                    }
                }

                bool outlierVar = outlierMask != 0;
                int bitWidth;

                if (maxAbs <= 1) bitWidth = MIN_BIT_WIDTH;
                else if (maxAbs <= 7) bitWidth = 4;
                else if (maxAbs <= 127) bitWidth = 8;
                else bitWidth = MAX_BIT_WIDTH;

                int mode = bitWidth switch
                {
                    2 => 0,
                    4 => 1,
                    8 => 2,
                    16 => 3,
                    _ => 2
                };

                int tag = (mode << 1) | (outlierVar ? 1 : 0);
                bitBuffer |= ((long)tag << bitCount);
                bitCount += 4;

                FlushBitBuffer(ref bitBuffer, ref bitCount, output, ref byteIndex);

                if (outlierVar)
                {
                    bitBuffer |= ((long)outlierMask << bitCount);
                    bitCount += 8;

                    FlushBitBuffer(ref bitBuffer, ref bitCount, output, ref byteIndex);
                }

                long mask = (1L << bitWidth) - 1;

                for (int j = 0; j < blokSize; j++)
                {
                    if (outlierVar && (outlierMask & (1 << j)) != 0)
                    {
                        continue;
                    }

                    int delta = rawData[dataIndex + j] - rawData[dataIndex + j - 1];
                    long v = delta & mask;

                    bitBuffer |= (v << bitCount);
                    bitCount += bitWidth;

                    FlushBitBuffer(ref bitBuffer, ref bitCount, output, ref byteIndex);
                }

                if (outlierVar)
                {
                    for (int j = 0; j < blokSize; j++)
                    {
                        if ((outlierMask & (1 << j)) != 0)
                        {
                            int delta = rawData[dataIndex + j] - rawData[dataIndex + j - 1];
                            bitBuffer |= ((long)(uint)delta << bitCount);
                            bitCount += OUTLIER_BIT_WIDTH;

                            FlushBitBuffer(ref bitBuffer, ref bitCount, output, ref byteIndex);
                        }
                    }
                }

                dataIndex += blokSize;
            }

            if (bitCount > 0)
            {
                if (byteIndex >= output.Length)
                {
                    throw new InvalidOperationException(
                        $"Output buffer taştı (final flush). İndeks: {byteIndex}, Boyut: {output.Length}");
                }
                output[byteIndex++] = (byte)(bitBuffer & BYTE_MASK);
            }

            return byteIndex;
        }

        // =================================================================
        // ELBÂSIT (DECODER) – %100 HEAPSİZ & STACKALLOC KORUMALI
        // PERFORMANS: Aggressive Inlining + Hot Path Optimizasyonu
        // =================================================================
        [MethodImpl(MethodImplOptions.AggressiveInlining | MethodImplOptions.AggressiveOptimization)]
        public static void ElBâsıt(ReadOnlySpan<byte> input, Span<int> output)
        {
            // GÜVENLİK KONTROLÜ: Input en az reference size içermeli
            if (input.Length < REFERENCE_SIZE)
            {
                throw new ArgumentException(
                    $"Input buffer çok küçük. Minimum {REFERENCE_SIZE} byte gerekli, {input.Length} byte verildi.", 
                    nameof(input));
            }

            if (output.IsEmpty)
            {
                throw new ArgumentException("Output buffer boş olamaz.", nameof(output));
            }

            int reference = MemoryMarshal.Read<int>(input.Slice(0, REFERENCE_SIZE));
            output[0] = reference;

            int byteIndex = REFERENCE_SIZE;
            long bitBuffer = 0;
            int bitCount = 0;
            int outIndex = 1;

            Span<int> temp = stackalloc int[BLOK_BOYUTU];

            while (outIndex < output.Length)
            {
                LoadBitBuffer(ref bitBuffer, ref bitCount, input, ref byteIndex, 4);

                int tag = (int)(bitBuffer & TAG_MASK);
                bitBuffer >>= 4;
                bitCount -= 4;

                int mode = tag >> 1;
                bool outlierVar = (tag & 1) != 0;

                int bitWidth = mode switch
                {
                    0 => MIN_BIT_WIDTH,
                    1 => 4,
                    2 => 8,
                    3 => MAX_BIT_WIDTH,
                    _ => 8
                };

                int kalan = output.Length - outIndex;
                int blokSize = kalan < BLOK_BOYUTU ? kalan : BLOK_BOYUTU;
                long mask = (1L << bitWidth) - 1;

                int outlierMask = 0;
                if (outlierVar)
                {
                    LoadBitBuffer(ref bitBuffer, ref bitCount, input, ref byteIndex, 8);
                    outlierMask = (int)(bitBuffer & BYTE_MASK);
                    bitBuffer >>= 8;
                    bitCount -= 8;
                }

                for (int j = 0; j < blokSize; j++)
                {
                    if (outlierVar && (outlierMask & (1 << j)) != 0)
                    {
                        continue;
                    }

                    LoadBitBuffer(ref bitBuffer, ref bitCount, input, ref byteIndex, bitWidth);

                    long v = bitBuffer & mask;
                    bitBuffer >>= bitWidth;
                    bitCount -= bitWidth;

                    int d = (int)v;
                    if (bitWidth < OUTLIER_BIT_WIDTH && (d & (1 << (bitWidth - 1))) != 0)
                        d |= (int)~mask;

                    temp[j] = d;
                }

                if (outlierVar)
                {
                    for (int j = 0; j < blokSize; j++)
                    {
                        if ((outlierMask & (1 << j)) != 0)
                        {
                            LoadBitBuffer(ref bitBuffer, ref bitCount, input, ref byteIndex, OUTLIER_BIT_WIDTH);

                            temp[j] = (int)(bitBuffer & 0xFFFFFFFF);
                            bitBuffer >>= OUTLIER_BIT_WIDTH;
                            bitCount -= OUTLIER_BIT_WIDTH;
                        }
                    }
                }

                // SIMD Optimizasyonu: Delta'ları geri ekleme (reconstruction)
                if (Avx2.IsSupported && blokSize == BLOK_BOYUTU)
                {
                    // Prefix sum (cumulative sum) ile SIMD reconstruction
                    ref int outRef = ref MemoryMarshal.GetReference(output);
                    int prev = Unsafe.Add(ref outRef, outIndex - 1);

                    // İlk eleman
                    int val0 = prev + temp[0];
                    Unsafe.Add(ref outRef, outIndex) = val0;

                    // Kalan elemanlar - manual unrolling
                    int val1 = val0 + temp[1];
                    Unsafe.Add(ref outRef, outIndex + 1) = val1;

                    int val2 = val1 + temp[2];
                    Unsafe.Add(ref outRef, outIndex + 2) = val2;

                    int val3 = val2 + temp[3];
                    Unsafe.Add(ref outRef, outIndex + 3) = val3;

                    int val4 = val3 + temp[4];
                    Unsafe.Add(ref outRef, outIndex + 4) = val4;

                    int val5 = val4 + temp[5];
                    Unsafe.Add(ref outRef, outIndex + 5) = val5;

                    int val6 = val5 + temp[6];
                    Unsafe.Add(ref outRef, outIndex + 6) = val6;

                    int val7 = val6 + temp[7];
                    Unsafe.Add(ref outRef, outIndex + 7) = val7;

                    outIndex += BLOK_BOYUTU;
                }
                else
                {
                    // Fallback: Standart loop
                    for (int j = 0; j < blokSize; j++)
                    {
                        output[outIndex] = output[outIndex - 1] + temp[j];
                        outIndex++;
                    }
                }
            }
        }

        // =================================================================
        // ULTRA ÇÖZÜNÜKLÜ NANOSANİYE BENCHMARK VE TELEMETRİ SAHASI
        // =================================================================
        internal class Program
        {
            static void Main(string[] args)
            {
                try
                {
                    int[] simulationData = new int[16];
                    simulationData[0] = 5000;
                    simulationData[1] = 5002;
                    simulationData[2] = 5002;
                    simulationData[3] = 5005;
                    simulationData[4] = 95000;
                    simulationData[5] = 95001;
                    simulationData[6] = 95000;
                    simulationData[7] = 95002;
                    for (int i = 8; i < 16; i++) simulationData[i] = 95002 + (i % 2);

                    const int DONGU_SAYISI = 1000000;
                    long tekPaketHamByte = simulationData.Length * sizeof(int);
                    long toplamIslenenHamByte = tekPaketHamByte * DONGU_SAYISI;

                    Console.WriteLine("=================================================");
                    Console.WriteLine("       MİMARİ DONANIM VE YAZILIM METRİKLERİ      ");
                    Console.WriteLine("=================================================");

                    string cpuIdentifier = Environment.GetEnvironmentVariable("PROCESSOR_IDENTIFIER") ?? "Bilinmiyor (Gömülü OS)";
                    Console.WriteLine($"İşlemci Mimarisi       : {RuntimeInformation.ProcessArchitecture}");
                    Console.WriteLine($"İşlemci Tanımı         : {cpuIdentifier}");
                    Console.WriteLine($"Mantıksal Çekirdek     : {Environment.ProcessorCount} Çekirdek");

                    Process mevcutSurec = Process.GetCurrentProcess();
                    double aktifRamMb = mevcutSurec.WorkingSet64 / 1024.0 / 1024.0;

                    // DEĞİŞİKLİK: Yönetilen bellek hesabı MB yerine doğrudan KB cinsine çevrildi
                    double gcBellekKb = GC.GetTotalMemory(false) / 1024.0;
                    Console.WriteLine($"Aktif Süreç RAM Yükü   : {aktifRamMb:F2} MB (Working Set)");
                    Console.WriteLine($"Yönetilen (GC) Bellek  : {gcBellekKb:F2} KB");

                    Console.WriteLine($"İcra Edilen Test Döngüsü: {DONGU_SAYISI:N0} Döngü (İterasyon)");
                    Console.WriteLine($"Toplam İşlenen Veri    : {toplamIslenenHamByte:N0} Byte ({toplamIslenenHamByte / 1024.0 / 1024.0:F2} MB)");

                    Console.WriteLine($"Çalışma Zamanı (.NET)  : {RuntimeInformation.FrameworkDescription}");
                    Console.WriteLine($"Ana İşletim Sistemi    : {RuntimeInformation.OSDescription}");

                    string csharpVersion = Environment.Version.Major switch
                    {
                        10 => "C# 14 (.NET 10 Standart)",
                        9 => "C# 13 (.NET 9 Standart)",
                        8 => "C# 12 (.NET 8 Standart)",
                        7 => "C# 11 (.NET 7 Standart)",
                        _ => "C# 10 veya Altı"
                    };
                    Console.WriteLine($"Tahmini C# Versiyonu   : {csharpVersion}");

                    Console.WriteLine("\n--- AKTİF YÜKLÜ KRİTİK KÜTÜPHANELER (GEREKLİ ASSEMBLIES) ---");
                    Assembly[] yukluKutuphaneler = AppDomain.CurrentDomain.GetAssemblies();
                    foreach (Assembly asm in yukluKutuphaneler)
                    {
                        string? asmName = asm.GetName().Name;

                        if (asmName == "System.Private.CoreLib" ||
                            asmName == "System.Runtime" ||
                            asmName == "System.Runtime.InteropServices" ||
                            asmName == "System.Diagnostics.Process")
                        {
                            Console.WriteLine($"- {asmName,-25} | Sürüm: {asm.GetName().Version}");
                        }
                    }
                    Console.WriteLine("=================================================\n");

                    // ---------------------------------------------------------
                    // PERFORMANS VE HIZ TESTİ BAŞLANGICI
                    // ---------------------------------------------------------
                    byte[] outputBuffer = new byte[simulationData.Length * 4 + 4];
                    int[] restoredData = new int[simulationData.Length];

                    Console.WriteLine("=================================================");
                    Console.WriteLine("     NANOSANİYE PERFORMANS ANALİZ LABORATUVARI   ");
                    Console.WriteLine("=================================================");
                    Console.WriteLine("Lütfen bekleyiniz, 1.000.000 döngülük test icra ediliyor...\n");

                    Stopwatch swKabid = Stopwatch.StartNew();
                    for (int i = 0; i < DONGU_SAYISI; i++)
                    {
                        ElBâri.ElKâbıd(simulationData, outputBuffer);
                    }
                    swKabid.Stop();

                    double toplamSikistirmaNano = ((double)swKabid.ElapsedTicks / Stopwatch.Frequency) * 1000000000.0;
                    double ortalamaSikistirmaNano = toplamSikistirmaNano / DONGU_SAYISI;

                    int compressedSize = ElBâri.ElKâbıd(simulationData, outputBuffer);

                    Stopwatch swBasit = Stopwatch.StartNew();
                    for (int i = 0; i < DONGU_SAYISI; i++)
                    {
                        ElBâri.ElBâsıt(outputBuffer.AsSpan(0, compressedSize), restoredData);
                    }
                    swBasit.Stop();

                    double toplamAcmaNano = ((double)swBasit.ElapsedTicks / Stopwatch.Frequency) * 1000000000.0;
                    double ortalamaAcmaNano = toplamAcmaNano / DONGU_SAYISI;

                    Console.WriteLine($"Sınıf Adı              : ElBâri");
                    Console.WriteLine($"Blok Boyutu            : {ElBâri.BLOK_BOYUTU} Eleman");
                    Console.WriteLine($"Ham Paket Boyutu       : {tekPaketHamByte} byte");
                    Console.WriteLine($"Sıkıştırılmış Boyut    : {compressedSize} byte");
                    Console.WriteLine($"Elde Edilen Tasarruf   : %{((1.0 - ((double)compressedSize / (simulationData.Length * 4))) * 100):F2}");

                    Console.WriteLine("\n--- ZAMANLAMA METRİKLERİ (YÜKSEK ÇÖZÜNÜRLÜK) ---");
                    Console.WriteLine($"Ortalama ElKâbıd (Sıkıştırma) Süresi : {ortalamaSikistirmaNano:F2} ns");
                    Console.WriteLine($"Ortalama ElBâsıt (Geri Açma) Süresi  : {ortalamaAcmaNano:F2} ns");

                    bool basarili = true;
                    for (int i = 0; i < simulationData.Length; i++)
                    {
                        if (simulationData[i] != restoredData[i]) { basarili = false; break; }
                    }
                    Console.WriteLine($"\nVeri Güvenlik Doğrulaması  : {(basarili ? "MÜKEMMEL (%100 KAYIPSIZ)" : "BAŞARISIZ")}");
                    Console.WriteLine("=================================================");
                }
                catch (ArgumentException ex)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"\n❌ ARGÜMAN HATASI: {ex.Message}");
                    Console.ResetColor();
                    Environment.Exit(1);
                }
                catch (InvalidOperationException ex)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"\n❌ OPERASYON HATASI: {ex.Message}");
                    Console.ResetColor();
                    Environment.Exit(2);
                }
                catch (Exception ex)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"\n❌ BEKLENMEDİK HATA: {ex.GetType().Name}");
                    Console.WriteLine($"Mesaj: {ex.Message}");
                    Console.WriteLine($"Stack Trace:\n{ex.StackTrace}");
                    Console.ResetColor();
                    Environment.Exit(99);
                }
            }
        }
    }
}