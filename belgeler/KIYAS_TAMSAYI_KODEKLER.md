# Kıyas — ElBâri, ait olduğu tamsayı kodek ailesiyle

> Ölçüm kodu: [`c/kiyas/`](../c/kiyas/) · Metodoloji ve sınırlar:
> [`c/kiyas/BENIOKU.md`](../c/kiyas/BENIOKU.md) · Ham çıktı: `kiyas_sonuclari.csv`

## 1. Bu ölçüm neden yapıldı

ElBâri kendi belgelerinde açıkça **"yeni bir algoritma değil, PFOR-Delta ailesinin bir
uygulamasıdır"** diyor. Buna rağmen bugüne kadarki tüm karşılaştırmalar **genel amaçlı
bayt sıkıştırıcılarıyla** (zstd, LZ4, Brotli, Deflate) yapılmıştı.

Bu, karşılaştırmanın en zayıf noktasıydı. Bir kodeği kendi ailesine yerleştirip o ailenin
hiçbir üyesiyle ölçmemek, katkının büyüklüğü hakkında hiçbir şey söylemez — "zstd'yi üç
kat geçtik" cümlesi doğru rakip zstd olmadığı için anlamsızdır.

Bu belge o boşluğu kapatıyor ve **beklendiği gibi, iddiayı küçültüyor.**

## 2. Metodoloji

| | |
| --- | --- |
| Veri | §3 ana tablosu: `testverisi/gercek_gps.bin` — 24.642 gerçek GPS kaydı × 3 kanal. Ayrıca ALFA uçuş logundan altı fikstür (§3, yedi veri seti tablosu) |
| Ham boyut | 295.704 bayt (ana tablo); 164 KB – 3,2 MB (uçuş fikstürleri) |
| Ortam | Windows 11, x64, MSVC 19.51, `/std:c17 /W4 /O2` |
| Tur | 200 (her kodek için), 5 tur ısınma |
| Derleme sınıfı | **Skaler C, SIMD yok — hem ElBâri hem rakipler** |
| Doğrulama | Her kodek tam tur (round-trip) doğrulamasından geçti |

**Adil karşılaştırma için yapılanlar:**

- **Ön işleme rakiplere bedava verildi.** Kanal ayrımı + fark + zigzag, tek akış
  kodekleri için dışarıda yapılır ve **bu süre onların encode süresine dahildir.**
  ElBâri aynı işleri kendi içinde yapar.
- **Başlık yükü sayılır.** Her kodeğin kendi çerçeveleme maliyeti boyuta dahildir.
- **Boyutlar deterministik.** Üç ayrı koşuda bayt bayt aynı; hızda ~%3 oynama.
- **Rakip uygulamalar ayrıca kenar durum testinden geçti.** Oran sütunu ancak
  uygulamalar doğruysa anlamlıdır; gerçek GPS verisi kuyruk yollarını, sınır değerleri
  (0, 2³¹, 2³²−1) ve tamamen sıfır blokları tetiklemez.
  [`c/kiyas/oztest.c`](../c/kiyas/oztest.c) her kodeği 6 değer dağılımı × 24 uzunluk
  (Sprintz için ayrıca 6 kanal sayısı) ile doğrular — **toplam 1.080 durum, hepsi
  kayıpsız.**

> ### ⚠️ Hız sütununu okumadan önce
>
> Rakip kodekler **yazarlarının kütüphaneleri değildir**; yayınlanmış biçim
> tanımlarından yeniden yazılmış skaler C uygulamalarıdır.
>
> - **Oran taşınabilirdir.** Bir biçimin ürettiği bayt sayısı biçim tanımından gelir,
>   uygulama kalitesinden değil. Oran sütunu literatürle karşılaştırılabilir.
> - **Hız taşınabilir değildir.** FastPFor ve StreamVByte'ın SIMD sürümleri buradakinden
>   kat kat hızlıdır. **Hız sütunu rakipler için bir ALT SINIRDIR.**
>
> Bu yüzden aşağıda "ElBâri daha hızlı" iddiası **kurulmamıştır.** Kurulabilmesi için
> yazarların SIMD'li kütüphanelerinin bağlanması gerekir — bkz. §8.

## 3. Sonuç — kanal ayrımı ile (adil kıyas)

| Kodek | Kaynak | Bayt | Oran | bit/değer | encode MB/sn | decode MB/sn |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| **ElBâri (kanal)** | bu çalışma | **59.695** | **4.95x** | **6.46** | 1.200 | 1.380 |
| Sprintz-Delta | Blalock ve ark. 2018 | 63.321 | 4.67x | 6.85 | 463 | 1.045 |
| Simple8b | Anh & Moffat 2010 | 63.885 | 4.63x | 6.91 | 364 | 1.824 |
| OptPFD (PFOR+yama) | Zukowski 2006 / Yan 2009 | 64.807 | 4.56x | 7.01 | 260 | 1.103 |
| ElBâri (çerçeve, 100) | bu çalışma | 68.844 | 4.30x | 7.45 | 538 | 851 |
| BP128 | Lemire & Boytsov 2015 | 81.130 | 3.64x | 8.78 | 556 | 1.078 |
| VByte (LEB128) | varint temel çizgisi | 93.540 | 3.16x | 10.12 | 2.599 | 1.612 |
| StreamVByte | Lemire & Kurz 2017 | 104.855 | 2.82x | 11.35 | 1.803 | 1.671 |

Tümü tam tur doğrulamasını geçti (kayıpsız).

### Pareto sınırı

![Pareto sınırı — oran, hız](pareto_tamsayi_kodekler.svg)

Kesikli çizgi Pareto cephesini gösterir; cephenin altında kalan her nokta, hem oran hem
hız bakımından kendisinden daha iyi bir nokta olduğu için **baskılanmıştır**.

| Eksen | Cephedeki kodekler |
| --- | --- |
| oran ↔ encode | **ElBâri (kanal)**, VByte |
| oran ↔ decode | **ElBâri (kanal)**, Simple8b |

ElBâri her iki cephede de yer alıyor — ancak yukarıdaki uyarı gereği bu, hız ekseninde
**skaler uygulamalar arasında** geçerli bir sonuçtur.

### Yedi veri setinde konum — gerçek uçuş logu

Tek veri setiyle verilen bir kıyas, veri setinin şeklini kodeğin erdemi sanma riskini
taşır. Ölçüm ALFA uçuş logundan üretilen altı fikstürle tekrarlandı
([`c/veri/`](../c/veri/), ArduPlane 3.9.0beta1, sabit kanat):

| Veri seti | K | Ham B | **ElBâri** | ElBâri enc | Ailenin en iyisi | Fark | Pareto |
| --- | ---: | ---: | ---: | ---: | --- | ---: | :---: |
| GPS (OSM referans) | 3 | 295.704 | **4,95x** | 1.121 | Sprintz 4,67x | **+%6,0** | E+D |
| GPS (ALFA uçuş) | 3 | 164.064 | **5,60x** | 997 | Simple8b 5,43x | **+%3,1** | E+D |
| Titreşim (VIBE) | 3 | 820.056 | 5,38x | 957 | Simple8b 5,51x | −%2,4 | E |
| Yönelim (ATT) | 3 | 820.188 | 14,70x | 1.482 | Sprintz 15,55x | −%5,5 | E |
| IMU (jiro+ivme) | 6 | 3.280.752 | 6,88x | 1.002 | Simple8b 7,43x | −%7,4 | E |
| Servo (RCOU) | 8 | 2.186.848 | 25,64x | 1.834 | Sprintz 34,18x | −%25,0 | E |
| Kumanda (RCIN) | 8 | 2.186.848 | 40,09x | 2.197 | Sprintz 74,39x | **−%46,1** | — |

*Fark = ElBâri'nin, kendisi hariç ailenin en iyi oranına göre farkı. Pareto: E = oran ↔
encode cephesinde, D = oran ↔ decode cephesinde. Encode MB/sn skaler C.*

**Okunuşu üç cümlede:**

1. **Konum verisinde ElBâri lider** — iki GPS setinde de en yüksek oran, üstelik her iki
   Pareto cephesinde birden.
2. **Yönelim, IMU ve titreşimde rekabetçi ama lider değil** — %2–7 geride. Buna karşılık
   oran liderinin **2–2,8 katı encode hızıyla** çalışıyor ve encode cephesinde kalıyor.
3. **Tekrarlı PWM kanallarında (RC girişi/servo çıkışı) açık farkla geride** — ve bu bir
   ayar meselesi değil, aşağıdaki gibi **biçimsel bir tavan.**

#### RCIN bulgusu: biçimin sert tavanı

ElBâri her 8 değer için 4 bitlik bir etiket yazar; sıfır blokta veri biti hiç yazılmaz
ama **etiket yine yazılır.** Yani biçimin taban maliyeti **değer başına 0,5 bit**tir ve
bu, 32 bitlik değerler için oranı **64x'te sabitler** — veri ne kadar sabit olursa olsun.

RCIN'de aritmetik şöyle:

| | |
| --- | ---: |
| Blok sayısı (68.339 kayıt × 8 kanal ÷ 8) | 68.344 |
| Hepsi sıfır blok olsaydı | 34.204 bayt |
| **ElBâri'nin bu veri şeklindeki teorik tavanı** | **63,94x** |
| ElBâri'nin ölçülen değeri | 40,09x (54.543 bayt) |
| **Sprintz-Delta'nın ölçülen değeri** | **74,39x (29.398 bayt)** |

**Sprintz, ElBâri'nin teorik tavanının üstüne çıkıyor.** Sebep: Sprintz sıfır koşularını
bloklar boyunca çalıştırır (run-length), ElBâri ise her bloğa ayrı etiket yazar. Neredeyse
sabit bir kanalda ElBâri'nin çıktısının **%63'ü etikettir.**

Bu, ölçümün en somut mimari bulgusudur ve iki yol açar:
- **Kabul et ve kapsamı daralt** — ElBâri sürekli değişen telemetri (konum, yönelim, IMU)
  için tasarlanmıştır; neredeyse sabit ayrık kanallar hedef değildir.
- **Ya da biçime blok-üstü sıfır koşusu ekle** — sürüm 3 adayı. Tavanı kaldırır ama
  bağımsız çerçeve garantisiyle etkileşimi ayrıca ölçülmelidir.

## 4. Sonuçların dürüst okunması

**1. Katkı gerçek ama küçük: tek haneli yüzde.**

Doğru rakip ailesiyle ölçüldüğünde ElBâri'nin oran üstünlüğü şu:

| Rakip | ElBâri'nin farkı |
| --- | ---: |
| Sprintz-Delta | **+%6,1** |
| Simple8b | +%7,0 |
| OptPFD | +%8,6 |
| BP128 | +%35,9 |
| VByte | +%56,7 |

Yani zstd karşısındaki "üç kat" farkın yerini, gerçek ailede **%6'lık bir fark** aldı.
Bu, beklenen ve kabul edilmesi gereken sonuçtur: ElBâri zaten aynı fikirleri kullanıyor.
%6, kanal başına uyarlanabilir fark derecesi + genişletilmiş bit genişliği tablosu +
sıfır blok kısayolunun birlikte getirdiği kazançtır.

> **Ve bu %6 veri setine bağlıdır.** Yukarıdaki yedi set tablosunda aynı fark GPS'te
> +%3…+%6, yönelim/IMU/titreşimde −%2…−%7, tekrarlı PWM kanallarında −%25…−%46 çıkıyor.
> Tek bir veri setinden okunan "%6 önde" ifadesi **genellenemez**; savunulabilir ifade
> "konum verisinde önde, sürekli telemetride rekabetçi, tekrarlı kanallarda geride"dir.

**2. Çerçeveleme açılınca ElBâri oran liderliğini kaybediyor.**

Bu, bu ölçümün en önemli bulgusudur:

| | Oran |
| --- | ---: |
| ElBâri (kanal, çerçevesiz) | 4,95x |
| Sprintz-Delta | 4,67x |
| **ElBâri (çerçeve, 100 kayıt)** | **4,30x** |

Paket kaybı dayanıklılığı açıldığında ElBâri, Sprintz'in **%8 altına** düşüyor. Yani
projenin asıl ayırt edici özelliği (bağımsız çerçeveler), oran üstünlüğünün tamamını ve
fazlasını yiyor.

Bunun iki sonucu var:
- **İddia yeniden konumlanmalı.** ElBâri'nin savunulabilir üstünlüğü "en yüksek oran"
  değil, **"kayıplı linkte çalışabilen tek aile üyesi"**. Tablodaki diğer yedi kodeğin
  hiçbirinde paket kaybı dayanıklılığı yok; tek paket düşerse akış çözülemez.
- **Çerçeve boyutu optimizasyonu teorik bir merak değil, doğrudan bir gerek.** 4,95 →
  4,30 farkının nerede optimum olduğu §5'te süpürüldü — ve maliyet %13 değil, gerçek
  kısıtlar altında **%43** çıktı.

**3. Kanal ayrımı argümanı saman adam değilmiş.**

Kanal ayrımı olmadan (iç içe geçmiş kayıt akışını tek seri gibi işleyerek):

| Kodek | Oran (kanal ayrımı yok) |
| --- | ---: |
| ElBâri (kanal) | 1,00x — **reddedildi, ham geçiş** |
| BP128 | 1,00x |
| OptPFD | 1,00x |
| Sprintz-Delta | 0,97x |
| StreamVByte | 0,94x |
| VByte | 0,86x |
| **Simple8b** | **0,50x — veriyi ikiye katlıyor** |

**Ailenin tamamı çöküyor**, ElBâri'ye özgü bir zayıflık değil. Simple8b'nin veriyi
büyütmesinin sebebi açık: iç içe geçmiş farklar 30 bitten geniş olduğu için her değer
tek başına 64 bitlik bir kelimeye düşüyor (73.926 × 8 bayt = 591.408).

Bu, README'deki "kanal katmanı olmadan reddediliyor" argümanının **rakipler için de
geçerli** olduğunu gösteriyor — dolayısıyla kanal katmanı bir savunma değil, bu problem
sınıfının zorunlu ön koşulu.

## 5. Çerçeve boyutu — "dayanıklılık ne kadara mal oluyor?"

§4'te çerçevelemenin oran liderliğini yediği görüldü. Peki 100 kayıt/çerçeve doğru
seçim mi? Çerçeve boyutu **bedava seçilen bir parametre değildir**; üç kısıt aynı anda
bağlar:

1. **Oran** — çerçeve küçüldükçe 16 baytlık başlık + CRC yükü ve kaybolan fark bağlamı
   oranı düşürür.
2. **Gecikme** — bir çerçeve **dolana kadar gönderilemez.** 10 Hz telemetride 100
   kayıtlık çerçeve, 10 saniye tamponlama demektir.
3. **Paket boyutu** — çerçeve **tek bir radyo paketine sığmalıdır.** Sığmazsa parçalanır;
   parçalardan biri düşerse çerçevenin tamamı gider ve bağımsızlık iddiası çöker.
   SiK / RFD900 sınıfı radyolarda kullanılabilir yük tipik olarak ~200-250 bayttır.

Ölçüm (gerçek GPS verisi, tümü tam tur doğrulamasını geçti):

| Çerçeve (kayıt) | Çerçeve sayısı | Toplam bayt | Oran | ort. B/çerçeve | **en büyük B** | gecikme @10 Hz | @50 Hz |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| *çerçeveleme yok* | — | 59.695 | **4.95x** | — | — | *akış* | *akış* |
| 1 | — | — | — | — | — | — | — |
| 2 | 12.321 | 596.564 | 0.50x | 48 | 56 | 0,20 s | 0,04 s |
| 5 | 4.929 | 302.147 | 0.98x | 61 | 92 | 0,50 s | 0,10 s |
| 10 | 2.465 | 176.888 | 1.67x | 72 | 106 | 1,00 s | 0,20 s |
| 20 | 1.233 | 117.037 | 2.53x | 95 | 160 | 2,00 s | 0,40 s |
| **25** | 986 | 104.838 | **2.82x** | 106 | **185** | 2,50 s | **0,50 s** |
| 50 | 493 | 79.928 | 3.70x | 162 | 315 | 5,00 s | 1,00 s |
| 100 | 247 | 68.844 | 4.30x | 279 | **572** | 10,00 s | 2,00 s |
| 200 | 124 | 63.321 | 4.67x | 511 | 1.001 | 20,00 s | 4,00 s |
| 500 | 50 | 61.207 | 4.83x | 1.224 | 2.339 | 50,00 s | 10,00 s |
| 1000 | 25 | 59.730 | 4.95x | 2.389 | 4.228 | 100,00 s | 20,00 s |

*(1 kayıt/çerçeve desteklenmiyor: çekirdek kodlayıcı tek elemanlı girdiyi
`ELBARI_SIKISTIRILAMAZ` ile reddediyor.)*

### Bunun anlamı — README'deki 4.30x rakamı ne kadar gerçekçi?

**Sıkıştırma 5 kayıt/çerçevenin altında tamamen kayboluyor.** 2 kayıtta oran 0.50x —
veri **iki katına çıkıyor**, çünkü 16 baytlık çerçeve başlığı 24 baytlık yükten büyük.

**4.30x rakamı, 10 saniyelik tamponlama gerektiriyor** (10 Hz telemetride). Canlı
telemetri için bu kabul edilemez: uçağın 10 saniye önceki konumunu görürsün.

**4.30x rakamı ayrıca tek pakete sığmıyor.** En büyük çerçeve 572 bayt; tipik SiK
radyo yükünün iki katından fazla. Çerçeve parçalanmak zorunda kalırsa,
*"bir paket düşerse yalnızca o çerçeve kaybolur"* garantisi zayıflar — çünkü artık
bir çerçeve birden fazla pakete yayılmıştır.

**Gerçekçi çalışma noktası çok daha küçük.** Hem tek pakete sığan (≤185 B) hem
gecikmesi kabul edilebilir (50 Hz'de 0,5 s) tek satır **25 kayıt/çerçeve** ve orada
oran **2.82x** — README'nin öne çıkardığı 4.95x'in **%57'si**.

> Bu, projenin en önemli ölçülmemiş varsayımıydı: çerçeveleme maliyeti README'de
> "%13" (4.95 → 4.30) olarak geçiyor. Gerçek kısıtlar altında maliyet **%43**
> (4.95 → 2.82). Tezin merkezî sorusu bu tablonun üç ekseni arasındaki ödünleşimdir.

## 6. Bu ölçüm projenin iddialarını nasıl değiştiriyor

| Eski iddia | Ölçümden sonra |
| --- | --- |
| "zstd/LZ4/Brotli'yi üç kat geçiyoruz" | Doğru ama ilgisiz — onlar doğru rakip değil |
| "En yüksek oran bizde" | **Yalnızca konum verisinde ve çerçevesiz modda.** Yedi veri setinin ikisinde lider, üçünde %2–7 geride, ikisinde %25–46 geride |
| "Hem oran hem hız lideri" | Skaler uygulamalar arasında evet; SIMD'li kütüphanelerle test edilmedi. Hız iddiası **kurulmadı** |
| "Kanal ayrımı ayırt edici özelliğimiz" | Değil — ailenin tamamı buna muhtaç |
| "Her tür telemetride çalışır" | **Hayır.** Neredeyse sabit ayrık kanallarda biçimin 64x'lik sert tavanı var; Sprintz o tavanın üstüne çıkıyor |
| Ayırt edici özellik | **Paket kaybı dayanıklılığı** — ailede başka kimsede yok. Oran değil, bu |

## 7. Sınırlar

1. **Tek uçuş, tek platform.** Yedi veri setinin altısı **aynı** ALFA uçuşundan
   (2018-07-30 16-13-40) gelir. Platform sabit kanattır (Carbon Z T-28); yönelimi bir
   çoklu rotordan belirgin biçimde daha düzgündür, dolayısıyla ATT/IMU oranları çoklu
   rotor telemetrisine göre **iyimser** taraftadır. Farklı uçuş ve farklı platformla
   tekrarlanmalıdır.
2. **Rakipler yeniden yazıldı**, yazarlarının kütüphaneleri bağlanmadı (§2 uyarısı).
3. **Sprintz-Delta uygulandı, Sprintz-Delta-Huf değil.** Tam sürüm bit paketlemeden
   sonra Huffman katmanı çalıştırır; oranı bir miktar artırır.
4. **Sprintz'in FIRE öngörücüsü yerine düz fark kullanıldı.**
5. **Sprintz 8/16 bit için tasarlandı, 32 bite uyarlandı.** Bu bir genişletmedir.
6. **OptPFD, FastPFor'un kendisi değil**; aynı ailenin daha eski üyesi. FastPFor,
   istisna sayfalarını bit genişliğine göre gruplayarak birkaç yüzde daha iyi verir.
7. **Çerçeve süpürmesi kayıp içermiyor.** §5 oran, gecikme ve paket boyutunu ölçer;
   **paket kaybı altındaki kurtarma oranı ölçülmedi.** Optimum çerçeve boyutu ancak
   kayıp istatistiği eklendiğinde belirlenebilir.
8. **Radyo MTU'su varsayım.** ~200-250 baytlık kullanılabilir yük, SiK/RFD900 sınıfı
   için tipik değerdir; hedef donanımın gerçek MTU'su ile doğrulanmalıdır.
9. **Masaüstü x64.** Gerçek hedef olan gömülü ARM üzerinde ölçüm hâlâ yok.

## 8. Sıradaki adımlar

1. **Yazarların kütüphanelerini bağla** (FastPFor, streamvbyte, Sprintz) ve SIMD'li
   sürümlerle tekrar ölç. Hız iddiası ancak bundan sonra kurulabilir.
2. **Süpürmeye kayıp eksenini ekle.** §5 üç kısıtı ölçtü (oran, gecikme, paket boyutu);
   dördüncüsü — **patlamalı paket kaybı altında kurtarma oranı** — eksik. Gilbert-Elliott
   kanal modeliyle eklendiğinde çerçeve boyutu için gerçek bir optimum tanımlanabilir.
   Tezin merkezî katkı adayı budur.
3. **Blok-üstü sıfır koşusu (biçim sürüm 3).** RCIN bulgusu biçimin 64x'lik sert
   tavanını gösterdi. Sıfır blokların koşu uzunluğuyla kodlanması bu tavanı kaldırır;
   bağımsız çerçeve garantisiyle etkileşimi ölçülmelidir. **Somut ve ölçülebilir bir
   sonraki adım budur.**
4. **Farklı platform ve uçuş.** Mevcut altı fikstür aynı sabit kanat uçuşundan geliyor;
   çoklu rotor logu tabloyu belirgin biçimde değiştirebilir.
5. **Gömülü ARM üzerinde** aynı tabloyu üret.

---

*Ölçüm: [`c/kiyas/kiyas.c`](../c/kiyas/kiyas.c) · Rakip uygulamalar:
[`kodekler.c`](../c/kiyas/kodekler.c), [`sprintz.c`](../c/kiyas/sprintz.c)*
