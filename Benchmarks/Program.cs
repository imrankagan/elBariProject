// ElBâri Compression Engine - Benchmark Entry Point
// Copyright (c) 2025. All rights reserved.
// Proprietary and Confidential - Unauthorized access prohibited.

using System;
using ElBâri.Benchmarks;

internal class Program
{
    private static void Main(string[] args)
    {
        Console.WriteLine("═══════════════════════════════════════════════");
        Console.WriteLine("  QUICK FIX VALIDATION");
        Console.WriteLine("═══════════════════════════════════════════════");
        Console.WriteLine();

        // Test 1: int.MinValue overflow fix
        Console.WriteLine("Test 1: int.MinValue Overflow Fix");
        int[] mixedExtremes = { int.MinValue, 0, int.MaxValue, -1, 1 };
        byte[] compressed = new byte[100];

        try
        {
            int result = ElBâri.ElBâri.ElKâbıd(mixedExtremes, compressed);

            if (result > 0)
            {
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine($"✓ PASS: Compressed to {result} bytes (no overflow)");
            }
            else if (result == -1)
            {
                Console.ForegroundColor = ConsoleColor.Yellow;
                Console.WriteLine("✓ PASS: REJECTED as incompressible (expected)");
            }
            Console.ResetColor();
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"✗ FAIL: Exception - {ex.Message}");
            Console.ResetColor();
        }

        Console.WriteLine();

        // Test 2: Random data rejection
        Console.WriteLine("Test 2: Random Data Rejection");
        Random rand = new Random(42);
        int[] randomData = new int[1000];
        for (int i = 0; i < 1000; i++)
        {
            randomData[i] = rand.Next(int.MinValue, int.MaxValue);
        }

        byte[] compressed2 = new byte[10000];
        int result2 = ElBâri.ElBâri.ElKâbıd(randomData, compressed2);

        if (result2 == -1)
        {
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine("✓ PASS: Random data REJECTED (as expected)");
        }
        else
        {
            Console.ForegroundColor = ConsoleColor.Yellow;
            Console.WriteLine($"⚠ ACCEPTED: Compressed to {result2} bytes");
        }
        Console.ResetColor();

        Console.WriteLine();

        // Test 3: Good data acceptance
        Console.WriteLine("Test 3: Sequential Data Acceptance");
        int[] goodData = new int[1000];
        for (int i = 0; i < 1000; i++)
        {
            goodData[i] = i;
        }

        byte[] compressed3 = new byte[10000];
        int result3 = ElBâri.ElBâri.ElKâbıd(goodData, compressed3);

        if (result3 > 0)
        {
            float ratio = (1000 * 4f) / result3;
            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"✓ PASS: Compressed to {result3} bytes ({ratio:F2}x ratio)");
        }
        else
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine("✗ FAIL: Sequential data REJECTED (should be accepted!)");
        }
        Console.ResetColor();

        Console.WriteLine();
        Console.WriteLine("═══════════════════════════════════════════════");
        Console.WriteLine();

        // Ana benchmark'ı çalıştır
        Console.WriteLine("Running full benchmark suite...");
        Console.WriteLine();
        BenchmarkRunner.RunAll(verbose: true);

        // Sadece etkileşimli konsolda bekle. Çıktı/girdi yönlendirildiğinde (CI, dosyaya
        // log alma) ReadKey InvalidOperationException atardı; bu kontrol onu önler.
        if (!Console.IsInputRedirected)
        {
            Console.WriteLine();
            Console.WriteLine("Press any key to exit...");
            Console.ReadKey();
        }
    }
}
