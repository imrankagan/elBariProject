// ElBâri Compression Engine - Test Scenarios
// Copyright (c) 2025. All rights reserved.
// Proprietary and Confidential - Unauthorized access prohibited.

using System;

namespace ElBâri.Benchmarks;

/// <summary>
/// Test senaryosu tanımı.
/// </summary>
public record TestScenario
{
    public required string Name { get; init; }
    public required string Category { get; init; }
    public required int[] InputData { get; init; }
    public required string Description { get; init; }
    public string? ExpectedOutcome { get; init; }

    /// <summary>
    /// Kayıt başına alan sayısı. 1 = tek akış (klasik ElKâbıd/ElBâsıt yolu).
    /// &gt;1 ise ElBâriKanal katmanı kullanılır.
    /// </summary>
    public int KanalSayisi { get; init; } = 1;

    /// <summary>
    /// Çerçeve başına kayıt sayısı. 0 = çerçeveleme yok (tek blok).
    /// &gt;0 ise ElBâriÇerçeve katmanı kullanılır (paket kaybına dayanıklı).
    /// </summary>
    public int CerceveBoyutu { get; init; } = 0;
}

/// <summary>
/// Tüm test senaryolarını yöneten sınıf.
/// </summary>
public static class TestScenarios
{
    /// <summary>
    /// Tüm test senaryolarını döndürür.
    /// </summary>
    public static TestScenario[] GetAllScenarios()
    {
        return new[]
        {
            // ===== CORRECTNESS TESTS =====
            new TestScenario
            {
                Name = "Empty Array",
                Category = "Correctness",
                InputData = DataGenerators.Empty(),
                Description = "Boş array - edge case kontrolü",
                ExpectedOutcome = "Encode/decode başarılı veya uygun hata"
            },
            new TestScenario
            {
                Name = "Single Element",
                Category = "Correctness",
                InputData = DataGenerators.SingleElement(999),
                Description = "Tek elemanlı array",
                ExpectedOutcome = "Perfect round-trip"
            },
            new TestScenario
            {
                Name = "Minimal SIMD (8 elements)",
                Category = "Correctness",
                InputData = DataGenerators.MinimalSIMD(),
                Description = "Minimum SIMD boyutu (AVX2/NEON vectorization)",
                ExpectedOutcome = "Perfect round-trip"
            },
            new TestScenario
            {
                Name = "All Zeros",
                Category = "Correctness",
                InputData = DataGenerators.AllZeros(100),
                Description = "Tüm elemanlar sıfır - en iyi sıkıştırma",
                ExpectedOutcome = "Compression ratio > 10:1"
            },

            // ===== COMPRESSION QUALITY TESTS =====
            new TestScenario
            {
                Name = "Sequential Small (100)",
                Category = "Compression Quality",
                InputData = DataGenerators.Sequential(100),
                Description = "Sıralı artış - ideal sıkıştırma",
                ExpectedOutcome = "Very high compression ratio"
            },
            new TestScenario
            {
                Name = "Sequential Medium (10K)",
                Category = "Compression Quality",
                InputData = DataGenerators.Sequential(10_000),
                Description = "Orta boyut sıralı veri",
                ExpectedOutcome = "High compression ratio"
            },
            new TestScenario
            {
                Name = "Sequential Large (1M)",
                Category = "Compression Quality",
                InputData = DataGenerators.Sequential(1_000_000),
                Description = "Büyük boyut sıralı veri",
                ExpectedOutcome = "High compression ratio"
            },
            new TestScenario
            {
                Name = "Constant Value (1K)",
                Category = "Compression Quality",
                InputData = DataGenerators.Constant(1000, 42),
                Description = "Tüm elemanlar aynı - maksimum sıkıştırma",
                ExpectedOutcome = "Compression ratio > 20:1"
            },
            new TestScenario
            {
                Name = "Dense Small Range (10K)",
                Category = "Compression Quality",
                InputData = DataGenerators.Dense(10_000),
                Description = "Küçük değer aralığı [-10, 10]",
                ExpectedOutcome = "Good compression ratio"
            },

            // ===== PERFORMANCE TESTS =====
            new TestScenario
            {
                Name = "Random Medium (10K)",
                Category = "Performance",
                InputData = DataGenerators.Random(10_000),
                Description = "Rastgele değerler - orta zorluk",
                ExpectedOutcome = "Moderate compression ratio"
            },
            new TestScenario
            {
                Name = "Random Large (100K)",
                Category = "Performance",
                InputData = DataGenerators.Random(100_000),
                Description = "Büyük rastgele veri - throughput testi",
                ExpectedOutcome = "Throughput > 1M items/sec"
            },
            new TestScenario
            {
                Name = "Sparse with Outliers (10K)",
                Category = "Performance",
                InputData = DataGenerators.Sparse(10_000),
                Description = "Seyrek veri + spike'lar - outlier handling",
                ExpectedOutcome = "Outlier detection works"
            },

            // ===== REAL-WORLD SIMULATION =====
            new TestScenario
            {
                Name = "Sine Wave (1K)",
                Category = "Real-World",
                InputData = DataGenerators.SineWave(1000),
                Description = "Periyodik sinyal - sensör verisi",
                ExpectedOutcome = "Good compression for periodic data"
            },
            new TestScenario
            {
                Name = "Gaussian Distribution (10K)",
                Category = "Real-World",
                InputData = DataGenerators.Gaussian(10_000, mean: 0, stdDev: 100),
                Description = "Normal dağılım - istatistiksel veri",
                ExpectedOutcome = "Moderate compression"
            },
            new TestScenario
            {
                Name = "Monotonic Variable Steps (10K)",
                Category = "Real-World",
                InputData = DataGenerators.MonotonicVariable(10_000),
                Description = "Değişken adımlarla artan - timestamp/counter",
                ExpectedOutcome = "Good compression ratio"
            },
            new TestScenario
            {
                Name = "UAV Telemetry (10K)",
                Category = "Real-World",
                InputData = DataGenerators.UAVTelemetry(10_000),
                Description = "İHA telemetri verisi (GPS + altitude)",
                ExpectedOutcome = "Realistic compression for IoT data"
            },
            new TestScenario
            {
                Name = "Repeating Pattern (1K)",
                Category = "Real-World",
                InputData = DataGenerators.RepeatingPattern(1000),
                Description = "Tekrar eden desen - pattern recognition",
                ExpectedOutcome = "High compression for patterns"
            },

            // ===== STRESS TESTS =====
            new TestScenario
            {
                Name = "All Positive (10K)",
                Category = "Stress",
                InputData = DataGenerators.AllPositive(10_000),
                Description = "Sadece pozitif değerler",
                ExpectedOutcome = "No negative handling overhead"
            },
            new TestScenario
            {
                Name = "All Negative (10K)",
                Category = "Stress",
                InputData = DataGenerators.AllNegative(10_000),
                Description = "Sadece negatif değerler",
                ExpectedOutcome = "Correct negative number handling"
            },
            new TestScenario
            {
                Name = "Worst Case (1K)",
                Category = "Stress",
                InputData = DataGenerators.WorstCase(1000),
                Description = "En kötü senaryo - sıkıştırma imkansız",
                ExpectedOutcome = "Compression ratio close to 1:1 (no compression)"
            },
            new TestScenario
            {
                Name = "Huge Sequential (1M)",
                Category = "Stress",
                InputData = DataGenerators.Sequential(1_000_000),
                Description = "Çok büyük veri seti - memory ve performans",
                ExpectedOutcome = "Handles large data without crash"
            },

            // ===== EDGE CASES =====
            new TestScenario
            {
                Name = "Max Int Values",
                Category = "Edge Cases",
                InputData = new int[] { int.MaxValue, int.MaxValue - 1, int.MaxValue - 2 },
                Description = "Maksimum integer değerleri",
                ExpectedOutcome = "No overflow in delta calculation"
            },
            new TestScenario
            {
                Name = "Min Int Values",
                Category = "Edge Cases",
                InputData = new int[] { int.MinValue, int.MinValue + 1, int.MinValue + 2 },
                Description = "Minimum integer değerleri",
                ExpectedOutcome = "No underflow in delta calculation"
            },
            new TestScenario
            {
                Name = "Mixed Extremes",
                Category = "Edge Cases",
                InputData = new int[] { int.MinValue, 0, int.MaxValue, -1, 1 },
                Description = "Karışık ekstrem değerler",
                ExpectedOutcome = "Handles full int32 range"
            },
            new TestScenario
            {
                Name = "Zigzag Pattern",
                Category = "Edge Cases",
                InputData = GenerateZigzag(1000),
                Description = "Sürekli yön değiştiren delta'lar",
                ExpectedOutcome = "Correct delta sign handling"
            },

            // ===== GERÇEK VERİ (OpenStreetMap GPS izleri) =====
            // Kaynak/lisans: testverisi/KAYNAK.md
            new TestScenario
            {
                Name = "GERÇEK GPS - tek akış (kanal ayrımsız)",
                Category = "Real Data",
                InputData = DataGenerators.GercekGpsVerisi(),
                Description = "Gerçek GPS telemetrisi, kanal ayrımı YAPILMADAN (mevcut davranış)",
                ExpectedOutcome = "REJECTED bekleniyor - kanallar iç içe olduğu için sıkıştırılamaz"
            },
            new TestScenario
            {
                Name = "GERÇEK GPS - kanal ayrımı",
                Category = "Real Data",
                InputData = DataGenerators.GercekGpsVerisi(),
                KanalSayisi = 3,
                Description = "Aynı gerçek veri, ElBâriKanal ile kanal kanal sıkıştırılmış",
                ExpectedOutcome = "Kanal ayrımı sayesinde sıkışır"
            },
            new TestScenario
            {
                Name = "GERÇEK GPS - çerçeveli (100 kayıt)",
                Category = "Real Data",
                InputData = DataGenerators.GercekGpsVerisi(),
                KanalSayisi = 3,
                CerceveBoyutu = 100,
                Description = "Paket kaybına dayanıklı çerçeveleme + CRC32 doğrulama",
                ExpectedOutcome = "Dayanıklılık karşılığında bir miktar oran kaybı"
            },
            new TestScenario
            {
                Name = "GERÇEK GPS - çerçeveli (500 kayıt)",
                Category = "Real Data",
                InputData = DataGenerators.GercekGpsVerisi(),
                KanalSayisi = 3,
                CerceveBoyutu = 500,
                Description = "Daha büyük çerçeve: daha iyi oran, daha kaba kayıp granülaritesi",
                ExpectedOutcome = "100 kayıtlık çerçeveden daha iyi oran"
            },

            // ===== ÇOK KANALLI İHA TELEMETRİSİ =====
            new TestScenario
            {
                Name = "İHA telemetri 6 kanal - kanal ayrımsız",
                Category = "Multi-Channel",
                InputData = DataGenerators.GercekciUAVTelemetry(9996),
                Description = "Gerçekçi uçuş verisi, kanal ayrımı yapılmadan",
                ExpectedOutcome = "REJECTED bekleniyor"
            },
            new TestScenario
            {
                Name = "İHA telemetri 6 kanal - kanal ayrımı",
                Category = "Multi-Channel",
                InputData = DataGenerators.GercekciUAVTelemetry(9996),
                KanalSayisi = 6,
                Description = "Aynı veri, ElBâriKanal ile (lat/lon 2. derece farkı seçer)",
                ExpectedOutcome = "Yüksek sıkıştırma oranı"
            },
            new TestScenario
            {
                Name = "İHA telemetri 6 kanal - çerçeveli",
                Category = "Multi-Channel",
                InputData = DataGenerators.GercekciUAVTelemetry(9996),
                KanalSayisi = 6,
                CerceveBoyutu = 250,
                Description = "Telsiz linki senaryosu: bağımsız çözülebilir çerçeveler",
                ExpectedOutcome = "Paket kaybına dayanıklı"
            },
        };
    }

    /// <summary>
    /// Kategoriye göre senaryoları filtrele.
    /// </summary>
    public static TestScenario[] GetByCategory(string category)
    {
        var all = GetAllScenarios();
        return Array.FindAll(all, s => s.Category.Equals(category, StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>
    /// Senaryo ismine göre bul.
    /// </summary>
    public static TestScenario? FindByName(string name)
    {
        var all = GetAllScenarios();
        return Array.Find(all, s => s.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
    }

    // Helper: Zigzag pattern generator
    private static int[] GenerateZigzag(int count)
    {
        int[] data = new int[count];
        int value = 0;
        int delta = 100;

        for (int i = 0; i < count; i++)
        {
            data[i] = value;
            value += delta;
            delta = -delta; // Yön değiştir
        }
        return data;
    }
}
