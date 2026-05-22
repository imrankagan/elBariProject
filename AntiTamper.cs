// ElBâri Compression Engine - Runtime Protection Layer
// Copyright (c) 2025. All rights reserved.
// Proprietary and Confidential - Unauthorized access prohibited.

using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Security.Cryptography;

namespace ElBâri.Protection;

/// <summary>
/// Çalışma zamanı anti-tamper ve anti-debug koruma katmanı.
/// Tersine mühendislik girişimlerini algılar ve engeller.
/// </summary>
internal static class AntiTamper
{
    private static readonly byte[] s_assemblyHash;
    private static bool s_initialized;

    static AntiTamper()
    {
        // Assembly'nin hash'ini hesapla (integrity check için)
        s_assemblyHash = ComputeAssemblyHash();
        s_initialized = false;
    }

    /// <summary>
    /// Koruma katmanını başlat. Her public metod çağrısında kontrol edilmeli.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void Initialize()
    {
        if (s_initialized) return;

        // 1. Debugger kontrolü
        if (Debugger.IsAttached)
        {
            Environment.FailFast("Unauthorized debugging attempt detected.");
        }

        // 2. Assembly integrity kontrolü
        if (!VerifyIntegrity())
        {
            Environment.FailFast("Assembly integrity violation detected.");
        }

        // 3. Çalışma ortamı kontrolü
        if (IsRunningInSuspiciousEnvironment())
        {
            Environment.FailFast("Suspicious runtime environment detected.");
        }

        s_initialized = true;
    }

    /// <summary>
    /// Hızlı inline debugger kontrolü - her kritik metodun başında çağrılabilir.
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    internal static void CheckDebugger()
    {
        if (Debugger.IsAttached)
        {
            Environment.FailFast("Runtime debugging detected.");
        }
    }

    /// <summary>
    /// Assembly'nin değiştirilip değiştirilmediğini kontrol et.
    /// </summary>
    private static bool VerifyIntegrity()
    {
        try
        {
            byte[] currentHash = ComputeAssemblyHash();

            if (currentHash.Length != s_assemblyHash.Length)
                return false;

            for (int i = 0; i < currentHash.Length; i++)
            {
                if (currentHash[i] != s_assemblyHash[i])
                    return false;
            }

            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Assembly'nin SHA256 hash'ini hesapla.
    /// </summary>
    private static byte[] ComputeAssemblyHash()
    {
        try
        {
            Assembly asm = typeof(AntiTamper).Assembly;
            string? location = asm.Location;

            if (string.IsNullOrEmpty(location))
            {
                // AOT senaryosu: module name kullan
                return SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(asm.FullName ?? "ElBâri"));
            }

            // Dosya bazlı hash
            using var stream = System.IO.File.OpenRead(location);
            return SHA256.HashData(stream);
        }
        catch
        {
            // Hash hesaplanamadıysa, fallback
            return SHA256.HashData(System.Text.Encoding.UTF8.GetBytes("ElBâri.Protection"));
        }
    }

    /// <summary>
    /// Şüpheli çalışma ortamı kontrolü (VM, sandbox, reverse engineering araçları).
    /// </summary>
    private static bool IsRunningInSuspiciousEnvironment()
    {
        try
        {
            // 1. İşlemci sayısı kontrolü (bazı VM'ler tek çekirdek verir)
            if (Environment.ProcessorCount < 2)
            {
                // Tek çekirdekli sistemler şüpheli (modern donanımda nadir)
                // Ancak embedded sistemler için false-positive olabilir
            }

            // 2. Timing anomalisi kontrolü (debugger yavaşlatır)
            var sw = Stopwatch.StartNew();
            System.Threading.Thread.Sleep(10);
            sw.Stop();

            // Normal durumda ~10-15ms olmalı, 50ms+ şüpheli
            if (sw.ElapsedMilliseconds > 50)
            {
                return true;
            }

            // 3. İşlem adı kontrolü (dnSpy, ILSpy, dotPeek gibi araçlar)
            string processName = Process.GetCurrentProcess().ProcessName.ToLowerInvariant();
            string[] suspiciousNames = { "dnspy", "ilspy", "dotpeek", "reflexil", "megadumper", "x64dbg", "windbg" };

            foreach (string suspicious in suspiciousNames)
            {
                if (processName.Contains(suspicious))
                {
                    return true;
                }
            }

            return false;
        }
        catch
        {
            // Kontrol başarısız olursa, güvenli tarafta kal
            return false;
        }
    }

    /// <summary>
    /// Rastgele timing jitter ekle (timing attack'lere karşı).
    /// </summary>
    [MethodImpl(MethodImplOptions.NoInlining)]
    internal static void AddTimingJitter()
    {
        // Mikro-saniye seviyesinde rastgele gecikme
        int jitter = RandomNumberGenerator.GetInt32(1, 100);
        for (int i = 0; i < jitter; i++)
        {
            // Boş döngü - compiler optimize etmesin diye volatile
            System.Threading.Thread.SpinWait(10);
        }
    }
}
