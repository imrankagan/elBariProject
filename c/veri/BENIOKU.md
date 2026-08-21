# Gerçek Uçuş Verisi — DataFlash log okuyucusu

Ölçümlerdeki en zayıf halka **sentetik yönelim/IMU verisiydi**. Bu klasör onu gerçek
uçuş loglarıyla değiştirmek için var.

## Neden Dataflash, neden CSV değil

[ALFA veri seti](https://theairlab.org/alfa-dataset/) uçuş verisini üç biçimde sunuyor:
ROS `.bag`, `.csv`/`.mat`, ve Pixhawk'ın kendi **Dataflash** logları.

İlk ikisi **MAVROS'tan geçmiştir.** MAVROS araya girip koordinat çerçevesini çevirir
(NED → ENU), birim dönüştürür ve Euler açılarını quaternion'a çevirir. Yani CSV'deki
sayılar **telde giden sayılar değildir.**

Biz telde ne aktığını ölçüyoruz. O yüzden okuduğumuz şey otopilotun ham kaydı olmalı:
`.bin`.

## Format kendini tanımlıyor

DataFlash logunda her kayıt `0xA3 0x95 <tip>` başlığı taşır. Tip `0x80` (FMT) özeldir:
**başka bir tipin yapısını tanımlar.** Yani log dosyası kendi sözlüğünü içinde taşır.

Bu yüzden ayrıştırıcı alanları **adıyla** arar, sabit ofsetle değil. ArduPilot sürümü
değişip mesaja alan eklense bile çalışmaya devam eder — ALFA değiştirilmiş bir
ArduPlane 3.9.0beta1 kullandığı için bu önemli.

## Kuantalama ölçekleri — ve neden bunlar

Ondalıklı log değerleri tamsayıya çevrilirken **MAVLink'in kendi gösterimiyle aynı
hassasiyet** kullanılır:

| Veri | Log birimi | Fikstür birimi | Gerekçe |
| --- | --- | --- | --- |
| Yönelim | derece | **milirad** | MAVLink `ATTITUDE` radyan taşır |
| Jiroskop | rad/sn | **mrad/sn** | `SCALED_IMU` gibi |
| İvme | m/sn² | **mg** | `SCALED_IMU` gibi |
| Enlem/boylam | int32 1e-7° | **aynen** | MAVLink ile birebir aynı, dönüşüm yok |

Kendi kafamıza göre ölçek seçmek sonuçları yanıltırdı: kaba ölçek oranı şişirir, ince
ölçek düşürür. MAVLink'in gösterimine bağlanmak bu keyfîliği ortadan kaldırıyor.

## Kullanım

```
derle.bat
donustur.exe <log.bin> --dok      # logun içinde ne var, listele
donustur.exe <log.bin>            # fikstürleri üret
```

Üretilen dosyalar `testverisi/gercek_gps.bin` ile **aynı biçimdedir**
(`[int32 kanal][int32 eleman][int32 veri...]`), yani doğrudan kıyas takımına verilebilir:

```
..\kiyas\kiyas.exe alfa_att.bin 200
```

| Dosya | İçerik |
| --- | --- |
| `alfa_att.bin` | yönelim: roll, pitch, yaw (3 kanal) |
| `alfa_imu.bin` | jiroskop + ivme (6 kanal) |
| `alfa_gps.bin` | enlem, boylam, irtifa (3 kanal) |
| `alfa_rcou.bin` | servo çıkışları (8 kanal) |
| `alfa_rcin.bin` | kumanda girişleri (8 kanal) |
| `alfa_bat.bin` | batarya (3 kanal) |
| `alfa_vibe.bin` | titreşim (3 kanal) |

Logda olmayan kayıt tipleri sessizce atlanmaz — **hangisinin neden atlandığı yazdırılır.**

## ⚠️ Veri seti kullanım koşulları

**ALFA veri seti bu depoya dâhil değildir ve edilmemelidir** (açılınca 12,5 GB).
`c/.gitignore` üretilen fikstürleri de dışarıda tutar.

Türetilmiş bir fikstürü `testverisi/` altına taşımadan önce **iki şey yapılmalıdır**:

1. **Lisansı doğrula.** ALFA'nın araç deposu BSD-3-Clause'dur ama veri setinin kendi
   lisansı ayrıdır — figshare/KiltHub sayfasındaki lisans alanına bakılmalıdır. Bu,
   `testverisi/gercek_gps.bin` ile yaşanan ODbL durumunun aynısıdır; bkz.
   [testverisi/KAYNAK.md](../../testverisi/KAYNAK.md).

2. **Atıf ver.** Veri seti atıf zorunludur:

   > Keipour, A., Mousaei, M., & Scherer, S. (2021). *ALFA: A dataset for UAV fault
   > and anomaly detection.* The International Journal of Robotics Research.
   > DOI: 10.1177/0278364920966642

## ⚠️ Veriyi yorumlarken

**Platform sabit kanattır.** Carbon Z T-28, 2 m kanat açıklığı, daire çizerek uçuyor.
Yönelimi bir çoklu rotordan **belirgin biçimde daha düzgündür**; dolayısıyla ölçülecek
sıkıştırma oranları çoklu rotor telemetrisine göre **iyimser** taraftadır. Tez çoklu
rotor hedefliyorsa bu açıkça yazılmalıdır.

**Veri setinde arıza senaryoları var.** Ani aktüatör arızaları yönelimde sıçrama
yaratır — delta kodlamayı zorlayan tam da budur. Buradan ayrı bir ölçüm çıkar:
*normal uçuşta oran X, arıza anında Y.* Sentetik veriyle yapılamayacak bir karşılaştırma.

## Kapsam

Bu klasör ölçüm ve veri hazırlama kodudur. MISRA C:2012 uyum iddiası [`c/src/`](../src/)
için geçerlidir; burası kapsam dışıdır.

## Durum

Ayrıştırıcı derleniyor, **gerçek log ile henüz sınanmadı.** İlk `.bin` geldiğinde
önce `--dok` ile içerik dökülmeli, alan adlarının beklenenle örtüştüğü doğrulanmalı,
sonra fikstürler üretilmelidir.
