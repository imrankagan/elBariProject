# İki Kademeli MAVLink Vekili — Ölçüm

> Tasarım, sınırlar ve şema politikası: [`c/mavlink/BENIOKU.md`](../c/mavlink/BENIOKU.md)
> · Ölçüm kodu: [`c/mavlink/mav_olcum.c`](../c/mavlink/mav_olcum.c)

## 1. Soru

ElBâri'nin vitrin rakamları (4.95x) tek bir büyük blok sıkıştırmaktan geliyor. **Canlı
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

| | |
| --- | --- |
| Senaryo | 300 saniye uçuş, 15.001 mesaj, 11 mesaj tipi |
| Taban çizgi | **1.923 bayt/sn** (ham MAVLink v2, yük kırpması uygulanmış) |
| Telsiz MTU'su | 250 bayt varsayıldı (SiK / RFD900 sınıfı) |
| Doğrulama | Her ayarda tam tur geçti — YİS aynı mesajları geri alıyor |

Konum verisi gerçektir (OpenStreetMap GPS izleri); yönelim, IMU, RC, servo, batarya ve
titreşim sentetiktir — gürültü bilerek yüksek tutulmuştur (bkz. BENIOKU §5).

## 3. Sonuç

**Ondalıklı alanlar kuantalanmış** (yönelim 0,001 rad; hız/irtifa 0,01 birim):

| Gecikme bütçesi | Vekil B/sn | Kazanç | Canlı B/sn | En büyük paket |
| ---: | ---: | ---: | ---: | ---: |
| 0,25 s | 2.003 | 0,96x | 183 | 119 |
| 0,50 s | 1.832 | 1,05x | 183 | 167 |
| 1,00 s | 1.459 | **1,32x** | 183 | 250 |
| 2,00 s | 1.175 | **1,64x** | 183 | 250 |
| 5,00 s | 1.028 | 1,87x | 183 | 250 |

**Kayıpsız** (float bit desenleri korunur):

| Gecikme bütçesi | Vekil B/sn | Kazanç |
| ---: | ---: | ---: |
| 0,25 s | 2.046 | 0,94x |
| 0,50 s | 1.968 | 0,98x |
| 1,00 s | 1.713 | 1,12x |
| 2,00 s | 1.489 | 1,29x |
| 5,00 s | 1.362 | 1,41x |

Kuantalama, canlı telemetride kazancın yaklaşık **üçte birini** sağlıyor.

## 4. Mesaj başına kırılım (2 s, kuantalanmış)

Toplam kazanç rakamı, hangi mesajın işe yaradığını gizler:

| Mesaj | Ham B/sn | Link B/sn | Kazanç | Ham geçiş |
| --- | ---: | ---: | ---: | ---: |
| **ATTITUDE** | 400 | 98 | **4,06x** | 0 |
| VFR_HUD | 124 | 53 | 2,33x | 0 |
| GLOBAL_POSITION_INT | 200 | 94 | 2,13x | 0 |
| GPS_RAW_INT | 210 | 99 | 2,12x | 0 |
| SCALED_IMU | 340 | 192 | 1,77x | 0 |
| SERVO_OUTPUT_RAW | 160 | 96 | 1,67x | 0 |
| RC_CHANNELS | 270 | 248 | 1,09x | 75 |
| VIBRATION | 64 | 62 | 1,03x | 0 |
| BATTERY_STATUS | 48 | 49 | 0,98x | **150 (tamamı)** |

- **ATTITUDE tek başına yıldız**: 10 Hz, düzgün kanallar, bant genişliğinin %21'i.
- **BATTERY_STATUS hiç sıkışmıyor** ve %100 ham geçişe düşüyor — 1 Hz'lik bir mesajı
  biriktirmenin anlamı yok. Doğru davranış budur; kademe tablosunda CANLI'ya alınabilir.
- **RC_CHANNELS**, MTU bölmesinden en çok etkilenen mesaj: 21 kanal × 2 s = büyük paket,
  bölününce parçalar sıkıştırma eşiğinin altına düşüyor.

## 5. Ölçümün ortaya çıkardığı iki tasarım hatası

Bu ikisi **tahminle değil ölçümle** bulundu ve düzeltildi.

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

*Bedeli ölçüldü:* 2 s'de kazanç 1,91x → **1,64x**, 5 s'de 2,70x → 1,87x. MTU sert bir
kısıttır ve bedava değildir.

## 6. Ne söylenebilir, ne söylenemez

**Söylenebilir:**
> Kritik telemetriyi hiç geciktirmeden, 2 saniyelik tamponlama bütçesiyle, tüm paketler
> telsiz MTU'suna sığarken ve kayıpsız geri dönüş doğrulanmışken **1,64x** bant genişliği
> kazancı.

**Söylenemez:**
- "4.95x" — o rakam tek blok sıkıştırmaya aittir, canlı telemetriye değil.
- 0,5 s'nin altında anlamlı kazanç. Orada başabaş.
- Bu sayıların gerçek uçuş verisini temsil ettiği — akışın yönelim/IMU kısmı sentetiktir.

## 7. Sıradaki adımlar

1. **Gerçek uçuş logu** (PX4 ULog / ArduPilot .bin) ile tekrarla. Sentetik yönelim/IMU
   verisi bu tablonun en zayıf halkası.
2. **CRC_EXTRA tablosunu doldur** ve SITL'de (ArduPilot/PX4) uçtan uca dene.
3. **Kademe tablosunu ayarla** — BATTERY_STATUS ve VIBRATION toplu kademede yer
   kaplıyor ama kazandırmıyor.
4. **RC_CHANNELS için kanal sayısını şemadan kısıtla** — 18 kanalın 8'i kullanılıyor;
   kullanılmayanlar kanal olarak taşınmasa paket küçülür.
5. **Paket kaybı altında ölç** — çerçeveler bağımsız ama bu koşullarda doğrulanmadı.
