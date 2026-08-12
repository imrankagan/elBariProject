# Test Verisi — Kaynak ve Lisans

## gercek_gps.bin

**Ne içerir:** 24.642 gerçek GPS kaydı (3 kanal: enlem, boylam, unix zaman damgası).
İstanbul bölgesinden, 20 ayrı sürekli iz (trip) birleştirilerek oluşturulmuştur.

**Neden var:** Sentetik veri sıkıştırma algoritmalarını yanıltır. Gerçek GPS
verisinde gerçek ölçüm gürültüsü, düzensiz örnekleme aralıkları ve iz geçişleri
bulunur; benchmark sonuçlarının anlamlı olması için bunlar gereklidir.

**Biçim (little-endian):**

```
[int32] kanal sayısı  (= 3)
[int32] toplam eleman sayısı
[int32 × N] iç içe kayıtlar: lat, lon, zaman, lat, lon, zaman, ...
```

- `lat` / `lon`: 1e-7 derece ölçekli tamsayı (MAVLink/NMEA'da yaygın gösterim)
- `zaman`: unix saniye

---

## ⚠️ LİSANS UYARISI — TİCARİ DAĞITIMDAN ÖNCE OKUYUN

**Kaynak:** OpenStreetMap halka açık GPS iz arşivi
(`https://api.openstreetmap.org/api/0.6/trackpoints`)

**Lisans:** Open Database License (ODbL) — https://opendatacommons.org/licenses/odbl/
© OpenStreetMap katkıcıları

**Bu ne anlama geliyor:**

- ✅ **Geliştirme ve dahili test için kullanım serbesttir.** Bu dosya yalnızca
  benchmark fikstürüdür; kütüphanenin kendisine dahil değildir ve dağıtılan
  ikili dosyalara girmez.
- ⚠️ **Ürünle birlikte dağıtılırsa** ODbL'in atıf ve share-alike (aynı lisansla
  paylaşma) yükümlülükleri devreye girebilir. Bu, kapalı kaynak ticari lisans
  modeliyle çakışabilir.

**Öneri:** Bu klasörü ticari dağıtım paketlerinin dışında tutun. Benchmark
altyapısı bu dosya olmadan da çalışır — dosya bulunamazsa gerçekçi bir sentetik
üreteç devreye girer (bkz. `DataGenerators.GercekGpsVerisi`).

Dosyayı tamamen kaldırmak isterseniz: `TestData/` klasörünü silmeniz yeterlidir;
benchmark'lar sentetik yedekle çalışmaya devam eder.
