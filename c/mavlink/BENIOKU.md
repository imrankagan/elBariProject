# İki Kademeli MAVLink Vekili

## Fikir

Telsizin **iki ucuna birer vekil** konur. Otopilot ve yer istasyonu normal MAVLink
konuşur; arada, link üzerinde sıkıştırılmış akar.

```
[Otopilot] --MAVLink--> [vekil] ~~~telsiz~~~ [vekil] --MAVLink--> [Yer İstasyonu]
```

MAVLink'in **yerine geçmez, altına girer.** Uçuş yazılımı ve YİS değişmez.

## Neden iki kademe

Sıkıştırma toplu iş ister: N örnek birikmeden paket gönderilemez. Ama telemetrinin bir
kısmı **bekleyemez** — kalp atışı, uçuş modu, batarya alarmı. Bu yüzden akış ikiye
ayrılır:

| Kademe | İçerik | Gecikme | Sıkıştırma |
| --- | --- | --- | --- |
| **CANLI** | HEARTBEAT, SYS_STATUS + seyreltilmiş konum/gösterge | **0** | yok |
| **TOPLU** | ATTITUDE, IMU, RC, servo, GPS, titreşim, batarya | tampon süresi | var |

**Seyreltme:** GLOBAL_POSITION_INT ve VFR_HUD'un her N'inci örneği *ayrıca* canlı akışa
konur. Operatör haritada 1 Hz sıfır gecikmeli konum görür; tam hızlı veri sıkıştırılmış
akar. Bedeli küçük bir tekrardır.

## Neden çalışıyor

MAVLink akışı heterojendir — ATTITUDE, GPS, RC_CHANNELS hepsi farklı yapıda. Ama
**her mesaj tipinin alan düzeni sabittir.** msgid'ye göre ayırınca her grup, ElBâri'nin
beklediği düzenli kayıt matrisine dönüşür:

```
satırlar = mesaj örnekleri     sütunlar = alanlar (kanallar)
```

Yani "ElBâri MAVLink'e uymaz" doğru değil — msgid başına ayırınca birebir uyuyor.

## İki koruyucu mekanizma

Bunlar tasarımın en önemli parçalarıdır; ikisi de **ölçüm sonucu eklendi**.

**1. Ham geçişe düşme.** Küçük toplu paketlerde sabit yük (9 bayt link başlığı + 16 bayt
çerçeve başlığı = 25 bayt) sıkıştırma kazancını aşabilir. Her parça için iki aday
üretilir — sıkıştırılmış ve ham — ve **küçük olan seçilir**. Bu olmadan vekil, düşük
gecikme bütçelerinde taban çizgisinden *daha çok* bayt harcıyordu (ölçüldü: 0.80x).

**2. MTU'ya bölme.** Toplu paket telsizin taşıyabildiği boyutu aşarsa alt katman
parçalar; bir parça düşerse çerçeve komple gider ve "bağımsız çerçeve" garantisi çöker.
Bu yüzden paket, MTU'ya sığacak parçalara bölünür. Sonuç: **paket boyutu gecikme
bütçesinden bağımsız** — gecikmeyi büyütmek paketleri büyütmez, yalnızca daha çok parça
üretir.

## Dosyalar

| Dosya | İçerik |
| --- | --- |
| [mav.h](mav.h) | Tüm arayüz, link paketi biçimi, sınırlar |
| [mav_bicim.c](mav_bicim.c) | MAVLink v2 çerçeve okuma/yazma, CRC-16/MCRF4XX, yük kırpma |
| [mav_sema.c](mav_sema.c) | **Mesaj şema tablosu ve kademe politikası** — değiştirilecek yer burası |
| [mav_kademe.c](mav_kademe.c) | Vekil: yönlendirme, biriktirme, MTU bölme, ham geçiş |
| [mav_uretici.c](mav_uretici.c) | Sentetik MAVLink akışı üreteci (ölçüm için) |
| [mav_olcum.c](mav_olcum.c) | Ölçüm programı |

## ⚠️ Sınırlar — kullanmadan önce okuyun

**1. CRC_EXTRA tablosu boştur.** Gerçek MAVLink CRC'si, mesaj adından ve alan
türlerinden türetilen bir CRC_EXTRA baytı gerektirir. Bu tablo üretilmiş MAVLink
başlıklarından doldurulmadan **gerçek bir otopilotla konuşulamaz.** Ölçüm için önemsizdir
(CRC her hâlükârda 2 bayttır, boyut hesapları değişmez), entegrasyon için zorunludur.

**2. Şema tablosu elle yazılmıştır.** Alan düzenleri `common.xml`'in yaygın sürümüne
göre girilmiştir. MAVLink v2 "message extension" özelliği mesajlara sonradan alan
ekleyebilir. Kendi dialect sürümünüzle doğrulayın — `mav_sema_dogrula()` alan
boyutlarının toplamının yük boyutunu tutturduğunu denetler ve program başında çalışır.

**3. Sıra numarası (seq) korunamaz.** MAVLink seq tüm akış için tek sayaçtır; msgid'ye
göre biriktirince korunamaz. Alıcı taraf seq'i yeniden üretir. Yer istasyonunun "paket
düştü" sayacı vekil arkasında anlamını yitirir — kayıp tespiti link katmanına (CRC32 +
çerçeve sıra no) taşınmıştır. **Bu gerçek bir davranış değişikliğidir**, entegrasyonda
hesaba katılmalıdır.

**4. Kuantalama kayıplıdır.** Yönelim 0.001 rad, hız/irtifa 0.01 birim hassasiyetle
taşınır. Tam değer gerekiyorsa kuantalama kapatılabilir (oran düşer). Hassasiyetler
[mav_sema.c](mav_sema.c) içindeki alan tablolarındadır.

**5. Sentetik veri.** Ölçüm akışında enlem/boylam gerçektir (OpenStreetMap GPS izleri);
yönelim, IMU, RC, servo, batarya, titreşim **sentetiktir**. Gürültü bilerek yüksek
tutulmuştur (sonuçlar iyimser değil kötümser tarafa eğik), ama gerçek bir uçuş logu
(PX4 ULog / ArduPilot .bin) ile tekrarlanmadan tezde kullanılmamalıdır.

**6. Kapsam.** Bu klasör ölçüm ve referans uygulamadır. MISRA C:2012 uyum iddiası
[`c/src/`](../src/) için geçerlidir; burası kapsam dışıdır.

## Derleme ve çalıştırma

Windows (MSVC):

    derle.bat
    mav_olcum.exe ..\..\testverisi\gercek_gps.bin 300

Linux / macOS:

    make -C .. mav_olcum
    ../mav_olcum ../../testverisi/gercek_gps.bin 300

İkinci argüman simüle edilecek uçuş süresidir (saniye, varsayılan 300).

## Şema tablosunu kendi aracınıza uyarlama

[mav_sema.c](mav_sema.c) içinde iki şey değiştirilir:

1. **`hz` değerleri** — ArduPilot'ta `SR0_*` parametreleriyle, PX4'te akış
   yapılandırmasıyla eşleştirin. Ölçümün gerçekçiliği doğrudan buna bağlıdır.
2. **Kademe ataması** — bir mesajın üzerine gerçek zamanda karar veriliyorsa
   `KADEME_CANLI`, değilse `KADEME_TOPLU`. Emin değilseniz CANLI seçin: güvenli taraf odur.
