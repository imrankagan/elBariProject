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

PROGRAMLAR = dogrulama olcum fuzz uygunluk kapsamli

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

# Denetleyicili surumler ayri isimle uretilir; normal surumlerin
# uzerine yazilmaz.
sanitize: uygunluk-san fuzz-san

uygunluk-san: test/uygunluk.c $(KAYNAK)
	$(CC) $(SAN_BAYRAK) -o $@ $^

fuzz-san: test/fuzz.c $(KAYNAK)
	$(CC) $(SAN_BAYRAK) -o $@ $^

# Uygunluk vektorleri depoda oldugu icin bu test bagimsiz calisir.
# Fuzz de dis veri istemez.
test: uygunluk fuzz
	./uygunluk ../testverisi/vektorler.txt
	./fuzz 200000

temizle:
	rm -f $(PROGRAMLAR) uygunluk-san fuzz-san *.o
