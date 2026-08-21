# =====================================================================
# ElBari - C surumu derleme dosyasi (Linux / macOS / BSD)
# ---------------------------------------------------------------------
# Windows icin derle.bat kullanin.
#
# Kullanim:
#   make                 tum test programlarini derle
#   make CC=clang        clang ile derle
#   make sanitize        adres + tanimsiz davranis denetleyicileri ile derle
#   make test            derle ve uygunluk + fuzz testlerini calistir
#   make temizle         uretilen dosyalari sil
# =====================================================================

CC ?= cc

# Siki uyari ayarlari. Farkli derleyiciler farkli hatalar yakalar; bu
# yuzden hem GCC hem Clang ile derlenmesi hedeflenir.
UYARI = -Wall -Wextra -Wpedantic -Wshadow -Wcast-qual -Wstrict-prototypes

CFLAGS ?= -std=c17 -O2 $(UYARI)

# Denetleyiciler (sanitizers): calisma aninda tanimsiz davranis ve bellek
# hatalarini yakalar. MISRA belgesindeki "tanimsiz davranis yok" iddiasi
# elle inceleme ile degil, BUNUNLA kanitlanir.
SAN_BAYRAK = -std=c17 -O1 -g -fno-omit-frame-pointer \
             -fsanitize=address,undefined \
             -fno-sanitize-recover=all

KAYNAK = src/elbari.c src/elbari_kanal.c src/elbari_cerceve.c \
         src/elbari_float.c src/elbari_float_xor.c

# Rakip tamsayi kodekleri (olcum kodu; kutuphaneye girmez, MISRA kapsami disi)
KIYAS_KAYNAK = kiyas/onisleme.c kiyas/kodekler.c kiyas/sprintz.c

# Iki kademeli MAVLink vekili (olcum ve referans uygulama)
MAV_KAYNAK = mavlink/mav_bicim.c mavlink/mav_sema.c mavlink/mav_kademe.c \
             mavlink/mav_uretici.c

PROGRAMLAR = dogrulama olcum fuzz uygunluk kapsamli kiyas kiyas-oztest mav_olcum

.PHONY: all sanitize test temizle

all: $(PROGRAMLAR)

dogrulama: test/dogrulama.c $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

olcum: test/olcum.c $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

fuzz: test/fuzz.c $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

uygunluk: test/uygunluk.c $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

kapsamli: test/kapsamli.c $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

# ElBari'yi kendi ailesiyle (Simple8b, BP128, OptPFD, Sprintz, ...) olcer.
# Calistirma: ./kiyas ../testverisi/gercek_gps.bin 200
kiyas: kiyas/kiyas.c $(KIYAS_KAYNAK) $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

# Rakip uygulamalarin kenar durum dogrulamasi. Dis veri istemez.
kiyas-oztest: kiyas/oztest.c $(KIYAS_KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

# Iki kademeli MAVLink vekili olcumu.
# Calistirma: ./mav_olcum ../testverisi/gercek_gps.bin 300
mav_olcum: mavlink/mav_olcum.c $(MAV_KAYNAK) $(KAYNAK)
	$(CC) $(CFLAGS) -o $@ $^

# Denetleyicili surumler ayri isimle uretilir; normal surumlerin
# uzerine yazilmaz.
sanitize: uygunluk-san fuzz-san

uygunluk-san: test/uygunluk.c $(KAYNAK)
	$(CC) $(SAN_BAYRAK) -o $@ $^

fuzz-san: test/fuzz.c $(KAYNAK)
	$(CC) $(SAN_BAYRAK) -o $@ $^

# Uygunluk vektorleri depoda oldugu icin bu test bagimsiz calisir.
# Fuzz de dis veri istemez.
test: uygunluk fuzz kiyas-oztest
	./uygunluk ../testverisi/vektorler.txt
	./fuzz 200000
	./kiyas-oztest

temizle:
	rm -f $(PROGRAMLAR) uygunluk-san fuzz-san *.o
