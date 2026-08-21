# İki Kademeli MAVLink Vekili — Ölçüm

> Tasarım, sınırlar ve şema politikası: [`c/mavlink/BENIOKU.md`](../c/mavlink/BENIOKU.md)
> · Ölçüm kodu: [`c/mavlink/mav_olcum.c`](../c/mavlink/mav_olcum.c)

## 1. Soru

ElBâri'nin vitrin rakamları (5.05x) tek bir büyük blok sıkıştırmaktan geliyor. **Canlı
İHA telemetrisinde** durum farklıdır: veri damla damla akar, kritik mesajlar bekleyemez
ve her paket telsizin MTU'suna sığmak zorundadır.

Bu ölçüm şunu soruyor: *Gerçek bir MAVLink akışında, kritik telemetriyi geciktirmeden,
ne kadar bant genişliği kazanılır?*

## 2. Kurgu

Telsizin iki ucuna birer **şeffaf vekil** konur; otopilot ve yer istasyonu normal
MAVLink konuşur. Akış ikiye ayrılır:

- **CANLI** — HEARTBEAT, SYS_STATUS + seyreltilmiş konum/gösterge. Gecikme **sıfır**.
- **TOPLU** — ATTITUDE, IMU, RC, servo, GPS, titreşim, batarya. msgid'ye göre
  biriktirilip ElBâri ile sıkıştırılır.

### Veri: artık üç kanal da gerçek

| Kanal | Kaynak |
| --- | --- |
| Enlem / boylam / irtifa | **GERÇEK** — ALFA `GPS` kaydı → `alfa_gps.bin` |
| Yönelim (ATTITUDE roll/pitch/yaw) | **GERÇEK** — ALFA `ATT` kaydı → `alfa_att.bin` |
| IMU (SCALED_IMU jiro + ivme) | **GERÇEK** — ALFA `IMU` kaydı → `alfa_imu.bin` |
| RC, servo, batarya, titreşim, manyetometre | sentetik |

Açısal hız alanları (`rollspeed`/`pitchspeed`/`yawspeed`) gerçek jiroskoptan gelir;
`ATT` kaydı hız taşımadığı için `IMU` kaydı kullanılır. Bu ikisi **örnek örnek
hizalanmış değildir** — ama sıkıştırma her kanalı kendi içinde işlediğinden önemli
olan kanalın istatistiğidir, o da gerçektir.

```
mav_olcum alfa_gps.bin 300 --att alfa_att.bin --imu alfa_imu.bin
```

`--att` / `--imu` verilmezse o kanallar sentetiğe düşer ve çıktının başında
**hangisinin gerçek olduğu açıkça yazılır.**

### Yayın hızı tek bir sayı değildir

Bir mesajın kaç Hz aktığı **araca değil, linke** bağlıdır. Aynı uçak USB'den saniyede
25 durum mesajı yollarken telemetri telsizinden 2 yollar. Önceki sürümde tek bir
"temsilî hız" tablosu vardı ve bu, kazanç rakamını **seçilmiş bir varsayıma**
bağlıyordu.

Artık hızlar mesajın kendisinde değil, bağlı olduğu **ArduPilot akış grubunda**
(`SRx_EXTRA1`, `SRx_RAW_SENS`, …) tutuluyor ve ölçüm iki profille ayrı ayrı koşuluyor.
Profil değerleri uydurulmamış — ALFA uçuşunun kendi parametre dökümünden
(`2018-07-30 16-13-40.bin.param`, ArduPlane 3.9.0beta1) **aynen** okunmuştur:

| | **SR1** — telemetri telsizi | **SR0** — USB / companion |
| --- | --- | --- |
| Süre / mesaj | 300 s / 7.501 | 300 s / 39.296 |
| Taban çizgi | **953 bayt/sn** | **5.252 bayt/sn** |
| ATTITUDE | 4 Hz (`SR1_EXTRA1`) | 10 Hz (`SR0_EXTRA1`) |
| SCALED_IMU | 2 Hz (`SR1_RAW_SENS`) | 10 Hz (`SR0_RAW_SENS`) |
| SYS_STATUS / GPS_RAW_INT | 2 Hz (`SR1_EXT_STAT`) | 25 Hz (`SR0_EXT_STAT`) |

Telsiz MTU'su her iki profilde 250 bayt varsayıldı (SiK / RFD900 sınıfı). **Her ayarda
tam tur geçti** — YİS aynı mesajları geri alıyor.

## 3. Sonuç — profile göre keskin biçimde ayrışıyor

**Ondalıklı alanlar kuantalanmış** (yönelim 0,001 rad; hız/irtifa 0,01 birim):

| Gecikme bütçesi | SR1 B/sn | SR1 kazanç | SR0 B/sn | SR0 kazanç |
| ---: | ---: | ---: | ---: | ---: |
| 0,25 s | 1.026 | 0,93x | 5.089 | 1,03x |
| 0,50 s | 1.026 | 0,93x | 4.180 | 1,26x |
| 1,00 s | 933 | 1,02x | 3.378 | 1,56x |
| 2,00 s | 825 | **1,16x** | 3.070 | **1,71x** |
| 5,00 s | 569 | **1,68x** | 3.063 | 1,71x |

**Kayıpsız** (float bit desenleri korunur):

| Gecikme bütçesi | SR1 kazanç | SR0 kazanç |
| ---: | ---: | ---: |
| 0,25 s | 0,93x | 1,02x |
| 0,50 s | 0,93x | 1,20x |
| 1,00 s | 0,97x | 1,45x |
| 2,00 s | 1,08x | 1,53x |
| 5,00 s | 1,42x | 1,51x |

### Bu tablonun asıl söylediği

**Vekil, akış hızı yükseldikçe kazanır.** Sebep aritmetik: çerçeve başlığı sabittir,
kazanç ancak yeterince kayıt üstüne dağıtılınca çıkar.

- SR1'de 2 saniyelik bütçe başına yalnızca **4–8 kayıt** birikir. Kazanç 1,16x'te kalır,
  kısa gecikmelerde 1'in **altına** düşer — orada vekil ham geçişe düşerek taban
  çizgisini korur, ama bir şey de kazandırmaz.
- SR0'da aynı bütçe **20–50 kayıt** biriktirir ve kazanç 1,71x'e çıkar.

Dar bantlı bir telsizde kazanmanın yolu gecikme bütçesini büyütmektir: SR1'de 5 saniye
**1,68x** verir. Kritik telemetri zaten CANLI kademede olduğu için bu geçerli bir takas.

SR0'da 2 s ile 5 s arasında kazanç **artmıyor** (1,71x → 1,71x): orada sınır artık
gecikme değil, MTU bölmesidir.

> **Gerçek veri sentetikten İYİ çıktı.** Sentetik üreteç gürültüyü bilerek yüksek
> tutuyordu (bkz. `c/mavlink/BENIOKU.md` §5) — yani kötümser tarafa eğikti. Gerçek
> uçuş verisiyle SR0/2 s kazancı 1,65x → **1,71x**, SCALED_IMU tek başına
> 1,77x → **3,24x** oldu. Tasarım kararı tuttu: sentetik sayılar tavan değil, tabandı.

## 4. Mesaj başına kırılım (2 s, kuantalanmış)

Toplam kazanç rakamı, hangi mesajın işe yaradığını gizler.

**SR1 (telsiz) — çoğu mesaj eşiğin altında:**

| Mesaj | Ham B/sn | Link B/sn | Kazanç | Ham geçiş |
| --- | ---: | ---: | ---: | ---: |
| **ATTITUDE** | 158 | 52 | **3,04x** | 0 |
| VFR_HUD | 124 | 53 | 2,34x | 0 |
| GLOBAL_POSITION_INT | 80 | 74 | 1,08x | 0 |
| GPS_RAW_INT | 84 | 81 | 1,04x | 1 |
| VIBRATION | 64 | 62 | 1,03x | 0 |
| RC_CHANNELS | 108 | 110 | 0,98x | 178 |
| BATTERY_STATUS | 96 | 98 | 0,98x | 150 |
| SCALED_IMU | 68 | 70 | 0,97x | 124 |
| SERVO_OUTPUT_RAW | 64 | 66 | 0,97x | 150 |

**SR0 (USB) — aynı mesajlar, dört-beş katı hız:**

| Mesaj | Ham B/sn | Link B/sn | Kazanç | Ham geçiş |
| --- | ---: | ---: | ---: | ---: |
| **ATTITUDE** | 397 | 62 | **6,44x** | 0 |
| VFR_HUD | 310 | 69 | 4,50x | 0 |
| **SCALED_IMU** | 340 | 105 | **3,24x** | 0 |
| SERVO_OUTPUT_RAW | 320 | 113 | 2,84x | 0 |
| VIBRATION | 320 | 113 | 2,83x | 0 |
| GPS_RAW_INT | 1.050 | 408 | 2,57x | 0 |
| GLOBAL_POSITION_INT | 400 | 182 | 2,20x | 0 |
| BATTERY_STATUS | 480 | 270 | 1,78x | 0 |
| RC_CHANNELS | 540 | 465 | 1,16x | 255 |

- **ATTITUDE her iki profilde de yıldız** — gerçek yönelim, sentetiğin verdiği 4,06x'i
  aşıp SR0'da 6,44x'e çıkıyor. Sabit kanat uçuşunun düzgünlüğü burada doğrudan görünüyor.
- **Hız, kazancı doğrudan taşıyor:** SCALED_IMU, SERVO_OUTPUT_RAW, VIBRATION ve
  BATTERY_STATUS SR1'de ham geçişe düşerken SR0'da 1,8–3,2x veriyor. Aynı mesaj, aynı
  kod — değişen tek şey saniyedeki kayıt sayısı.
- **RC_CHANNELS her iki profilde de en zayıf halka**: 21 kanal × bütçe = büyük paket,
  MTU'da bölününce parçalar sıkıştırma eşiğinin altına düşüyor.
- Vekil hiçbir mesajda taban çizgisinin **altına** düşmüyor; kazandırmayan kayıtlar ham
  gönderiliyor.

## 5. Ölçümün ortaya çıkardığı üç hata

Üçü de **tahminle değil ölçümle** bulundu ve düzeltildi.

**1. Vekil trafiği artırıyordu.** İlk sürümde 0,5 s'de **0,80x** — yani sıkıştırma linki
kötüleştiriyordu. Sebep: paket başına 25 bayt sabit yük (9 link başlığı + 16 çerçeve
başlığı), küçük gruplarda kazancı yiyor.

*Çözüm:* her parça için sıkıştırılmış ve ham aday üretip küçüğünü seçmek. Artık vekil
taban çizgisinden **hiçbir koşulda kötü olamaz**.

**2. Paketler MTU'yu aşıyordu.** 1 s ve üstünde toplu paket 258–1.274 bayta çıkıyordu.
Aşarsa alt katman parçalar; bir parça düşünce çerçeve komple gider ve *"bir paket düşerse
yalnızca o çerçeve kaybolur"* garantisi çöker.

*Çözüm:* toplu paketi MTU'ya sığacak parçalara bölmek. Sonuç: **paket boyutu gecikme
bütçesinden bağımsız** — gecikmeyi büyütmek paketleri büyütmez, sadece daha çok parça
üretir.

*Bedeli ölçüldü:* MTU sert bir kısıttır ve bedava değildir. SR0 profilinde etkisi
doğrudan görünür: 2 s ile 5 s arasında kazanç artık **hiç artmıyor** (1,71x → 1,71x),
çünkü sınırlayan artık gecikme değil paket bölmesidir.

**3. Çekirdek kodek, tam olarak 2³¹'lik bir farkta sessizce bit kaybediyordu.**
Gerçek IMU verisi bağlanır bağlanmaz ATTITUDE **kayıpsız modda tam turu geçemedi.**

*Sebep:* ardışık iki değerin farkı tam olarak `INT32_MIN` olduğunda — yani bir float'ın
işareti değişip büyüklüğü aynı kaldığında (`+0.001f` → `-0.001f`, bit desenleri
`0x3A83126F` / `0xBA83126F`) — 32 bitlik mutlak değer **negatif kalıyordu.** Bu yüzden
fark ne "aykırı" işaretleniyor (tam 32 bitle yazılmıyor) ne de blok bit genişliğini
yükseltiyordu; sonra dar maskeyle paketlenip **üst biti kaybediyordu.** Çözücü değeri
işareti ters dönmüş halde geri veriyordu.

*Çözüm:* aykırı-değer sınıflandırması artık büyüklüğü **64 bitte** ölçüyor
([`c/src/elbari.c`](../c/src/elbari.c), blok kodlayıcı). Böyle bir fark aykırı sayılıp
tam 32 bitle yazılıyor.

*Neden daha önce yakalanmadı:* 27 uygunluk vektörünün hiçbirinde 2³¹'lik fark yoktu, ve
`fuzz` bir **çözücü sağlamlık** testidir — bozuk girdiyi çözücüye verir, rastgele
*değerleri* kodlayıp geri okumaz. Sentetik yönelim verisi de sıfırı geçiyordu ama
işaret değişimi hep büyüklük değişimiyle birlikte oluyordu; tam olarak 2³¹ hiç çıkmadı.
Bu deseni ancak **gerçek jiroskop** üretti.

*Regresyon koruması:* `cekirdek_isaret_biti` vektörü eklendi
([`testverisi/vektorler.txt`](../testverisi/vektorler.txt)). Mevcut 27 vektörün çıktısı
**değişmedi** — düzeltme yalnızca zaten bozuk olan akışları etkiliyor, biçim kırılmadı.

> ⚠️ **Aynı hata C# sürümünde de olabilir.** 32 bitlik mutlak değer davranışı iki sürüm
> arasında bilinçli olarak aynı tutulmuştu; `kaynak/` altındaki karşılığı denetlenmelidir.

## 6. Ne söylenebilir, ne söylenemez

**Söylenebilir:**
> Gerçek bir ArduPlane uçuşunun konum, yönelim ve IMU verisiyle; kritik telemetriyi hiç
> geciktirmeden, tüm paketler telsiz MTU'suna sığarken ve kayıpsız geri dönüş
> doğrulanmışken: **USB profilinde (SR0) 2 saniyelik bütçeyle 1,71x**, **telemetri
> telsizi profilinde (SR1) 5 saniyelik bütçeyle 1,68x** bant genişliği kazancı.
> Tek başına ATTITUDE mesajında SR0'da **6,44x**.

**Söylenemez:**
- **Tek bir kazanç rakamı.** Bu sayı hangi hız profilinin seçildiğine bağlıdır; profil
  belirtilmeden verilirse yanıltıcıdır.
- **Dar bantlı telsizde kısa gecikmeyle kazanç.** SR1 profilinde 2 s bütçe yalnızca
  1,16x, 1 s ve altı başabaş ya da altında. Vekilin en dar olduğu yer, en çok
  ihtiyaç duyulan yerdir — bu, tasarımın bilinen ve ölçülmüş sınırıdır.
- "5.05x" — o rakam tek blok sıkıştırmaya aittir, canlı telemetriye değil.
- **Bu sayıların her İHA'yı temsil ettiği.** Platform sabit kanattır (Carbon Z T-28);
  yönelimi bir çoklu rotordan belirgin biçimde daha düzgündür, dolayısıyla ATTITUDE
  kazancı çoklu rotor telemetrisine göre **iyimser** taraftadır.
- RC, servo, batarya, titreşim ve manyetometre hâlâ **sentetiktir**.

## 7. Sıradaki adımlar

1. **C# sürümünde 2³¹ farkı denetle.** §5'teki üçüncü hata C sürümünde düzeltildi;
   `kaynak/` altındaki karşılığında aynı desen olabilir.
2. **Kodlama tarafı için değer fuzz'ı ekle.** Mevcut `fuzz` yalnızca çözücü
   sağlamlığını sınıyor. Rastgele *değerleri* kodlayıp geri okuyan bir tur bu hatayı
   çok daha önce yakalardı.
3. **SITL'de (ArduPilot/PX4) uçtan uca dene.** CRC_EXTRA artık şemadan hesaplanıyor ve
   bilinen referansla örtüşüyor; sıradaki adım canlı bir bağlantı.
3. **Kademe tablosunu ayarla** — BATTERY_STATUS ve VIBRATION toplu kademede yer
   kaplıyor ama kazandırmıyor.
4. **RC_CHANNELS için kanal sayısını şemadan kısıtla** — 18 kanalın 8'i kullanılıyor;
   kullanılmayanlar kanal olarak taşınmasa paket küçülür.
5. **Paket kaybı altında ölç** — çerçeveler bağımsız ama bu koşullarda doğrulanmadı.
