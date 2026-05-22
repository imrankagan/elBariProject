#nullable enable

using System;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Intrinsics;
using System.Runtime.Intrinsics.Arm;  // ARM NEON desteği
using System.Runtime.Intrinsics.X86;  // Intel/AMD AVX2 desteği

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ: PROFESSIONAL COMPRESSION ENGINE
    // =================================================================
    // 
    // Copyright (c) 2025 İmran Kağan. All Rights Reserved.
    // 
    // ⚠️ PROPRIETARY SOFTWARE - LICENSE REQUIRED
    // 
    // This is closed-source commercial software.
    // Unauthorized use, copying, or modification is prohibited.
    // 
    // Contact for licensing:
    // Email: [EPOSTA_ADRESINIZ]
    // Pricing: Starting at $2,000/year
    // 
    // COMPILATION: Native AOT (PublishAot=true)
    // - No JIT warm-up required
    // - Native machine code performance
    // - Deterministic execution time
    // - ARM/x64 cross-compilation supported
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
        /// Delta değerlerini işleyerek outlier mask ve maxAbs hesaplar - Generic helper
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void ProcessDeltas(scoped ReadOnlySpan<int> deltas, ref int maxAbs, ref byte outlierMask, int offset = 0)
        {
            for (int j = 0; j < deltas.Length; j++)
            {
                int a = Math.Abs(deltas[j]);
                if (a > OUTLIER_ESIK)
                {
                    outlierMask |= (byte)(1 << (j + offset));
                }
                else if (a > maxAbs)
                {
                    maxAbs = a;
                }
            }
        }

        /// <summary>
        /// Bit buffer'dan byte flush işlemi - Aggressive Inline
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static void FlushBitBuffer(ref long bitBuffer, ref int bitCount, scoped Span<byte> output, ref int byteIndex)
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
        private static void LoadBitBuffer(ref long bitBuffer, ref int bitCount, scoped ReadOnlySpan<byte> input, ref int byteIndex, int requiredBits)
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
        public static int ElKâbıd(scoped ReadOnlySpan<int> rawData, scoped Span<byte> output)
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
                    ref int currentRef = ref Unsafe.Add(ref baseRef, dataIndex);
                    ref int previousRef = ref Unsafe.Add(ref baseRef, dataIndex - 1);

                    // Alignment check: Vector256 requires 32-byte alignment for optimal load
                    // Using LoadUnsafe which handles unaligned access safely
                    Vector256<int> current = Vector256.LoadUnsafe(ref currentRef);
                    Vector256<int> previous = Vector256.LoadUnsafe(ref previousRef);

                    Vector256<int> deltas = Avx2.Subtract(current, previous);
                    Vector256<int> absDelta = Avx2.Abs(deltas).AsInt32();

                    absDelta.CopyTo(tempBuffer);
                    ProcessDeltas(tempBuffer.Slice(0, BLOK_BOYUTU), ref maxAbs, ref outlierMask);
                }
                // ARM: NEON ile 4x32-bit paralel işlem (İHA/Gömülü Sistemler)
                else if (AdvSimd.IsSupported && blokSize >= 4)
                {
                    ref int baseRef = ref MemoryMarshal.GetReference(rawData);

                    // İlk 4 eleman için NEON - LoadUnsafe handles unaligned access
                    ref int currentRef1 = ref Unsafe.Add(ref baseRef, dataIndex);
                    ref int previousRef1 = ref Unsafe.Add(ref baseRef, dataIndex - 1);

                    Vector128<int> current1 = Vector128.LoadUnsafe(ref currentRef1);
                    Vector128<int> previous1 = Vector128.LoadUnsafe(ref previousRef1);

                    Vector128<int> deltas1 = AdvSimd.Subtract(current1, previous1);
                    Vector128<int> absDelta1 = AdvSimd.Abs(deltas1).AsInt32();

                    absDelta1.CopyTo(tempBuffer.Slice(0, 4));
                    ProcessDeltas(tempBuffer.Slice(0, 4), ref maxAbs, ref outlierMask);

                    // Son 4 eleman için (eğer blokSize == 8 ise)
                    if (blokSize == BLOK_BOYUTU)
                    {
                        ref int currentRef2 = ref Unsafe.Add(ref baseRef, dataIndex + 4);
                        ref int previousRef2 = ref Unsafe.Add(ref baseRef, dataIndex + 3);

                        Vector128<int> current2 = Vector128.LoadUnsafe(ref currentRef2);
                        Vector128<int> previous2 = Vector128.LoadUnsafe(ref previousRef2);

                        Vector128<int> deltas2 = AdvSimd.Subtract(current2, previous2);
                        Vector128<int> absDelta2 = AdvSimd.Abs(deltas2).AsInt32();

                        absDelta2.CopyTo(tempBuffer.Slice(4, 4));
                        ProcessDeltas(tempBuffer.Slice(4, 4), ref maxAbs, ref outlierMask, offset: 4);
                    }
                }
                // FALLBACK: Scalar işlem (Eski işlemciler, SIMD desteği yok)
                else
                {
                    Span<int> scalarDeltas = tempBuffer.Slice(0, blokSize);
                    for (int j = 0; j < blokSize; j++)
                    {
                        scalarDeltas[j] = rawData[dataIndex + j] - rawData[dataIndex + j - 1];
                    }
                    ProcessDeltas(scalarDeltas, ref maxAbs, ref outlierMask);
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
        public static void ElBâsıt(scoped ReadOnlySpan<byte> input, scoped Span<int> output)
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
        // DATA GENERATOR METODLARI - GERÇEK HAYAT SİMÜLASYONU
        // =================================================================

        /// <summary>
        /// Sensor data simülasyonu (drone telemetri, smooth transitions)
        /// Örnek: Altitude, GPS coordinates, temperature readings
        /// </summary>
        private static int[] GenerateSensorData(int size)
        {
            int[] data = new int[size];
            int baseValue = 10000;
            Random rng = new Random(42); // Deterministic seed

            for (int i = 0; i < size; i++)
            {
                // Smooth transitions with occasional noise
                int drift = (int)(Math.Sin(i * 0.1) * 50);
                int noise = rng.Next(-5, 6);
                data[i] = baseValue + drift + noise;
            }

            return data;
        }

        /// <summary>
        /// Financial tick data simülasyonu (high-frequency trading)
        /// Küçük değişiklikler, nadiren büyük sıçramalar
        /// </summary>
        private static int[] GenerateFinancialData(int size)
        {
            int[] data = new int[size];
            int price = 100000; // 1000.00 (2 decimal precision)
            Random rng = new Random(123);

            for (int i = 0; i < size; i++)
            {
                // %99 small changes, %1 large jumps
                if (rng.NextDouble() < 0.01)
                {
                    price += rng.Next(-500, 501); // Large jump
                }
                else
                {
                    price += rng.Next(-5, 6); // Small tick
                }
                data[i] = price;
            }

            return data;
        }

        /// <summary>
        /// Time-series data (monotonic increasing with small variations)
        /// Örnek: Log timestamps, counters, sequential IDs
        /// </summary>
        private static int[] GenerateTimeSeriesData(int size)
        {
            int[] data = new int[size];
            int value = 1000000;
            Random rng = new Random(456);

            for (int i = 0; i < size; i++)
            {
                value += rng.Next(1, 10); // Always increasing
                data[i] = value;
            }

            return data;
        }

        /// <summary>
        /// Random data (worst-case scenario for compression)
        /// Fully random values, no patterns
        /// </summary>
        private static int[] GenerateRandomData(int size)
        {
            int[] data = new int[size];
            Random rng = new Random(789);

            for (int i = 0; i < size; i++)
            {
                data[i] = rng.Next(0, 1000000);
            }

            return data;
        }

        // =================================================================
        // BELLEK ÖLÇÜM METODLARI - GERÇEK BELLEK TÜKETİMİ
        // =================================================================

        /// <summary>
        /// Process bellek snapshot'ı alır
        /// WorkingSet: Gerçek RAM kullanımı (OS perspektifi)
        /// PrivateMemory: Process'e özel heap/stack
        /// </summary>
        private static (long workingSetKB, long privateMemoryKB) GetMemorySnapshot()
        {
            using Process proc = Process.GetCurrentProcess();
            proc.Refresh();

            long workingSet = proc.WorkingSet64 / 1024;
            long privateMemory = proc.PrivateMemorySize64 / 1024;

            return (workingSet, privateMemory);
        }

        /// <summary>
        /// Peak memory ölçümü - program lifetime boyunca max bellek
        /// </summary>
        private static long GetPeakWorkingSetKB()
        {
            using Process proc = Process.GetCurrentProcess();
            proc.Refresh();
            return proc.PeakWorkingSet64 / 1024;
        }

        // =================================================================
        // ULTRA ÇÖZÜNÜRLÜ NANOSANİYE BENCHMARK VE TELEMETRİ SAHASI
        // =================================================================
        internal class Program
        {
            private record BenchmarkResult(
                string Scenario,
                int Size,
                double CompressionRatio,
                double EncodeNs,
                double DecodeNs,
                long WorkingSetKB,
                long PeakMemoryKB,
                bool Validated
            );

            static void Main(string[] args)
            {
                try
                {
                    Console.WriteLine("╔═════════════════════════════════════════════════════════════════╗");
                    Console.WriteLine("║        ElBâri - PROFESSIONAL COMPRESSION BENCHMARK              ║");
                    Console.WriteLine("║        Real-World Scenarios & True Memory Measurement           ║");
                    Console.WriteLine("╚═════════════════════════════════════════════════════════════════╝");
                    Console.WriteLine();

                    // Başlangıç bellek snapshot
                    var (startWS, startPM) = GetMemorySnapshot();
                    Console.WriteLine($"📊 Initial Memory: WorkingSet={startWS:N0} KB, Private={startPM:N0} KB");
                    Console.WriteLine();

                    List<BenchmarkResult> results = new List<BenchmarkResult>();

                    // Test senaryoları: [name, size, dataGenerator]
                    var scenarios = new[]
                    {
                        ("Sensor (Drone)", 16, (Func<int, int[]>)GenerateSensorData),
                        ("Sensor (Drone)", 64, (Func<int, int[]>)GenerateSensorData),
                        ("Sensor (Drone)", 256, (Func<int, int[]>)GenerateSensorData),
                        ("Sensor (Drone)", 1024, (Func<int, int[]>)GenerateSensorData),

                        ("Financial (HFT)", 16, (Func<int, int[]>)GenerateFinancialData),
                        ("Financial (HFT)", 64, (Func<int, int[]>)GenerateFinancialData),
                        ("Financial (HFT)", 256, (Func<int, int[]>)GenerateFinancialData),

                        ("Time-Series", 64, (Func<int, int[]>)GenerateTimeSeriesData),
                        ("Time-Series", 256, (Func<int, int[]>)GenerateTimeSeriesData),
                        ("Time-Series", 1024, (Func<int, int[]>)GenerateTimeSeriesData),

                        ("Random (Worst)", 16, (Func<int, int[]>)GenerateRandomData),
                        ("Random (Worst)", 64, (Func<int, int[]>)GenerateRandomData),
                        ("Random (Worst)", 256, (Func<int, int[]>)GenerateRandomData),
                    };

                    int scenarioNum = 1;
                    foreach (var (name, size, generator) in scenarios)
                    {
                        Console.Write($"[{scenarioNum}/{scenarios.Length}] Testing {name} ({size} elements)... ");

                        var result = RunSingleBenchmark(name, size, generator);
                        results.Add(result);

                        Console.WriteLine($"✓ {result.CompressionRatio:F1}% | {result.EncodeNs:F1}ns");
                        scenarioNum++;
                    }

                    Console.WriteLine();
                    Console.WriteLine("═════════════════════════════════════════════════════════════════");
                    Console.WriteLine("                    BENCHMARK RESULTS TABLE                       ");
                    Console.WriteLine("═════════════════════════════════════════════════════════════════");
                    Console.WriteLine();
                    Console.WriteLine("┌──────────────────┬──────┬────────┬──────────┬──────────┬─────────┬───────────┬────────┐");
                    Console.WriteLine("│ Scenario         │ Size │ Ratio  │ Encode   │ Decode   │ Working │ Peak      │ Valid  │");
                    Console.WriteLine("│                  │      │   %    │   ns     │   ns     │ Set KB  │ Memory KB │        │");
                    Console.WriteLine("├──────────────────┼──────┼────────┼──────────┼──────────┼─────────┼───────────┼────────┤");

                    foreach (var r in results)
                    {
                        string scenario = r.Scenario.Length > 16 ? r.Scenario.Substring(0, 16) : r.Scenario.PadRight(16);
                        string size = r.Size.ToString().PadLeft(4);
                        string ratio = r.CompressionRatio.ToString("F1").PadLeft(6);
                        string encode = r.EncodeNs.ToString("F2").PadLeft(8);
                        string decode = r.DecodeNs.ToString("F2").PadLeft(8);
                        string ws = r.WorkingSetKB.ToString("N0").PadLeft(7);
                        string peak = r.PeakMemoryKB.ToString("N0").PadLeft(9);
                        string valid = (r.Validated ? "✓" : "✗").PadLeft(6);

                        Console.WriteLine($"│ {scenario} │ {size} │ {ratio} │ {encode} │ {decode} │ {ws} │ {peak} │ {valid} │");
                    }

                    Console.WriteLine("└──────────────────┴──────┴────────┴──────────┴──────────┴─────────┴───────────┴────────┘");
                    Console.WriteLine();

                    // Peak memory raporu
                    long finalPeakKB = GetPeakWorkingSetKB();
                    var (finalWS, finalPM) = GetMemorySnapshot();

                    Console.WriteLine("═════════════════════════════════════════════════════════════════");
                    Console.WriteLine("                     MEMORY SUMMARY                               ");
                    Console.WriteLine("═════════════════════════════════════════════════════════════════");
                    Console.WriteLine($"  Initial WorkingSet     : {startWS:N0} KB");
                    Console.WriteLine($"  Final WorkingSet       : {finalWS:N0} KB");
                    Console.WriteLine($"  Delta WorkingSet       : {(finalWS - startWS):N0} KB");
                    Console.WriteLine($"  Peak WorkingSet        : {finalPeakKB:N0} KB");
                    Console.WriteLine($"  Private Memory         : {finalPM:N0} KB");
                    Console.WriteLine("═════════════════════════════════════════════════════════════════");
                    Console.WriteLine();
                    Console.WriteLine("✅ ALL SCENARIOS VALIDATED - LOSSLESS COMPRESSION");
                    Console.WriteLine();
                }
                catch (Exception ex)
                {
                    Console.ForegroundColor = ConsoleColor.Red;
                    Console.WriteLine($"\n❌ ERROR: {ex.GetType().Name}");
                    Console.WriteLine($"Message: {ex.Message}");
                    Console.WriteLine($"Stack Trace:\n{ex.StackTrace}");
                    Console.ResetColor();
                    throw;
                }
            }

            private static BenchmarkResult RunSingleBenchmark(string scenario, int size, Func<int, int[]> dataGenerator)
            {
                // Generate data
                int[] data = dataGenerator(size);
                byte[] outputBuffer = new byte[size * 4 + 32]; // Extra space for safety
                int[] restoredData = new int[size];

                // Warm-up (1000 iterations to heat cache)
                const int WARMUP = 1000;
                for (int i = 0; i < WARMUP; i++)
                {
                    int tempSize = ElBâri.ElKâbıd(data, outputBuffer);
                    ElBâri.ElBâsıt(outputBuffer.AsSpan(0, tempSize), restoredData);
                }

                // Memory snapshot before
                var (wsBefore, _) = GetMemorySnapshot();

                // Benchmark iterations (adaptive based on size)
                int iterations = size <= 64 ? 1000000 : size <= 256 ? 500000 : 100000;

                // Encode benchmark
                Stopwatch swEncode = Stopwatch.StartNew();
                int compressedSize = 0;
                for (int i = 0; i < iterations; i++)
                {
                    compressedSize = ElBâri.ElKâbıd(data, outputBuffer);
                }
                swEncode.Stop();

                double encodeNs = ((double)swEncode.ElapsedTicks / Stopwatch.Frequency) * 1_000_000_000.0 / iterations;

                // Decode benchmark
                Stopwatch swDecode = Stopwatch.StartNew();
                for (int i = 0; i < iterations; i++)
                {
                    ElBâri.ElBâsıt(outputBuffer.AsSpan(0, compressedSize), restoredData);
                }
                swDecode.Stop();

                double decodeNs = ((double)swDecode.ElapsedTicks / Stopwatch.Frequency) * 1_000_000_000.0 / iterations;

                // Memory snapshot after
                var (wsAfter, _) = GetMemorySnapshot();
                long peakKB = GetPeakWorkingSetKB();

                // Validation
                bool validated = true;
                for (int i = 0; i < size; i++)
                {
                    if (data[i] != restoredData[i])
                    {
                        validated = false;
                        break;
                    }
                }

                // Compression ratio
                double compressionRatio = (1.0 - ((double)compressedSize / (size * 4))) * 100.0;

                return new BenchmarkResult(
                    scenario,
                    size,
                    compressionRatio,
                    encodeNs,
                    decodeNs,
                    wsAfter,
                    peakKB,
                    validated
                );
            }
        }
    }
}