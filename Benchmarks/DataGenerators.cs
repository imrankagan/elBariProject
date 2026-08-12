// ElBâri Compression Engine - Benchmark Data Generators
// Copyright (c) 2025. All rights reserved.
// Proprietary and Confidential - Unauthorized access prohibited.

using System;

namespace ElBâri.Benchmarks;

/// <summary>
/// Çeşitli test senaryoları için veri üretici.
/// </summary>
public static class DataGenerators
{
    private static readonly Random s_random = new Random(42); // Deterministic seed

    /// <summary>
    /// Sıralı artış deseni: [0, 1, 2, 3, 4, ...]
    /// İdeal sıkıştırma senaryosu (delta = 1).
    /// </summary>
    public static int[] Sequential(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = i;
        }
        return data;
    }

    /// <summary>
    /// Tamamen rastgele değerler [-1000, 1000] aralığında.
    /// Zorlu sıkıştırma senaryosu (büyük deltalar).
    /// </summary>
    public static int[] Random(int count, int min = -1000, int max = 1000)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = s_random.Next(min, max);
        }
        return data;
    }

    /// <summary>
    /// Seyrek veri: çoğunlukla sıfır, arada büyük spike'lar.
    /// Outlier handling test senaryosu.
    /// </summary>
    public static int[] Sparse(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            // %95 sıfır, %5 spike
            data[i] = s_random.NextDouble() < 0.95 ? 0 : s_random.Next(10000, 50000);
        }
        return data;
    }

    /// <summary>
    /// Yoğun veri: küçük değerler etrafında yoğunlaşmış.
    /// İyi sıkıştırma senaryosu (küçük bit-width).
    /// </summary>
    public static int[] Dense(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = s_random.Next(-10, 10); // [-10, 10] aralığında
        }
        return data;
    }

    /// <summary>
    /// Sinüs dalga deseni (İHA/sensör verisi simülasyonu).
    /// Periyodik veri, orta sıkıştırma kalitesi.
    /// </summary>
    public static int[] SineWave(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = (int)(100 * Math.Sin(i * 0.1));
        }
        return data;
    }

    /// <summary>
    /// Sabit değer: tüm elemanlar aynı.
    /// En iyi sıkıştırma senaryosu (delta = 0).
    /// </summary>
    public static int[] Constant(int count, int value = 42)
    {
        int[] data = new int[count];
        Array.Fill(data, value);
        return data;
    }

    /// <summary>
    /// En kötü senaryo: her eleman maksimum farklı.
    /// Sıkıştırma neredeyse imkansız (32-bit gerekir).
    /// </summary>
    public static int[] WorstCase(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = int.MaxValue - i * 123456789; // Büyük ve düzensiz atlama
        }
        return data;
    }

    /// <summary>
    /// Monoton artan ama değişken adımlar (gerçek telemetri verisi).
    /// </summary>
    public static int[] MonotonicVariable(int count)
    {
        int[] data = new int[count];
        int current = 0;
        for (int i = 0; i < count; i++)
        {
            current += s_random.Next(1, 100); // 1-100 arası adımlar
            data[i] = current;
        }
        return data;
    }

    /// <summary>
    /// Gaussian (normal) dağılım: ortalama=0, standart sapma=100.
    /// Gerçek dünya verisi simülasyonu.
    /// </summary>
    public static int[] Gaussian(int count, double mean = 0, double stdDev = 100)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            // Box-Muller transform
            double u1 = s_random.NextDouble();
            double u2 = s_random.NextDouble();
            double z = Math.Sqrt(-2.0 * Math.Log(u1)) * Math.Cos(2.0 * Math.PI * u2);
            data[i] = (int)(mean + stdDev * z);
        }
        return data;
    }

    /// <summary>
    /// Tekrar eden desen: [1,2,3,1,2,3,1,2,3,...]
    /// Pattern recognition test senaryosu.
    /// </summary>
    public static int[] RepeatingPattern(int count)
    {
        int[] pattern = { 1, 2, 3, 5, 8, 13 }; // Fibonacci
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = pattern[i % pattern.Length];
        }
        return data;
    }

    /// <summary>
    /// Tüm pozitif değerler (0 ile max int arası).
    /// </summary>
    public static int[] AllPositive(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = s_random.Next(0, int.MaxValue / 2);
        }
        return data;
    }

    /// <summary>
    /// Tüm negatif değerler (min int ile 0 arası).
    /// </summary>
    public static int[] AllNegative(int count)
    {
        int[] data = new int[count];
        for (int i = 0; i < count; i++)
        {
            data[i] = s_random.Next(int.MinValue / 2, 0);
        }
        return data;
    }

    /// <summary>
    /// Edge case: Tüm elemanlar sıfır.
    /// </summary>
    public static int[] AllZeros(int count)
    {
        return new int[count]; // Default = 0
    }

    /// <summary>
    /// Edge case: Tek eleman.
    /// </summary>
    public static int[] SingleElement(int value = 123)
    {
        return new int[] { value };
    }

    /// <summary>
    /// Edge case: Boş array.
    /// </summary>
    public static int[] Empty()
    {
        return Array.Empty<int>();
    }

    /// <summary>
    /// Edge case: Minimum boyut (8 eleman - ElBâri'nin SIMD width'i).
    /// </summary>
    public static int[] MinimalSIMD()
    {
        return Sequential(8);
    }

    // =================================================================
    // GERÇEK VERİ
    // =================================================================

    /// <summary>
    /// Gerçek GPS telemetrisi (TestData/gercek_gps.bin).
    /// 3 kanal: enlem, boylam, unix zaman damgası — iç içe.
    /// Dosya bulunamazsa GercekciUAVTelemetry ile sentetik yedek döner.
    /// Kaynak ve lisans bilgisi için TestData/KAYNAK.md dosyasına bakın.
    /// </summary>
    public static int[] GercekGpsVerisi(int enFazlaKayit = int.MaxValue)
    {
        string? yol = TestVeriDosyasiBul("gercek_gps.bin");
        if (yol == null)
        {
            // Yedek: dosya yoksa gerçekçi sentetik üret (3 kanal)
            return GercekciUAVTelemetry(Math.Min(enFazlaKayit, 8000) * 3, 3);
        }

        byte[] bayt = System.IO.File.ReadAllBytes(yol);
        int kanal = BitConverter.ToInt32(bayt, 0);
        int adet = BitConverter.ToInt32(bayt, 4);

        int kayitSayisi = adet / kanal;
        if (enFazlaKayit < kayitSayisi) kayitSayisi = enFazlaKayit;
        int alinacak = kayitSayisi * kanal;

        int[] veri = new int[alinacak];
        Buffer.BlockCopy(bayt, 8, veri, 0, alinacak * sizeof(int));
        return veri;
    }

    /// <summary>Gerçek GPS veri dosyasının kanal sayısı (dosya yoksa 3).</summary>
    public static int GercekGpsKanalSayisi()
    {
        string? yol = TestVeriDosyasiBul("gercek_gps.bin");
        if (yol == null) return 3;
        byte[] bas = new byte[4];
        using var akis = System.IO.File.OpenRead(yol);
        return akis.Read(bas, 0, 4) == 4 ? BitConverter.ToInt32(bas, 0) : 3;
    }

    /// <summary>Gerçek veri dosyası mevcut mu?</summary>
    public static bool GercekVeriMevcutMu() => TestVeriDosyasiBul("gercek_gps.bin") != null;

    /// <summary>
    /// Çalıştırma dizininden yukarı doğru arayarak TestData klasörünü bulur.
    /// (bin/Debug/net10.0 gibi derin çıktı dizinlerinden çalışmayı destekler.)
    /// </summary>
    private static string? TestVeriDosyasiBul(string dosyaAdi)
    {
        string dizin = AppContext.BaseDirectory;
        for (int i = 0; i < 8 && !string.IsNullOrEmpty(dizin); i++)
        {
            string aday = System.IO.Path.Combine(dizin, "TestData", dosyaAdi);
            if (System.IO.File.Exists(aday)) return aday;

            System.IO.DirectoryInfo? ust = System.IO.Directory.GetParent(dizin);
            if (ust == null) break;
            dizin = ust.FullName;
        }
        return null;
    }

    /// <summary>
    /// GERÇEKÇİ İHA telemetrisi: sabit hızla ilerleyen bir hava aracı.
    /// Kanallar (6): lat, lon, alt(cm), roll, pitch, yaw (milirad).
    ///
    /// NOT: Eski UAVTelemetry üreteci enlem/boylamı rastgele gürültü olarak
    /// üretiyordu; gerçek bir uçuşta pozisyon DÜZGÜN ilerler ve bu, ikinci
    /// derece fark kodlamasının çok daha iyi çalışmasını sağlar. Bu üreteç
    /// gerçek uçuş dinamiğini taklit eder.
    /// </summary>
    public static int[] GercekciUAVTelemetry(int count, int kanal = 6)
    {
        Random r = new Random(7);
        int[] data = new int[count];
        long lat = 400000000;   // 1e-7 derece
        long lon = 290000000;
        int alt = 12000;        // cm
        int adet = count / kanal;

        for (int i = 0; i < adet; i++)
        {
            // ~15 m/s sabit hız + küçük GPS gürültüsü
            lat += 135 + r.Next(-3, 4);
            lon += 98 + r.Next(-3, 4);
            alt += r.Next(-8, 9);

            int b = i * kanal;
            data[b] = (int)lat;
            if (kanal > 1) data[b + 1] = (int)lon;
            if (kanal > 2) data[b + 2] = alt;
            if (kanal > 3) data[b + 3] = (int)(200 * Math.Sin(i * 0.02)) + r.Next(-2, 3);
            if (kanal > 4) data[b + 4] = (int)(150 * Math.Sin(i * 0.013)) + r.Next(-2, 3);
            if (kanal > 5) data[b + 5] = (int)(1500 + 300 * Math.Sin(i * 0.005)) + r.Next(-2, 3);
        }
        return data;
    }

    /// <summary>
    /// İHA telemetri simülasyonu: GPS koordinatları + altitude.
    /// </summary>
    public static int[] UAVTelemetry(int count)
    {
        int[] data = new int[count];
        int altitude = 10000; // 100m başlangıç

        for (int i = 0; i < count; i += 3)
        {
            // Latitude (scaled int)
            if (i < count) data[i] = 4000000 + s_random.Next(-100, 100);
            // Longitude (scaled int)
            if (i + 1 < count) data[i + 1] = 2900000 + s_random.Next(-100, 100);
            // Altitude (cm)
            if (i + 2 < count)
            {
                altitude += s_random.Next(-50, 50);
                data[i + 2] = Math.Max(0, altitude);
            }
        }
        return data;
    }
}
