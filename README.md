# Project Convergence

> *Kedalaman Java. Kecepatan Bedrock. Satu dunia.*

**Project Convergence** adalah dokumen desain dan prototipe teknis untuk mengeksplorasi pertanyaan: jika sebuah game voxel lintas platform dibangun dari nol hari ini, dengan pelajaran dari ekosistem Java dan Bedrock, seperti apa arsitektur idealnya?

Proyek ini menggabungkan eksplorasi mekanik, rancangan sistem, dan implementasi engine voxel orisinal. Target awalnya Android dengan UI Java dan runtime native C++.

## Disclaimer

Project Convergence adalah proyek independen dan **bukan** produk resmi Mojang Studios atau Microsoft. Proyek ini tidak berafiliasi dengan, didukung oleh, atau dihubungkan dengan Mojang Studios maupun Microsoft.

Nama Minecraft, Java Edition, dan Bedrock Edition digunakan hanya untuk menjelaskan konteks kompatibilitas dan inspirasi mekanik. Proyek ini tidak menyertakan aset, kode, merek, atau resource proprietary milik pihak tersebut. Seluruh implementasi dan aset harus dibuat secara orisinal atau digunakan berdasarkan lisensi yang sesuai.

## Visi

Mengeksplorasi arsitektur voxel yang memadukan kedalaman mekanik, runtime native yang terukur, dukungan lintas perangkat, serta ekosistem konten terbuka.

| Pilar | Fondasi | Arah pengembangan |
|---|---|---|
| **Mesin** | C++ native runtime | Profiling, debug tools, pipeline asset |
| **Mekanik** | Simulasi deterministik | Tick scheduler dan optimasi tanpa mengubah hasil |
| **Ekosistem** | Add-on dan format terbuka | Parser datapack-style dan adapter berbasis subset |

## Status proyek

| Fase | Status | Ringkasan |
|---|---|---|
| **A. Game Design Document** | Draft v0.1 tersedia | Visi, mekanik inti, combat, hardcore, dan world generation didokumentasikan |
| **B. Voxel Engine Prototype** | Scaffold awal | Modul Android, GL ES 3.0 surface, JNI bridge, dan pustaka data voxel awal; belum diverifikasi pada perangkat |
| **C. Redstone Engine** | Direncanakan | Scheduler deterministik, update order, dan test suite |
| **D. Combat System** | Direncanakan | Timing, shield, damage model, dan kontrol sentuh |
| **E. Compatibility Layer** | Riset | Parser serta adapter berbasis subset; bukan janji menjalankan semua mod Java |
| **F. Polish & Community** | Direncanakan | Dokumentasi, automated testing, beta, dan umpan balik |

Lihat [ROADMAP.md](ROADMAP.md) untuk status yang lebih rinci.

## Arsitektur tingkat tinggi

~~~~text
┌──────────────────────────────────────────────────┐
│ Android Application: UI · Lifecycle · Touch      │
├──────────────────────────────────────────────────┤
│ JNI Boundary: Validation · Ownership · Threading │
├──────────────────────────────────────────────────┤
│ Native C++ Runtime: World · Simulation · Combat  │
├──────────────────────────────────────────────────┤
│ Rendering: OpenGL ES 3.0 baseline · Vulkan later │
├──────────────────────────────────────────────────┤
│ Android Platform: Min SDK 24                     │
└──────────────────────────────────────────────────┘
~~~~

**Stack awal:** Java (Android SDK), C++ (Android NDK), OpenGL ES 3.0, JNI, dan CMake. Vulkan merupakan jalur eksplorasi lanjutan, bukan dependensi prototipe saat ini. Ray tracing tidak diasumsikan tersedia di seluruh perangkat Android.

## Struktur repository

~~~~text
.
├── README.md · LICENSE · .gitignore · ROADMAP.md · CONTRIBUTING.md
├── docs/
│   ├── GDD.md
│   ├── ARCHITECTURE.md
│   └── lore/ANOMALI-KONVERGENSI.md
├── app/
│   ├── build.gradle
│   └── src/main/{java,cpp,res}/
├── engine/{include,src}/
└── tools/{redstone-test-suite,world-converter}/
~~~~

## Build prototipe Android

Prasyarat: Android Studio dengan dukungan Android Gradle Plugin 8.7.x, JDK 17, Android SDK Platform 35, Android NDK r25+ (konfigurasi awal menargetkan 27.0.12077973), CMake 3.22.1, dan perangkat/emulator dengan OpenGL ES 3.0.

~~~~bash
git clone https://github.com/Kyra-devx/Project-Convergence.git
cd Project-Convergence
~~~~

Buka folder repository di Android Studio, pasang SDK/NDK/CMake yang diminta, lalu lakukan Gradle Sync dan jalankan konfigurasi app. Gradle Wrapper belum disertakan; Android Studio atau instalasi Gradle lokal diperlukan sampai wrapper resmi ditambahkan.

**Batas prototipe saat ini:** scaffold native hanya membuat GL ES 3.0 context dan membersihkan frame. Belum ada renderer blok, chunk meshing, world generation, atau gameplay loop.

## Dokumentasi

- [Game Design Document](docs/GDD.md)
- [Arsitektur Teknis](docs/ARCHITECTURE.md)
- [Lore: Anomali Konvergensi](docs/lore/ANOMALI-KONVERGENSI.md)
- [Roadmap](ROADMAP.md)
- [Panduan Kontribusi](CONTRIBUTING.md)

## Area kontribusi

Prioritas awal: verifikasi build Android, chunk meshing dan face culling, world generation dan penyimpanan chunk, spesifikasi redstone deterministik, serta desain kontrol mobile.

## Lisensi

Kode proyek ini menggunakan [MIT License](LICENSE), kecuali jika direktori atau aset menyatakan lisensi berbeda. Lisensi MIT tidak memberikan hak atas merek atau aset pihak ketiga.

## Referensi inspirasi

- [Luanti](https://github.com/luanti-org/luanti) — referensi arsitektur voxel open-source.
- [Craft](https://github.com/fogleman/Craft) — contoh implementasi voxel minimal.
- [GeyserMC](https://github.com/GeyserMC/Geyser) — referensi interoperabilitas protokol.

*Batu demi batu. Tick demi tick.*

*Sunyi yang punya arah.*
