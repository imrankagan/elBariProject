#nullable enable

using System;

namespace ElBâri
{
    // =================================================================
    // ELBÂRİ FLOAT KUANTALAMA KATMANI
    // =================================================================
    //
    // Telif Hakkı (c) 2025 İmran Kağan. Tüm Hakları Saklıdır.
    //
    // NEDEN VAR:
    // Çekirdek motor tamsayı (int32) üzerinde çalışır. Gerçek telemetrinin
    // önemli bir kısmı ise ondalıklı (float32) taşınır: yönelim açıları,
    // hız, ivme, batarya gerilimi, quaternion bileşenleri...
    //
    // Bu katman ondalıklı değerleri, istenen HASSASİYETE göre ölçekleyip
    // tamsayıya çevirir. Sonuç mevcut kanal ve çerçeve katmanlarına olduğu
    // gibi verilebilir; BİÇİM DEĞİŞMEZ.
    //
    // -----------------------------------------------------------------
    // ⚠️ BU KATMAN KAYIPLIDIR
    // -----------------------------------------------------------------
    // Kuantalama, sayının seçilen hassasiyetin altındaki kısmını atar.
    // Örneğin ölçek = 1000 ise 0.0014567 değeri 0.001 veya 0.002 olarak
    // geri gelir. Bu, telemetri için genellikle istenen davranıştır:
    // bir yönelim açısını 0.001 radyan (0.06 derece) hassasiyetle taşımak
    // fazlasıyla yeterlidir ve tam float taşımak bant genişliği israfıdır.
    //
    // ANCAK: Tam değerin korunması gereken veriler (ham sensör kaydı,
    // sağlama toplamı, kriptografik malzeme) bu katmandan GEÇİRİLMEMELİDİR.
    //
    // Kayıpsız float sıkıştırma (XOR tabanlı, Gorilla/Chimp ailesi) ayrı
    // bir bit biçimi gerektirir ve bu sürümde YOKTUR.
    // -----------------------------------------------------------------
    //
    // HASSASİYET SEÇİMİ (ölçek = 1 / hassasiyet):
    //
    //   Büyüklük              Tipik hassasiyet     Ölçek
    //   --------------------  -------------------  -------
    //   Yönelim (radyan)      0.001 rad            1000
    //   Yönelim (derece)      0.01 derece          100
    //   Hız (m/s)             0.01 m/s             100
    //   İrtifa (m)            0.01 m (cm)          100
    //   Batarya (V)           0.01 V               100
    //   Quaternion (birimsiz) 0.0001               10000
    //
    // BELİRLENİMCİLİK:
    // Yuvarlama, C sürümüyle BİREBİR AYNI sonucu vermek zorundadır. Bu
    // yüzden hesap çift duyarlıkta (double) yapılır ve yuvarlama açıkça
    // "sıfırdan uzağa" uygulanır. Math.Round kullanılmaz — .NET'in
    // varsayılanı bankacı yuvarlamasıdır (çifte yuvarlar) ve C tarafıyla
    // ayrışırdı.
    // =================================================================
    public static class ElBâriFloat
    {
        /// <summary>Bir çağrıda işlenebilecek en fazla eleman sayısı (C sürümüyle aynı).</summary>
        public const int MAKS_ELEMAN = 200_000_000;

        /// <summary>
        /// Ondalıklı değerleri ölçekleyip tamsayıya çevirir (KAYIPLI).
        /// </summary>
        /// <param name="olcek">1 / istenen_hassasiyet. Örnek: 0.001 hassasiyet → 1000.</param>
        /// <exception cref="ArgumentException">
        /// Taşma, NaN/sonsuz değer ya da geçersiz ölçek durumunda atılır.
        /// Sessizce yanlış değer üretilmez.
        /// </exception>
        public static void Kuantala(scoped ReadOnlySpan<float> girdi,
                                    float olcek,
                                    scoped Span<int> cikti)
        {
            if (cikti.Length < girdi.Length)
            {
                throw new ArgumentException(
                    $"Çıktı dizisi çok küçük. {girdi.Length} eleman gerekli, {cikti.Length} verildi.",
                    nameof(cikti));
            }
            if (girdi.Length > MAKS_ELEMAN)
            {
                throw new ArgumentException(
                    $"Eleman sayısı sınırı aşıldı (en fazla {MAKS_ELEMAN}).", nameof(girdi));
            }
            OlcekDogrula(olcek, nameof(olcek));

            for (int i = 0; i < girdi.Length; i++)
            {
                cikti[i] = TekDegerKuantala(girdi[i], olcek, i);
            }
        }

        /// <summary>Kuantalamanın tersi: tamsayıdan ondalıklıya.</summary>
        public static void Coz(scoped ReadOnlySpan<int> girdi,
                               float olcek,
                               scoped Span<float> cikti)
        {
            if (cikti.Length < girdi.Length)
            {
                throw new ArgumentException(
                    $"Çıktı dizisi çok küçük. {girdi.Length} eleman gerekli, {cikti.Length} verildi.",
                    nameof(cikti));
            }
            OlcekDogrula(olcek, nameof(olcek));

            for (int i = 0; i < girdi.Length; i++)
            {
                cikti[i] = (float)((double)girdi[i] / (double)olcek);
            }
        }

        /// <summary>
        /// Çok kanallı sürüm: her kanalın kendi ölçeği vardır.
        /// Bir yönelim açısı ile batarya gerilimi aynı hassasiyeti gerektirmez.
        /// </summary>
        /// <remarks>
        /// Ölçekler biçim içinde TAŞINMAZ. Gönderici ve alıcı aynı ölçek dizisini
        /// kullanmak zorundadır (telemetri şemasının parçası olarak, bant dışı
        /// anlaşılır). Bu, MAVLink gibi protokollerin çalışma biçimiyle aynıdır.
        /// </remarks>
        public static void KuantalaKanalli(scoped ReadOnlySpan<float> girdi,
                                           int kanalSayisi,
                                           scoped ReadOnlySpan<float> olcekler,
                                           scoped Span<int> cikti)
        {
            KanalliDogrula(girdi.Length, kanalSayisi, olcekler.Length, cikti.Length);

            for (int i = 0; i < girdi.Length; i++)
            {
                cikti[i] = TekDegerKuantala(girdi[i], olcekler[i % kanalSayisi], i);
            }
        }

        /// <summary>Çok kanallı kuantalamanın tersi.</summary>
        public static void CozKanalli(scoped ReadOnlySpan<int> girdi,
                                      int kanalSayisi,
                                      scoped ReadOnlySpan<float> olcekler,
                                      scoped Span<float> cikti)
        {
            KanalliDogrula(girdi.Length, kanalSayisi, olcekler.Length, cikti.Length);

            for (int i = 0; i < girdi.Length; i++)
            {
                cikti[i] = (float)((double)girdi[i] / (double)olcekler[i % kanalSayisi]);
            }
        }

        /// <summary>İstenen hassasiyet için ölçek değeri (1 / hassasiyet).</summary>
        public static float OlcekOner(float hassasiyet)
            => hassasiyet > 0.0f ? 1.0f / hassasiyet : 0.0f;

        /// <summary>
        /// İki dizi arasındaki en büyük mutlak fark — kuantalama hatasının ölçüsü.
        /// </summary>
        public static float MaksHata(scoped ReadOnlySpan<float> orijinal,
                                     scoped ReadOnlySpan<float> geri)
        {
            float maks = 0.0f;
            int n = Math.Min(orijinal.Length, geri.Length);

            for (int i = 0; i < n; i++)
            {
                float fark = Math.Abs(orijinal[i] - geri[i]);
                if (fark > maks) maks = fark;
            }
            return maks;
        }

        // =================================================================
        // İÇ YARDIMCILAR
        // =================================================================

        /// <summary>
        /// Tek bir değeri kuantalar. Yuvarlama açıkça "sıfırdan uzağa" yapılır;
        /// Math.Round kullanılmaz çünkü .NET varsayılanı bankacı yuvarlamasıdır
        /// ve C sürümüyle ayrışırdı.
        /// </summary>
        private static int TekDegerKuantala(float deger, float olcek, int indeks)
        {
            double d = deger;

            // NaN kendisine eşit değildir; sonsuzlar da tamsayıya çevrilemez.
            if (double.IsNaN(d) || double.IsInfinity(d))
            {
                throw new ArgumentException(
                    $"Eleman {indeks}: NaN veya sonsuz değer kuantalanamaz.", nameof(deger));
            }

            double olcekli = d * (double)olcek;
            olcekli = olcekli >= 0.0 ? olcekli + 0.5 : olcekli - 0.5;

            if (olcekli > 2147483647.0 || olcekli < -2147483648.0)
            {
                throw new ArgumentException(
                    $"Eleman {indeks}: {deger} değeri {olcek} ölçeğiyle int32 aralığını aşıyor. " +
                    "Daha küçük bir ölçek kullanın.", nameof(deger));
            }

            return (int)olcekli;
        }

        private static void OlcekDogrula(float olcek, string ad)
        {
            if (!(olcek > 0.0f) || !(olcek < 3.0e38f))
            {
                throw new ArgumentException(
                    $"Ölçek pozitif ve sonlu olmalı. Verilen: {olcek}", ad);
            }
        }

        private static void KanalliDogrula(int elemanSayisi, int kanalSayisi,
                                           int olcekSayisi, int ciktiUzunlugu)
        {
            if (kanalSayisi < 1 || kanalSayisi > ElBâriKanal.MAKS_KANAL)
            {
                throw new ArgumentOutOfRangeException(nameof(kanalSayisi), kanalSayisi,
                    $"Kanal sayısı 1 ile {ElBâriKanal.MAKS_KANAL} arasında olmalı.");
            }
            if (olcekSayisi < kanalSayisi)
            {
                throw new ArgumentException(
                    $"Ölçek dizisi kanal sayısı kadar olmalı. {kanalSayisi} gerekli, {olcekSayisi} verildi.",
                    nameof(olcekSayisi));
            }
            if (ciktiUzunlugu < elemanSayisi)
            {
                throw new ArgumentException(
                    $"Çıktı dizisi çok küçük. {elemanSayisi} eleman gerekli, {ciktiUzunlugu} verildi.",
                    nameof(ciktiUzunlugu));
            }
            if (elemanSayisi > MAKS_ELEMAN)
            {
                throw new ArgumentException(
                    $"Eleman sayısı sınırı aşıldı (en fazla {MAKS_ELEMAN}).", nameof(elemanSayisi));
            }
        }
    }
}
