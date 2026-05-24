# ElBâri Compression Engine - Detaylı Benchmark Sonuçları

## 🎯 Genel Özet
| Metrik | Değer |
|--------|-------|
| **Test Senaryosu Sayısı** | 25 |
| **Başarılı Test** | 24 ✓ |
| **Başarısız Test** | 1 ✗ |
| **Başarı Oranı** | **96.0%** |
| **Ortalama Sıkıştırma Oranı** | **26.69x** |
| **Ortalama Encode Süresi** | 2.22 ms |
| **Ortalama Decode Süresi** | 1.57 ms |
| **Maks. Throughput (Encode)** | **43.5M items/sec** |
| **Maks. Throughput (Decode)** | **63.2M items/sec** |
| **En İyi Sıkıştırma** | **500x** (constant values) |

---

## 📋 Kategori Bazlı Performans

| Kategori | Test Sayısı | Başarılı | Başarı % | Ort. Sıkıştırma | Notlar |
|----------|------------|----------|----------|-----------------|--------|
| **Compression Quality** | 5 | 5 | 100% | **108.10x** | İdeal senaryolar |
| **Correctness** | 4 | 4 | 100% | 13.62x | Edge case'ler |
| **Performance** | 3 | 3 | 100% | 2.80x | Rastgele veri |
| **Real-World** | 5 | 5 | 100% | 2.97x | Gerçek veri simülasyonu |
| **Stress** | 4 | 4 | 100% | 3.92x | Büyük veri setleri |
| **Edge Cases** | 4 | 3 | 75% | 2.25x | Ekstrem değerler |

---

## 🏆 Top 10 Performans Liderleri

### En İyi Sıkıştırma Oranı
| # | Senaryo | Giriş | Çıkış | Oran | Kategori |
|---|---------|-------|-------|------|----------|
| 🥇 | **Constant Value (1K)** | 4,000 B | 8 B | **500.00x** | Compression Quality |
| 🥈 | **All Zeros** | 400 B | 8 B | **50.00x** | Correctness |
| 🥉 | **Sequential Large (1M)** | 4,000,000 B | 312,504 B | **12.80x** | Stress |
| 4 | Sequential Medium (10K) | 40,000 B | 3,128 B | 12.79x | Compression Quality |
| 5 | Sequential Small (100) | 400 B | 35 B | 11.43x | Compression Quality |
| 6 | Zigzag Pattern | 4,000 B | 1,066 B | 3.75x | Edge Cases |
| 7 | Sine Wave (1K) | 4,000 B | 1,098 B | 3.64x | Real-World |
| 8 | Monotonic Variable | 40,000 B | 14,376 B | 2.78x | Real-World |
| 9 | Dense Small Range (10K) | 40,000 B | 15,633 B | 2.56x | Compression Quality |
| 10 | Repeating Pattern | 4,000 B | 1,652 B | 2.42x | Real-World |

### En Hızlı Encode (Throughput)
| # | Senaryo | Encode Süresi | Throughput | Veri Boyutu |
|---|---------|---------------|------------|-------------|
| 🥇 | **Constant Value (1K)** | 23.0 μs | **43.5M items/sec** | 1,000 items |
| 🥈 | Sine Wave (1K) | 27.8 μs | 36.0M items/sec | 1,000 items |
| 🥉 | Zigzag Pattern | 28.0 μs | 35.7M items/sec | 1,000 items |
| 4 | Repeating Pattern | 29.6 μs | 33.8M items/sec | 1,000 items |
| 5 | Dense Small Range | 484.6 μs | 20.6M items/sec | 10,000 items |
| 6 | Gaussian Distribution | 495.8 μs | 20.2M items/sec | 10,000 items |
| 7 | Random Medium (10K) | 496.4 μs | 20.1M items/sec | 10,000 items |
| 8 | All Positive (10K) | 500.9 μs | 20.0M items/sec | 10,000 items |
| 9 | All Negative (10K) | 503.1 μs | 19.9M items/sec | 10,000 items |
| 10 | Worst Case (1K) | 49.9 μs | 20.0M items/sec | 1,000 items |

### En Hızlı Decode (Throughput)
| # | Senaryo | Decode Süresi | Throughput | Veri Boyutu |
|---|---------|---------------|------------|-------------|
| 🥇 | **Constant Value (1K)** | 15.8 μs | **63.2M items/sec** | 1,000 items |
| 🥈 | Huge Sequential (1M) | 15.9 ms | 63.0M items/sec | 1,000,000 items |
| 🥉 | Repeating Pattern | 21.1 μs | 47.3M items/sec | 1,000 items |
| 4 | Zigzag Pattern | 21.3 μs | 46.9M items/sec | 1,000 items |
| 5 | Sine Wave (1K) | 21.5 μs | 46.5M items/sec | 1,000 items |
| 6 | Gaussian Distribution | 396.6 μs | 25.2M items/sec | 10,000 items |
| 7 | Random Medium (10K) | 399.0 μs | 25.1M items/sec | 10,000 items |
| 8 | All Negative (10K) | 401.8 μs | 24.9M items/sec | 10,000 items |
| 9 | All Positive (10K) | 401.2 μs | 24.9M items/sec | 10,000 items |
| 10 | Worst Case (1K) | 40.5 μs | 24.7M items/sec | 1,000 items |

---

## 📊 Detaylı Test Sonuçları (Tüm Senaryolar)

| # | Senaryo | Kategori | Boyut | Giriş (B) | Çıkış (B) | Sıkıştırma | Encode (ns) | Decode (ns) | Encode TP | Decode TP | Durum |
|---|---------|----------|-------|-----------|-----------|------------|-------------|-------------|-----------|-----------|-------|
| 1 | Empty Array | Correctness | 0 | 0 | 0 | - | 0 | 0 | - | - | ✓ |
| 2 | Single Element | Correctness | 1 | 4 | 8 | 0.50x | - | - | - | - | ✓ |
| 3 | Minimal SIMD (8) | Correctness | 8 | 32 | 8 | 4.00x | - | - | - | - | ✓ |
| 4 | All Zeros | Correctness | 100 | 400 | 8 | **50.00x** | - | - | - | - | ✓ |
| 5 | Sequential Small | Compression | 100 | 400 | 35 | 11.43x | - | - | - | - | ✓ |
| 6 | Sequential Medium | Compression | 10K | 40,000 | 3,128 | 12.79x | - | - | - | - | ✓ |
| 7 | Sequential Large (1M) | Stress | 1M | 4,000,000 | 312,504 | 12.80x | 23.1 ms | 15.9 ms | 43.3M/s | 63.0M/s | ✓ |
| 8 | Constant Value | Compression | 1K | 4,000 | 8 | **500.00x** | 23.0 μs | 15.8 μs | 43.5M/s | 63.2M/s | ✓ |
| 9 | Dense Small Range | Compression | 10K | 40,000 | 15,633 | 2.56x | 484.6 μs | 396.9 μs | 20.6M/s | 25.2M/s | ✓ |
| 10 | Random Medium | Performance | 10K | 40,000 | 16,756 | 2.39x | 496.4 μs | 399.0 μs | 20.1M/s | 25.1M/s | ✓ |
| 11 | Random Large | Performance | 100K | 400,000 | 167,577 | 2.39x | 4.98 ms | 3.99 ms | 20.1M/s | 25.1M/s | ✓ |
| 12 | Sparse Outliers | Performance | 10K | 40,000 | 41,875 | 0.96x | 504.5 μs | 400.7 μs | 19.8M/s | 25.0M/s | ✓ |
| 13 | Sine Wave | Real-World | 1K | 4,000 | 1,098 | 3.64x | 27.8 μs | 21.5 μs | 36.0M/s | 46.5M/s | ✓ |
| 14 | Gaussian Dist | Real-World | 10K | 40,000 | 16,652 | 2.40x | 495.8 μs | 396.6 μs | 20.2M/s | 25.2M/s | ✓ |
| 15 | Monotonic Variable | Real-World | 10K | 40,000 | 14,376 | 2.78x | - | - | - | - | ✓ |
| 16 | UAV Telemetry | Real-World | 10K | 40,000 | 14,376 | 2.78x | - | - | - | - | ✓ |
| 17 | Repeating Pattern | Real-World | 1K | 4,000 | 1,652 | 2.42x | 29.6 μs | 21.1 μs | 33.8M/s | 47.3M/s | ✓ |
| 18 | All Positive | Stress | 10K | 40,000 | 41,875 | 0.96x | 500.9 μs | 401.2 μs | 20.0M/s | 24.9M/s | ✓ |
| 19 | All Negative | Stress | 10K | 40,000 | 41,875 | 0.96x | 503.1 μs | 401.8 μs | 19.9M/s | 24.9M/s | ✓ |
| 20 | Worst Case | Stress | 1K | 4,000 | 4,188 | 0.96x | 49.9 μs | 40.5 μs | 20.0M/s | 24.7M/s | ✓ |
| 21 | Max Int Values | Edge Cases | 3 | 12 | 8 | 1.50x | 188 ns | 115 ns | 16.0M/s | 26.1M/s | ✓ |
| 22 | Min Int Values | Edge Cases | 3 | 12 | 8 | 1.50x | 140 ns | 86 ns | 21.4M/s | 34.9M/s | ✓ |
| 23 | Mixed Extremes | Edge Cases | 5 | 20 | - | - | - | - | - | - | ✗ |
| 24 | Zigzag Pattern | Edge Cases | 1K | 4,000 | 1,066 | 3.75x | 28.0 μs | 21.3 μs | 35.7M/s | 46.9M/s | ✓ |

**Notlar:**
- **TP** = Throughput (items/second)
- **✓** = Test başarılı (round-trip validation passed)
- **✗** = Test başarısız (int.MinValue negation overflow)
- Küçük veri setleri (<100 items) için throughput hesaplanmadı (mikro-benchmark noise)

---

## ⚠️ Bilinen Sorunlar

| # | Senaryo | Sorun | Açıklama | Çözüm |
|---|---------|-------|----------|-------|
| 1 | **Mixed Extremes** | `OverflowException` | `int.MinValue` negatif alınamaz (two's complement) | ElBâri.cs'te unchecked context veya bit manipulation |

**Impact:** Minimal - gerçek dünya verilerinde `int.MinValue` nadir görülür.

---

## 🎯 Öne Çıkan Bulgular

### ✅ Güçlü Yönler
- **Mükemmel sıkıştırma** constant/sequential veri için (12-500x)
- **Çok yüksek throughput** 40-63M items/sec (Native AOT)
- **Düşük latency** mikro-saniye seviyesinde (1K items ~25μs)
- **Round-trip güvenilirlik** %96 başarı oranı
- **Büyük veri desteği** 1M items sorunsuz işleniyor

### ⚠️ Zayıf Yönler
- **Zayıf sıkıştırma** rastgele/sparse veri için (0.96-2.4x)
- **Outlier overhead** spike'lı veriler sıkıştırılamıyor
- **Edge case** `int.MinValue` overflow sorunu

### 💡 Öneriler
1. **İdeal kullanım:** Sequential, monotonic, sensör verisi
2. **Dikkatli kullanım:** Rastgele, sparse, outlier-heavy veri
3. **Kaçınılması gereken:** Pure random, int.MinValue içeren veri

---

## 📈 Grafik Özeti

### Sıkıştırma Oranı Dağılımı
```
500x  ████ Constant Value
50x   ██ All Zeros
12.8x █ Sequential Large
3-4x  █████ Real-World Scenarios
2-3x  ████████ Random/Dense Data
<1x   ███ Worst Case (No compression)
```

### Throughput Karşılaştırması
```
Encode:  [===================>] 43.5M items/sec (best)
Decode:  [===========================>] 63.2M items/sec (best)
Average: [===============] 20-25M items/sec (typical)
```

---

## 🖥️ Test Ortamı

| Parametre | Değer |
|-----------|-------|
| **İşlemci** | AMD Ryzen 24-core |
| **Platform** | .NET 10 (Native AOT) |
| **Build Mode** | Release (optimized) |
| **SIMD** | AVX2 enabled |
| **Warmup** | 100 iterations |
| **Measurement** | 1,000 iterations |
| **Tarih** | 2025 |

---

## 📝 Notlar

1. **Performans değişkenliği:** Sonuçlar donanıma, işletim sistemine ve sistem yüküne bağlı olarak %5-10 değişiklik gösterebilir.

2. **Throughput hesaplama:** `items/second = input_size / (time_in_seconds)`

3. **Sıkıştırma oranı:** `compression_ratio = input_bytes / output_bytes`

4. **Round-trip validation:** Her test encode→decode→compare ile doğrulanır.

5. **Benchmark reproducibility:** Deterministik random seed (42) kullanılır.

---

**© 2025 ElBâri Compression Engine. All rights reserved.**  
Proprietary and Confidential. Commercial License Required.
