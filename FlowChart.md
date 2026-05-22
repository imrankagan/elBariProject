PROGRAM BAŞLAR
	  ↓
   VERI GELİR (int[] data)
	  ↓
   ┌─────────────────────┐
   │ CPU'yu kontrol et   │
   └─────────────────────┘
			↓
	 ┌──────┴──────-────────-┐
	 ↓             ↓		 ↓
[Intel?]      [ARM?]      [Eski?]
	 ↓             ↓          ↓
  AVX2         NEON      Scalar
  (en hızlı)   (orta)    (yavaş)
	 ↓             ↓          ↓
	 └──────┬──────┴──────────┘
			↓
   Sıkıştırılmış veri çıkar
	  ↓
   PROGRAM BİTER