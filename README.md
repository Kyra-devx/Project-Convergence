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
| **B.1 Engine foundation** | Scaffold tersedia | Modul Android, JNI bridge, CMake, dan data model voxel |
| **B.2 Face culling + lighting** | Implementasi awal tersedia | Mesher 16³, normals per face, shader GLSL, VAO/VBO, camera MVP, draw call, dan tes native; Android device belum diverifikasi |
| **B.3 Camera + touch** | Direncanakan | Kontrol first-person dan input sentuh |
| **B.4 World generation** | Direncanakan | Terrain noise dan biome |
| **B.5 Chunk streaming** | Direncanakan | Neighbor-aware meshing, chunk loading/unloading |
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
│ JNI Boundary: Asset loading · Surface lifecycle  │
├──────────────────────────────────────────────────┤
│ Native C++ Engine: Chunk data · Face culling     │
├──────────────────────────────────────────────────┤
│ OpenGL ES 3.0: Shader · VAO/VBO · Depth testing  │
├──────────────────────────────────────────────────┤
│ Android Platform: Min SDK 24                     │
└──────────────────────────────────────────────────┘
~~~~

**Stack awal:** Java (Android SDK), C++17 (Android NDK), OpenGL ES 3.0, GLSL ES 3.0, JNI, dan CMake. Vulkan merupakan jalur eksplorasi lanjutan, bukan dependensi prototipe saat ini. Ray tracing tidak diasumsikan tersedia di seluruh perangkat Android.

## Face-culling meshing — Fase B.2

Mesher menghasilkan geometri hanya untuk face blok yang menghadap udara atau berada pada tepi chunk. Face yang berdampingan dengan blok solid di chunk yang sama dihilangkan. Setiap vertex membawa posisi, normal, dan warna; shader fragment menerapkan directional diffuse lighting dengan ambient minimum.

Tes native mencakup chunk kosong, satu blok, dua blok berdampingan, satu chunk solid 16³, dan konsistensi winding terhadap normal.

**Batas saat ini:** neighbor di luar chunk diperlakukan sebagai udara, sehingga face di batas chunk tetap dibuat. Neighbor-aware occlusion akan ditambahkan bersama chunk manager pada fase berikutnya. Persentase face yang tersisa bergantung pada bentuk dan kepadatan world; angka pengurangan bukan jaminan untuk setiap scene.

Untuk chunk 16³ yang seluruhnya solid, naive meshing menghasilkan 49.152 triangle, sedangkan face-culling menghasilkan 3.072 triangle pada enam permukaan luar (6,25% dari jumlah awal). Ini adalah kasus uji solid penuh, bukan klaim performa untuk seluruh terrain.

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
│   └── src/main/
│       ├── java/com/convergence/voxel/
│       ├── cpp/
│       ├── assets/shaders/
│       └── res/
├── engine/
│   ├── include/convergence/
│   ├── src/
│   └── tests/
└── tools/{redstone-test-suite,world-converter}/
~~~~

## Build prototipe Android

Prasyarat: Android Studio dengan dukungan Android Gradle Plugin 8.7.x, JDK 17, Android SDK Platform 35, Android NDK r25+ (konfigurasi awal menargetkan 27.0.12077973), CMake 3.22.1, dan perangkat/emulator dengan OpenGL ES 3.0.

~~~~bash
git clone https://github.com/Kyra-devx/Project-Convergence.git
cd Project-Convergence
~~~~

Buka folder repository di Android Studio, pasang SDK/NDK/CMake yang diminta, lalu lakukan Gradle Sync dan jalankan konfigurasi app. Gradle Wrapper belum disertakan; Android Studio atau instalasi Gradle lokal diperlukan sampai wrapper resmi ditambahkan.

Ketika build dan runtime Android berhasil, demo menampilkan tumpukan voxel bertingkat dengan warna grass/dirt/stone dan directional lighting. Scaffold menyediakan kamera statis untuk framing demo; first-person camera dan touch control masih fase B.3. Android build dan pengujian pada perangkat belum diverifikasi pada commit ini.

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
