// ElBâri Compression Engine - Benchmark Runner
// Copyright (c) 2025. All rights reserved.
// Proprietary and Confidential - Unauthorized access prohibited.

using System;
using System.Diagnostics;
using System.Linq;

namespace ElBâri.Benchmarks;

/// <summary>
/// Benchmark sonucu.
/// </summary>
public record BenchmarkResult
{
    public required string ScenarioName { get; init; }
    public required string Category { get; init; }
    public required int InputSize { get; init; }
    public required int InputBytes { get; init; }
    public required int CompressedBytes { get; init; }
    public required double CompressionRatio { get; init; }
    public required long EncodeNanoseconds { get; init; }
    public required long DecodeNanoseconds { get; init; }
    public required bool RoundTripSuccess { get; init; }
    public required string Status { get; init; }
    public string? ErrorMessage { get; init; }
}

/// <summary>
/// ElBâri benchmark runner - tüm senaryoları test eder.
/// </summary>
public static class BenchmarkRunner
{
    private const int WARMUP_ITERATIONS = 100;
    private const int MEASUREMENT_ITERATIONS = 1000;

    /// <summary>
    /// Tüm benchmark senaryolarını çalıştır.
    /// </summary>
    public static void RunAll(bool verbose = true)
    {
        Console.WriteLine("╔════════════════════════════════════════════════════════════════════════╗");
        Console.WriteLine("║         ElBâri Compression Engine - Benchmark Suite                   ║");
        Console.WriteLine("║         Copyright (c) 2025. All rights reserved.                      ║");
        Console.WriteLine("╚════════════════════════════════════════════════════════════════════════╝");
        Console.WriteLine();

        var scenarios = TestScenarios.GetAllScenarios();
        Console.WriteLine($"Total Scenarios: {scenarios.Length}");
        Console.WriteLine($"Warmup Iterations: {WARMUP_ITERATIONS}");
        Console.WriteLine($"Measurement Iterations: {MEASUREMENT_ITERATIONS}");
        Console.WriteLine();

        var results = new BenchmarkResult[scenarios.Length];
        int passCount = 0;
        int failCount = 0;
        int rejectedCount = 0;

        for (int i = 0; i < scenarios.Length; i++)
        {
            var scenario = scenarios[i];
            Console.WriteLine($"[{i + 1}/{scenarios.Length}] Running: {scenario.Name}");

            if (verbose)
            {
                Console.WriteLine($"    Category: {scenario.Category}");
                Console.WriteLine($"    Description: {scenario.Description}");
                Console.WriteLine($"    Input Size: {scenario.InputData.Length} elements");
            }

            var result = RunScenario(scenario);
            results[i] = result;

            if (result.Status == "PASS")
            {
                passCount++;
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine($"    ✓ PASS");
            }
            else if (result.Status == "REJECTED")
            {
                rejectedCount++;
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine($"    ⊘ REJECTED: {result.ErrorMessage}");
            }
            else
            {
                failCount++;
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"    ✗ FAIL: {result.ErrorMessage}");
            }
            Console.ResetColor();

            if (verbose && result.Status == "PASS")
            {
                Console.WriteLine($"    Compression: {result.InputBytes} → {result.CompressedBytes} bytes ({result.CompressionRatio:F2}x)");
                Console.WriteLine($"    Encode: {result.EncodeNanoseconds:N0} ns ({result.InputSize / (result.EncodeNanoseconds / 1e9):F0} items/sec)");
                Console.WriteLine($"    Decode: {result.DecodeNanoseconds:N0} ns ({result.InputSize / (result.DecodeNanoseconds / 1e9):F0} items/sec)");
            }
            Console.WriteLine();
        }

        // Summary
        PrintSummary(results, passCount, failCount, rejectedCount);
        PrintCategoryBreakdown(results);
        PrintTopPerformers(results);
    }

    /// <summary>
    /// Tek bir senaryoyu çalıştır ve sonuç döndür.
    /// </summary>
    public static BenchmarkResult RunScenario(TestScenario scenario)
    {
        try
        {
            int[] input = scenario.InputData;
            int inputSize = input.Length;
            int inputBytes = inputSize * sizeof(int);

            // Edge case: Empty array
            if (inputSize == 0)
            {
                return new BenchmarkResult
                {
                    ScenarioName = scenario.Name,
                    Category = scenario.Category,
                    InputSize = 0,
                    InputBytes = 0,
                    CompressedBytes = 0,
                    CompressionRatio = 0,
                    EncodeNanoseconds = 0,
                    DecodeNanoseconds = 0,
                    RoundTripSuccess = true,
                    Status = "PASS",
                    ErrorMessage = "Empty array - no compression needed"
                };
            }

            // Çok kanallı / çerçeveli senaryolar ayrı yoldan ölçülür
            if (scenario.KanalSayisi > 1 || scenario.CerceveBoyutu > 0)
            {
                return RunKatmanliScenario(scenario);
            }

            // Allocate buffers
            byte[] compressed = new byte[inputBytes * 2]; // Worst case
            int[] decompressed = new int[inputSize];

            // QUICK CHECK: Encoder veriyi kabul ediyor mu?
            int quickCheck = ElBâri.ElKâbıd(input, compressed);
            if (quickCheck == -1)
            {
                // Encoder veriyi reddetti (incompressible/meaningless data)
                return new BenchmarkResult
                {
                    ScenarioName = scenario.Name,
                    Category = scenario.Category,
                    InputSize = inputSize,
                    InputBytes = inputBytes,
                    CompressedBytes = 0,
                    CompressionRatio = 0,
                    EncodeNanoseconds = 0,
                    DecodeNanoseconds = 0,
                    RoundTripSuccess = true, // Rejection is not a failure
                    Status = "REJECTED",
                    ErrorMessage = "Data rejected by encoder (incompressible/meaningless)"
                };
            }

            // Warmup
            // NOT: ElBâsıt'a sıkıştırılmış verinin TAM boyutu verilmelidir.
            // Tampon fazla büyük verilirse çözücü, yapısal tüketim kontrolü
            // gereği bunu "bu akış benden çıkmamış" diye reddeder. Çözücü
            // verinin nerede bittiğini kendi başına bilemez.
            for (int i = 0; i < WARMUP_ITERATIONS; i++)
            {
                ElBâri.ElKâbıd(input, compressed);
                ElBâri.ElBâsıt(compressed.AsSpan(0, quickCheck), decompressed);
            }

            // Measure Encode
            long encodeNanos = MeasureOperation(() =>
            {
                ElBâri.ElKâbıd(input, compressed);
            }, MEASUREMENT_ITERATIONS);

            // Gerçek sıkıştırılmış boyut: ElKâbıd'ın döndürdüğü byte sayısı.
            // (quickCheck yukarıda aynı girdiyle encode edildi; encode deterministik olduğu
            //  için boyut birebir aynıdır. Sondaki-sıfır tahmini kullanmak yanlış sonuç verirdi:
            //  bit-packing çıktısı meşru olarak 0x00 ile bitebilir ve boyut olduğundan küçük ölçülürdü.)
            int compressedSize = quickCheck;

            // Measure Decode (tam boyut ile — yukarıdaki nota bakınız)
            long decodeNanos = MeasureOperation(() =>
            {
                ElBâri.ElBâsıt(compressed.AsSpan(0, compressedSize), decompressed);
            }, MEASUREMENT_ITERATIONS);

            // Validate round-trip
            bool roundTripSuccess = ValidateRoundTrip(input, decompressed);

            if (!roundTripSuccess)
            {
                return new BenchmarkResult
                {
                    ScenarioName = scenario.Name,
                    Category = scenario.Category,
                    InputSize = inputSize,
                    InputBytes = inputBytes,
                    CompressedBytes = compressedSize,
                    CompressionRatio = 0,
                    EncodeNanoseconds = encodeNanos,
                    DecodeNanoseconds = decodeNanos,
                    RoundTripSuccess = false,
                    Status = "FAIL",
                    ErrorMessage = "Round-trip validation failed - data corruption"
                };
            }

            double compressionRatio = (double)inputBytes / compressedSize;

            return new BenchmarkResult
            {
                ScenarioName = scenario.Name,
                Category = scenario.Category,
                InputSize = inputSize,
                InputBytes = inputBytes,
                CompressedBytes = compressedSize,
                CompressionRatio = compressionRatio,
                EncodeNanoseconds = encodeNanos,
                DecodeNanoseconds = decodeNanos,
                RoundTripSuccess = true,
                Status = "PASS"
            };
        }
        catch (Exception ex)
        {
            return new BenchmarkResult
            {
                ScenarioName = scenario.Name,
                Category = scenario.Category,
                InputSize = scenario.InputData.Length,
                InputBytes = scenario.InputData.Length * sizeof(int),
                CompressedBytes = 0,
                CompressionRatio = 0,
                EncodeNanoseconds = 0,
                DecodeNanoseconds = 0,
                RoundTripSuccess = false,
                Status = "FAIL",
                ErrorMessage = $"Exception: {ex.Message}"
            };
        }
    }

    /// <summary>
    /// Çok kanallı (ElBâriKanal) ve çerçeveli (ElBâriÇerçeve) senaryoları ölçer.
    /// Her iki katman da tahsisatsız çalışır: tamponlar burada bir kez ayrılır.
    /// </summary>
    private static BenchmarkResult RunKatmanliScenario(TestScenario scenario)
    {
        int[] input = scenario.InputData;
        int kanal = scenario.KanalSayisi;
        int inputSize = input.Length;
        int inputBytes = inputSize * sizeof(int);

        // Kayıt sınırına hizala (eksik kayıt kalmasın)
        int kayitSayisi = inputSize / kanal;
        int kullanilanEleman = kayitSayisi * kanal;

        long encodeNanos, decodeNanos;
        int compressedSize;
        bool roundTripSuccess;

        if (scenario.CerceveBoyutu > 0)
        {
            // ---- ÇERÇEVELİ YOL ----
            int kpc = scenario.CerceveBoyutu;
            int cerceveSayisi = (kayitSayisi + kpc - 1) / kpc;

            int[] calisma = new int[Math.Max(1, ElBâriÇerçeve.GerekliCalismaAlani(kpc, kanal))];
            byte[] tampon = new byte[ElBâriÇerçeve.EnKotuDurumCerceveBoyutu(kpc, kanal)];
            byte[][] paketler = new byte[cerceveSayisi][];

            // Isınma + paketleri üret
            int toplamBayt = 0;
            for (int w = 0; w < WARMUP_ITERATIONS / 10 + 1; w++)
            {
                toplamBayt = 0;
                uint sira = 0;
                int idx = 0;
                for (int i = 0; i < kayitSayisi; i += kpc)
                {
                    int a = Math.Min(kpc, kayitSayisi - i);
                    int n = ElBâriÇerçeve.CerceveYaz(input.AsSpan(i * kanal, a * kanal), kanal, sira++, calisma, tampon);
                    paketler[idx++] = tampon.AsSpan(0, n).ToArray();
                    toplamBayt += n;
                }
            }
            compressedSize = toplamBayt;

            encodeNanos = MeasureOperation(() =>
            {
                uint sira = 0;
                for (int i = 0; i < kayitSayisi; i += kpc)
                {
                    int a = Math.Min(kpc, kayitSayisi - i);
                    ElBâriÇerçeve.CerceveYaz(input.AsSpan(i * kanal, a * kanal), kanal, sira++, calisma, tampon);
                }
            }, MEASUREMENT_ITERATIONS / 10 + 1);

            int[] cikti = new int[kpc * kanal];
            int[] cozCalisma = new int[calisma.Length];

            decodeNanos = MeasureOperation(() =>
            {
                for (int i = 0; i < paketler.Length; i++)
                {
                    ElBâriÇerçeve.CerceveOku(paketler[i], kanal, cozCalisma, cikti, out _, out _);
                }
            }, MEASUREMENT_ITERATIONS / 10 + 1);

            // Round-trip: her çerçeveyi bağımsız çöz ve doğrula
            roundTripSuccess = true;
            for (int i = 0; i < paketler.Length && roundTripSuccess; i++)
            {
                if (!ElBâriÇerçeve.CerceveOku(paketler[i], kanal, cozCalisma, cikti, out uint s, out int adet))
                {
                    roundTripSuccess = false;
                    break;
                }
                int bas = (int)s * kpc * kanal;
                for (int j = 0; j < adet * kanal; j++)
                {
                    if (input[bas + j] != cikti[j]) { roundTripSuccess = false; break; }
                }
            }
        }
        else
        {
            // ---- KANAL AYRIMI (çerçevesiz) ----
            int[] calisma = new int[Math.Max(1, ElBâriKanal.GerekliCalismaAlani(kullanilanEleman, kanal))];
            byte[] cikti = new byte[ElBâriKanal.EnKotuDurumCiktiBoyutu(kullanilanEleman, kanal)];
            int[] geri = new int[kullanilanEleman];
            int[] cozCalisma = new int[calisma.Length];

            int boyut = 0;
            for (int w = 0; w < WARMUP_ITERATIONS / 10 + 1; w++)
            {
                boyut = ElBâriKanal.ElKâbıdKanal(input.AsSpan(0, kullanilanEleman), kanal, calisma, cikti);
                ElBâriKanal.ElBâsıtKanal(cikti.AsSpan(0, boyut), cozCalisma, geri);
            }
            compressedSize = boyut;

            encodeNanos = MeasureOperation(
                () => ElBâriKanal.ElKâbıdKanal(input.AsSpan(0, kullanilanEleman), kanal, calisma, cikti),
                MEASUREMENT_ITERATIONS / 10 + 1);

            int son = boyut;
            decodeNanos = MeasureOperation(
                () => ElBâriKanal.ElBâsıtKanal(cikti.AsSpan(0, son), cozCalisma, geri),
                MEASUREMENT_ITERATIONS / 10 + 1);

            roundTripSuccess = true;
            for (int i = 0; i < kullanilanEleman; i++)
            {
                if (input[i] != geri[i]) { roundTripSuccess = false; break; }
            }
        }

        if (!roundTripSuccess)
        {
            return new BenchmarkResult
            {
                ScenarioName = scenario.Name,
                Category = scenario.Category,
                InputSize = inputSize,
                InputBytes = inputBytes,
                CompressedBytes = compressedSize,
                CompressionRatio = 0,
                EncodeNanoseconds = encodeNanos,
                DecodeNanoseconds = decodeNanos,
                RoundTripSuccess = false,
                Status = "FAIL",
                ErrorMessage = "Round-trip validation failed - data corruption"
            };
        }

        return new BenchmarkResult
        {
            ScenarioName = scenario.Name,
            Category = scenario.Category,
            InputSize = kullanilanEleman,
            InputBytes = kullanilanEleman * sizeof(int),
            CompressedBytes = compressedSize,
            CompressionRatio = (double)(kullanilanEleman * sizeof(int)) / compressedSize,
            EncodeNanoseconds = encodeNanos,
            DecodeNanoseconds = decodeNanos,
            RoundTripSuccess = true,
            Status = "PASS"
        };
    }

    private static long MeasureOperation(Action operation, int iterations)
    {
        var sw = Stopwatch.StartNew();
        for (int i = 0; i < iterations; i++)
        {
            operation();
        }
        sw.Stop();

        return (long)((double)sw.ElapsedTicks / iterations * 1_000_000_000.0 / Stopwatch.Frequency);
    }

    private static bool ValidateRoundTrip(int[] original, int[] decoded)
    {
        if (original.Length != decoded.Length)
            return false;

        for (int i = 0; i < original.Length; i++)
        {
            if (original[i] != decoded[i])
                return false;
        }
        return true;
    }

    private static void PrintSummary(BenchmarkResult[] results, int passCount, int failCount, int rejectedCount)
    {
        Console.WriteLine("╔════════════════════════════════════════════════════════════════════════╗");
        Console.WriteLine("║                          BENCHMARK SUMMARY                             ║");
        Console.WriteLine("╚════════════════════════════════════════════════════════════════════════╝");
        Console.WriteLine();
        Console.WriteLine($"Total Tests:  {results.Length}");
        Console.ForegroundColor = ConsoleColor.Green;
        Console.WriteLine($"Passed:       {passCount}");
        Console.ResetColor();
        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.WriteLine($"Rejected:     {rejectedCount} (incompressible/meaningless data)");
        Console.ResetColor();
        Console.ForegroundColor = ConsoleColor.Red;
        Console.WriteLine($"Failed:       {failCount}");
        Console.ResetColor();
        Console.WriteLine($"Success Rate: {((passCount + rejectedCount) * 100.0 / results.Length):F1}%");
        Console.WriteLine();

        var passedResults = results.Where(r => r.Status == "PASS").ToArray();
        if (passedResults.Length == 0) return;

        double avgCompressionRatio = passedResults.Average(r => r.CompressionRatio);
        long avgEncodeNs = (long)passedResults.Average(r => r.EncodeNanoseconds);
        long avgDecodeNs = (long)passedResults.Average(r => r.DecodeNanoseconds);

        Console.WriteLine($"Average Compression Ratio: {avgCompressionRatio:F2}x");
        Console.WriteLine($"Average Encode Time:       {avgEncodeNs:N0} ns");
        Console.WriteLine($"Average Decode Time:       {avgDecodeNs:N0} ns");
        Console.WriteLine();
    }

    private static void PrintCategoryBreakdown(BenchmarkResult[] results)
    {
        Console.WriteLine("╔════════════════════════════════════════════════════════════════════════╗");
        Console.WriteLine("║                        CATEGORY BREAKDOWN                              ║");
        Console.WriteLine("╚════════════════════════════════════════════════════════════════════════╝");
        Console.WriteLine();

        var categories = results.GroupBy(r => r.Category)
                                .OrderBy(g => g.Key);

        foreach (var group in categories)
        {
            var passed = group.Where(r => r.Status == "PASS").ToArray();
            Console.WriteLine($"{group.Key}:");
            Console.WriteLine($"  Tests: {group.Count()}, Passed: {passed.Length}");

            if (passed.Length > 0)
            {
                double avgRatio = passed.Average(r => r.CompressionRatio);
                Console.WriteLine($"  Avg Compression: {avgRatio:F2}x");
            }
            Console.WriteLine();
        }
    }

    private static void PrintTopPerformers(BenchmarkResult[] results)
    {
        var passedResults = results.Where(r => r.Status == "PASS").ToArray();
        if (passedResults.Length == 0) return;

        Console.WriteLine("╔════════════════════════════════════════════════════════════════════════╗");
        Console.WriteLine("║                          TOP PERFORMERS                                ║");
        Console.WriteLine("╚════════════════════════════════════════════════════════════════════════╝");
        Console.WriteLine();

        // Best compression
        var bestCompression = passedResults.OrderByDescending(r => r.CompressionRatio).Take(3);
        Console.WriteLine("🏆 Best Compression Ratio:");
        foreach (var r in bestCompression)
        {
            Console.WriteLine($"   {r.ScenarioName}: {r.CompressionRatio:F2}x ({r.InputBytes} → {r.CompressedBytes} bytes)");
        }
        Console.WriteLine();

        // Fastest encode
        var fastestEncode = passedResults.Where(r => r.InputSize > 100).OrderBy(r => r.EncodeNanoseconds).Take(3);
        Console.WriteLine("⚡ Fastest Encode:");
        foreach (var r in fastestEncode)
        {
            double itemsPerSec = r.InputSize / (r.EncodeNanoseconds / 1e9);
            Console.WriteLine($"   {r.ScenarioName}: {r.EncodeNanoseconds:N0} ns ({itemsPerSec:F0} items/sec)");
        }
        Console.WriteLine();

        // Fastest decode
        var fastestDecode = passedResults.Where(r => r.InputSize > 100).OrderBy(r => r.DecodeNanoseconds).Take(3);
        Console.WriteLine("⚡ Fastest Decode:");
        foreach (var r in fastestDecode)
        {
            double itemsPerSec = r.InputSize / (r.DecodeNanoseconds / 1e9);
            Console.WriteLine($"   {r.ScenarioName}: {r.DecodeNanoseconds:N0} ns ({itemsPerSec:F0} items/sec)");
        }
        Console.WriteLine();
    }

    /// <summary>
    /// Kategoriye göre benchmark çalıştır.
    /// </summary>
    public static void RunCategory(string category)
    {
        var scenarios = TestScenarios.GetByCategory(category);
        Console.WriteLine($"Running {scenarios.Length} scenarios in category: {category}");
        Console.WriteLine();

        foreach (var scenario in scenarios)
        {
            var result = RunScenario(scenario);
            Console.WriteLine($"{scenario.Name}: {result.Status}");
            if (result.Status == "PASS")
            {
                Console.WriteLine($"  Compression: {result.CompressionRatio:F2}x, Encode: {result.EncodeNanoseconds:N0} ns");
            }
        }
    }
}
