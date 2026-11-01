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
| **B.2 Face culling + lighting** | Implementasi awal tersedia | Mesher 16³, normals, shader, VAO/VBO, dan tes native |
| **B.3 First-person + touch** | Implementasi awal tersedia | Camera GLM, yaw/pitch, delta-time movement, multi-touch, dan virtual joystick; device verification pending |
| **B.4 World generation** | Implementasi prototipe tersedia | FastNoiseLite height/detail noise, seeded terrain, vertical chunk sections, gravity, dan floor collision; biome application, save persistence, serta horizontal streaming belum tersedia |
| **B.5 Chunk streaming** | Direncanakan | Neighbor-aware meshing, chunk loading/unloading |
| **C. Redstone Engine** | Direncanakan | Scheduler deterministik, update order, dan test suite |
| **D. Combat System** | Direncanakan | Timing, shield, damage model, dan kontrol sentuh |
| **E. Compatibility Layer** | Riset | Parser serta adapter berbasis subset; bukan janji menjalankan semua mod Java |
| **F. Polish & Community** | Direncanakan | Dokumentasi, automated testing, beta, dan umpan balik |

Lihat [ROADMAP.md](ROADMAP.md) untuk status yang lebih rinci.

## Arsitektur tingkat tinggi

~~~~text
┌────────────────────────────────────────────────────┐
│ Android Activity + TouchControlsOverlay             │
│ Left: virtual joystick · Right: look                │
├────────────────────────────────────────────────────┤
│ VoxelGLSurfaceView + VoxelRenderer (GL thread)      │
│ Frame clock · EGL lifecycle · Native render calls  │
├────────────────────────────────────────────────────┤
│ JNI Input Boundary                                  │
│ Mutex-protected look delta and joystick state       │
├────────────────────────────────────────────────────┤
│ C++ Camera (GLM) + Chunk Mesher                     │
│ Position · yaw/pitch · view/projection · delta time │
├────────────────────────────────────────────────────┤
│ OpenGL ES 3.0: GLSL · VAO/VBO · Depth/Culling       │
└────────────────────────────────────────────────────┘
~~~~

**Stack:** Java Android SDK, C++17 Android NDK, GLM 1.0.1 (header-only, pinned revision), OpenGL ES 3.0, GLSL ES 3.0, JNI, dan CMake. Vulkan merupakan jalur eksplorasi lanjutan, bukan dependensi prototipe saat ini.

## Fase B.2 — Face culling dan basic lighting

Mesher menghasilkan geometri hanya untuk face blok yang menghadap udara atau berada pada tepi chunk. Face di antara blok solid dalam chunk yang sama dihilangkan. Setiap vertex membawa posisi, normal, dan RGB color.

Untuk chunk solid 16³, naive meshing menghasilkan 49.152 triangle, sedangkan face culling menghasilkan 3.072 triangle pada enam permukaan luar. Ini adalah kasus uji solid penuh, bukan klaim performa untuk semua terrain.

Batas saat ini: neighbor di luar chunk diperlakukan sebagai udara. Neighbor-aware occlusion lintas chunk menunggu chunk manager.

## Fase B.3 — First-person camera dan touch controls

- Camera native C++ menggunakan GLM untuk position, yaw, pitch, view matrix, dan perspective projection.
- Look input mengubah yaw/pitch; pitch dibatasi agar kamera tidak terbalik.
- Joystick movement memakai delta time, dengan batas maksimum 0,1 detik per frame untuk menghindari lonjakan setelah frame terlambat.
- Input kiri memakai floating virtual joystick; input kanan mengontrol look.
- Satu pointer untuk joystick dan satu pointer untuk look dapat aktif bersamaan.
- Input JNI dilindungi mutex; GL calls tetap berlangsung pada GL thread.
- Gerakan berjalan pada bidang horizontal; pitch kamera tidak membuat joystick mengangkat atau menenggelamkan pemain.
- Overlay menampilkan basis dan knob joystick saat pointer kiri aktif.

Kontrol ini belum mencakup collision, gravity, jump, sprint, touch sensitivity settings, atau gesture customization. Build Android dan pemeriksaan input di perangkat fisik tetap perlu diverifikasi.

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
│       │   ├── MainActivity.java
│       │   ├── VoxelRenderer.java
│       │   ├── VoxelGLSurfaceView.java
│       │   └── TouchControlsOverlay.java
│       ├── cpp/
│       │   ├── Camera.h
│       │   ├── Camera.cpp
│       │   └── native-lib.cpp
│       ├── assets/shaders/
│       └── res/
├── engine/{include,src,tests}/
└── tools/{redstone-test-suite,world-converter}/
~~~~

## Build prototipe Android

Prasyarat: Android Studio dengan dukungan Android Gradle Plugin 8.7.x, JDK 17, Android SDK Platform 35, Android NDK r25+ (konfigurasi menargetkan 27.0.12077973), dan CMake 3.22.1. GLM 1.0.1 diambil oleh CMake FetchContent dari upstream pada konfigurasi pertama; koneksi jaringan diperlukan kecuali source dependency sudah tersedia di cache.

~~~~bash
git clone https://github.com/Kyra-devx/Project-Convergence.git
cd Project-Convergence
~~~~

Buka folder repository di Android Studio, pasang SDK/NDK/CMake yang diminta, lalu lakukan Gradle Sync dan jalankan konfigurasi app. Gradle Wrapper belum disertakan; Android Studio atau instalasi Gradle lokal diperlukan sampai wrapper resmi ditambahkan.

Saat runtime berhasil, demo menampilkan terrain berbukit hasil seed 12345 pada satu kolom chunk X/Z (16 × 16) dengan beberapa section vertikal. Seret di kanan untuk mengubah arah pandang, dan gunakan joystick kiri untuk bergerak. Kamera mendapat gravity dan floor collision berbasis heightmap. Horizontal chunk streaming, biome, save persistence, Android build, dan pengujian perangkat masih belum diverifikasi.

## Dokumentasi

- [Game Design Document](docs/GDD.md)
- [Arsitektur Teknis](docs/ARCHITECTURE.md)
- [Lore: Anomali Konvergensi](docs/lore/ANOMALI-KONVERGENSI.md)
- [Roadmap](ROADMAP.md)
- [Panduan Kontribusi](CONTRIBUTING.md)

## Area kontribusi

Prioritas awal: verifikasi Android build dan perangkat, collision/physics, chunk meshing lintas batas, world generation, redstone deterministik, dan peningkatan input mobile.

## Lisensi

Kode proyek ini menggunakan [MIT License](LICENSE), kecuali jika direktori atau aset menyatakan lisensi berbeda. Lisensi MIT tidak memberikan hak atas merek atau aset pihak ketiga.

## Referensi inspirasi

- [Luanti](https://github.com/luanti-org/luanti) — referensi arsitektur voxel open-source.
- [Craft](https://github.com/fogleman/Craft) — contoh implementasi voxel minimal.
- [GeyserMC](https://github.com/GeyserMC/Geyser) — referensi interoperabilitas protokol.

*Batu demi batu. Tick demi tick.*

*Sunyi yang punya arah.*
