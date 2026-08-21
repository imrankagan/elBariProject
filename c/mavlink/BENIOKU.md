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

**1. CRC_EXTRA hesaplanıyor, ezberden yazılmıyor.** MAVLink her mesaj için, adından ve
alan tanımlarından türetilen bir bayt taşır; amacı iki ucun *aynı* mesaj tanımını
kullandığını garanti etmektir. [mav_sema.c](mav_sema.c) içindeki `mav_crc_extra()` bunu
mavgen ile aynı algoritmayla üretir:

```
crc = X25("MESAJ_ADI ")
her alan için (tel sırasında):
    crc += "tür_adı " + "alan_adı "
    alan gerçek bir diziyse: crc += dizi_uzunluğu (tek bayt)
CRC_EXTRA = (crc & 0xFF) XOR (crc >> 8)
```

Sabit tablo yazmak yerine hesaplamanın sebebi: **yanlış bir CRC_EXTRA, paketlerin gerçek
otopilotta sessizce reddedilmesine yol açar** — boş bir tablodan beterdir, çünkü hata
görünmez. Hesaplama şemadaki alan adlarına bağlı olduğu için, şemayı kendi dialect
sürümünüze göre düzeltince CRC de kendini düzeltir.

`mav_olcum` başlarken hesaplanan değerleri bilinen referanslarla karşılaştırır ve
uyumsuzluğu **yüksek sesle** bildirir. Yine de kendi `common.h`'inizdeki
`MAVLINK_MESSAGE_CRCS` listesiyle bir kez doğrulayın.

Şeması bulunmayan msgid'ler için CRC_EXTRA bilinemez; 0 kullanılır ve böyle bir mesaj
gerçek otopilot tarafından reddedilir. Bu doğru davranıştır — tanımını bilmediğimiz bir
mesajı doğrulanmış gibi göstermek daha kötü olurdu.

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

## Hız profilleri — neden tek bir hız yetmez

Bir mesajın kaç Hz aktığı **araca değil, linke** bağlıdır. Aynı uçak USB'den saniyede
25 durum mesajı yollarken telemetri telsizinden 2 yollar. Tek bir "temsilî hız" seçmek
ölçümü keyfî kılardı: dar bant seçilirse vekilin kazancı şişer, geniş bant seçilirse düşer.

Bu yüzden hızlar mesajın kendisinde değil, bağlı olduğu **ArduPilot akış grubunda**
(`SRx_EXTRA1`, `SRx_RAW_SENS`, …) tutulur; bir **profil tablosu** grubu Hz'e çevirir ve
ölçüm her profil için ayrı koşulur.

Tablodaki iki profil uydurulmamıştır — ALFA uçuşunun kendi parametre dökümünden
(`2018-07-30 16-13-40.bin.param`, ArduPlane 3.9.0beta1) **aynen** okunmuştur:

| Grup | `SR1_*` (telsiz) | `SR0_*` (USB) | Bu şemadaki mesajlar |
| --- | --- | --- | --- |
| `RAW_SENS` | 2 Hz | 10 Hz | SCALED_IMU |
| `EXT_STAT` | 2 Hz | 25 Hz | SYS_STATUS, GPS_RAW_INT |
| `POSITION` | 2 Hz | 10 Hz | GLOBAL_POSITION_INT |
| `RC_CHAN` | 2 Hz | 10 Hz | RC_CHANNELS, SERVO_OUTPUT_RAW |
| `EXTRA1` | 4 Hz | 10 Hz | ATTITUDE |
| `EXTRA2` | 4 Hz | 10 Hz | VFR_HUD |
| `EXTRA3` | 2 Hz | 10 Hz | VIBRATION, BATTERY_STATUS |

`HEARTBEAT` bir akış grubuna bağlı değildir; ArduPilot onu her iki bağlantıda da 1 Hz yayınlar.

**Sonuç ikisi arasında keskin biçimde ayrışır** ve bu, vekilin en önemli sınırıdır:
dar bantlı SR1 profilinde 2 saniyelik bütçe başına yalnızca 4–8 kayıt birikir, çerçeve
başlığı bu kadar az kaydın üzerine dağıtılamaz. Kazanç 2 sn'de **1.16x**'te kalır, kısa
gecikmelerde 1'in **altına** düşer (taban çizgisine geri düşülür, zarar edilmez).
Geniş bantlı SR0 profilinde aynı bütçe 20–50 kayıt biriktirir ve kazanç **1.71x** olur.

Yani: *iki kademeli vekil, akış hızı yükseldikçe kazanır.* Dar bantlı bir telsizde
kazanmak için gecikme bütçesini büyütmek (5 sn → 1.68x) gerekir.

## Gerçek uçuş verisi bağlama

Konum, yönelim ve IMU kanalları gerçek ArduPilot logundan beslenebilir:

```
mav_olcum alfa_gps.bin 300 --att alfa_att.bin --imu alfa_imu.bin
```

Fikstürleri [`c/veri/donustur.exe`](../veri/) üretir. Verilmeyen kanal sentetiğe düşer
ve çıktının başında **hangisinin gerçek olduğu açıkça yazılır.** Açısal hız alanları
`ATT` kaydında bulunmadığı için `IMU` jiroskopundan gelir.

Gerçek veri sentetikten **iyi** çıkar — sentetik üreteç gürültüyü bilerek yüksek tuttuğu
için (aşağıda §5) kötümser taraftaydı. SCALED_IMU tek başına 1.77x → **3.24x**,
ATTITUDE 4.06x → **6.44x** (SR0, 2 sn).

> Gerçek IMU verisi çekirdek kodekte bir **kayıpsızlık hatası** ortaya çıkardı: ardışık
> farkın tam olarak 2³¹ olduğu durumda (işareti değişip büyüklüğü aynı kalan float)
> üst bit kayboluyordu. Düzeltildi ve regresyon vektörü eklendi; ayrıntı
> [belgeler/MAVLINK_VEKIL.md §5](../../belgeler/MAVLINK_VEKIL.md).

## Şema tablosunu kendi aracınıza uyarlama

[mav_sema.c](mav_sema.c) içinde iki şey değiştirilir:

1. **`PROFILLER` tablosu** — kendi aracınızın `SRx_*` parametrelerini girin
   (PX4'te akış yapılandırması). Ölçümün gerçekçiliği doğrudan buna bağlıdır.
2. **Kademe ataması** — bir mesajın üzerine gerçek zamanda karar veriliyorsa
   `KADEME_CANLI`, değilse `KADEME_TOPLU`. Emin değilseniz CANLI seçin: güvenli taraf odur.
