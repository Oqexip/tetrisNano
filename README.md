# Tetris Arduino Nano dengan matriks LED 8×32

Proyek ini memakai Arduino Nano **klasik** (ATmega328P), modul matriks LED MAX7219 empat panel 8×8 bertipe **FC-16**, joystick analog dengan tombol, dan OLED **SSD1306 128×64 I2C**. Strip 8×32 dipasang **tegak**, sehingga arena Tetris menjadi **8 kolom × 32 baris**. Skor, jumlah baris, dan level muncul di OLED. Versi ini adalah permainan lokal; Arduino Nano klasik tidak menyediakan koneksi Wi-Fi secara bawaan.

## Komponen

- Arduino Nano klasik (ATmega328P), kabel USB data.
- Matriks LED MAX7219 4-in-1 (8×32), sebaiknya tipe FC-16.
- Joystick analog 5 pin: VCC, GND, VRx, VRy, SW.
- Push button sesaat (*normally open*) untuk mulai ulang permainan.
- Buzzer **pasif piezo** (2 pin) atau modul buzzer pasif 3 pin yang menerima sinyal nada. Sketch disetel untuk modul 3 pin **aktif-LOW**: pin S diam saat HIGH. Buzzer aktif yang hanya menghasilkan satu nada tidak cocok untuk melodi ini.
- OLED SSD1306 128×64 **I2C 4 pin** yang dinyatakan kompatibel dengan logika/pasokan 5 V.
- Catu 5 V teregulasi sekitar 2 A untuk matriks, kabel jumper, dan breadboard.

## Wiring

| Komponen | Pin modul | Pin Nano / sumber |
| --- | --- | --- |
| MAX7219 (sisi **IN**) | DIN | D11 (MOSI) |
| MAX7219 (sisi **IN**) | CLK | D13 (SCK) |
| MAX7219 (sisi **IN**) | CS/LOAD | D10 |
| MAX7219 | VCC | +5 V catu matriks |
| MAX7219 | GND | GND catu matriks **dan** GND Nano |
| Joystick | VRx | A0 |
| Joystick | VRy | A1 |
| Joystick | SW | D2 (`INPUT_PULLUP` di kode) |
| Joystick | VCC, GND | 5V, GND Nano |
| Tombol mulai ulang | Dua terminal sakelar | D4 dan GND Nano (`INPUT_PULLUP`) |
| Buzzer pasif 2 pin | +, − | D3, GND Nano |
| Modul buzzer pasif 3 pin | S, VCC, GND | D3, 5V Nano, GND Nano (ikuti label modul) |
| OLED I2C | SDA | A4 |
| OLED I2C | SCL | A5 |
| OLED I2C | VCC, GND | 5V, GND Nano *jika modul OLED memang mendukung 5 V* |

Nyalakan Nano dari USB, matriks dari catu 5 V tersendiri, dan **satukan semua GND**. Jangan menarik arus empat panel matriks dari pin 5V Nano. Jangan sambungkan keluaran +5 V dari catu matriks ke pin 5V Nano saat Nano juga tersambung ke USB. Jika OLED hanya mendukung 3,3 V, gunakan pasokan 3,3 V yang sesuai dan penggeser level I2C dua arah untuk SDA/SCL. Gunakan buzzer piezo berarus kecil; speaker biasa atau buzzer berarus besar memerlukan penggerak transistor.

Jika buzzer 3 pin berdengung terus ketika musik berhenti, periksa apakah pin **S** modul aktif saat LOW. Sketch ini mengatur D3 ke HIGH di sela nada dan setelah *game over* melalui `BUZZER_IDLE_LEVEL = HIGH`. Bila modul Anda justru diam saat LOW, ubah konstanta itu menjadi `LOW`. Jika suara tetap ada saat pin S pada level diam, periksa sambungan GND bersama dan pastikan modulnya buzzer pasif.

Suara buzzer dibangkitkan dengan fungsi Arduino `tone()` pada D3. Fungsi ini mengatur nada, tetapi sketch tidak menyediakan pengaturan volume melalui kode. Jika modul terlalu keras, kurangi suara secara fisik atau gunakan modul dengan pengaturan volume sendiri.

## Memprogram

1. Di Arduino IDE, pasang library **MD_MAX72XX** (MajicDesigns) dan **U8g2** (olikraus) melalui Library Manager. Library `U8x8lib.h` ikut dalam paket U8g2.
2. Buka [`tetrisNano.ino`](tetrisNano.ino). Pilih **Tools → Board → Arduino Nano** dan port yang benar. Untuk clone Nano lama, bila unggahan gagal, coba **Tools → Processor → ATmega328P (Old Bootloader)**.
3. Unggah sketch. Pastikan kabel data masuk ke konektor **IN** matriks, bukan OUT. Posisikan strip tegak dengan arah jatuh balok dari atas ke bawah.
4. Bila joystick kiri/kanan atau atas/bawah terbalik, ubah `INVERT_X` atau `INVERT_Y` menjadi `true` di awal sketch. Bila layar LED terbalik/cermin, ubah `MIRROR_MATRIX_X` atau `MIRROR_MATRIX_Y`.
5. OLED SSD1306 I2C umumnya memakai alamat `0x3C`. Jika modul Anda SH1106, 128×32, SPI, atau beralamat lain, konstruktor OLED pada sketch perlu disesuaikan dengan perangkat sebenarnya.

Jika tombol tekan joystick tidak bereaksi, periksa bahwa **SW benar-benar ke D2** dan **GND joystick tersambung ke GND Nano**. Untuk menguji input tanpa joystick, lepaskan kabel SW dari D2 lalu sentuhkan D2 sebentar ke GND: balok harus langsung *hard drop*, atau permainan mulai ulang saat *game over*. Jika tes D2–GND berhasil, masalah ada pada kabel atau sakelar SW joystick. Lepaskan tombol sebentar sebelum menekan lagi.

Untuk tombol mulai ulang tambahan, hubungkan **satu terminal ke D4 dan satu terminal ke GND**. Tidak perlu resistor eksternal dan jangan hubungkan tombol ini ke 5 V; sketch memakai `INPUT_PULLUP`. Jika tombol memiliki empat kaki, pilih dua kaki yang tidak tersambung secara internal saat tombol tidak ditekan. Tombol D4 mengosongkan papan dan skor serta mengulang musik, baik saat bermain maupun saat *game over*. Tombol joystick D2 tetap untuk *hard drop* dan juga bisa memulai ulang setelah *game over*. Jika hanya ingin me-reset **seluruh Nano**, tombol sesaat antara pin **RST/RESET** dan GND juga bisa dipakai tanpa mengubah kode.

## Cara bermain

| Kontrol | Aksi |
| --- | --- |
| Kiri / kanan | Gerakkan balok, dengan pengulangan saat ditahan |
| Atas | Putar balok sekali setiap dorongan |
| Bawah | *Soft drop* (turun lebih cepat) |
| Tekan | *Hard drop* (langsung jatuh dan mengunci) |
| Tekan setelah game over | Mulai permainan baru |
| Tombol tambahan D4 | Mulai ulang permainan kapan saja |

Gravity mulai pada **700 ms per langkah** dan berkurang **40 ms setiap 500 poin**, sampai **100 ms per langkah**. Soft drop memakai interval **55 ms** dan memberi **1 poin per sel**; hard drop memberi **2 poin per sel**. Menghapus 1/2/3/4 baris sekaligus memberi 100/300/500/800 poin dikali level. Skor tidak disimpan saat listrik mati. Rotasi memakai *wall kick* sederhana, bukan aturan SRS lengkap.

Buzzer memainkan **melodi utama Korobeiniki**: bagian A dua kali, lalu bagian penghubung B, kemudian berulang. Urutan nada disusun dengan acuan [partitur Theme A](https://www.flutetunes.com/tunes/tetris-theme-a-korobeiniki.pdf) dan [transkripsi nada melodi](https://www.piano-keyboard-guide.com/how-to-play-the-tetris-theme-song-easy-piano-tutorial-korobeiniki/). Durasi nada dibuat untuk buzzer monofonik; ini bukan transkripsi penuh aransemen Game Boy. Tempo melodi diatur oleh `MELODY_TICK_MS` (sekarang **208 ms** per not seperdelapan, kira-kira **144 BPM**); besarkan angkanya untuk memperlambat, kecilkan untuk mempercepat. Tempo efek suara tetap terpisah. Efek penghapusan baris menghentikan musik sejenak, lalu musik berlanjut. Saat *game over*, efek penutup turun E5–D5–C5–B4–A4 dan musik berhenti sampai permainan dimulai lagi. Nada dijadwalkan dengan `millis()` tanpa `delay()` agar kontrol dan gravity tetap berjalan.

[`melody-preview.wav`](melody-preview.wav) adalah pratinjau nada satu putaran melodi buzzer (A–A–B) yang dihasilkan dari array nada pada sketch. Keras suara pada modul dapat berbeda dari file audio ini.

[`gameover-preview.wav`](gameover-preview.wav) adalah pratinjau nada efek game over dari array nada pada sketch. Keras suara pada modul dapat berbeda dari file audio ini.

## Sumber teknis

- [Spesifikasi Arduino Nano klasik](https://docs.arduino.cc/resources/datasheets/A000005-datasheet.pdf)
- [API dan tipe modul MD_MAX72XX](https://github.com/MajicDesigns/MD_MAX72XX/blob/main/src/MD_MAX72xx.h)
- [Dokumentasi U8x8](https://github.com/olikraus/u8g2/wiki/u8x8reference)
- [Datasheet MAX7219](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX7219-MAX7221.pdf)
- [Implementasi `tone()` Arduino AVR](https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/Tone.cpp)
