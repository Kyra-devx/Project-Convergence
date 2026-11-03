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
| **B.2 Face culling + lighting** | Implementasi awal tersedia | Mesher CPU, normals, shader, VAO/VBO, dan tes native; volume chunk diperluas menjadi 16×128×16 pada B.6 |
| **B.3 First-person + touch** | Implementasi awal tersedia | Camera GLM, yaw/pitch, delta-time movement, multi-touch, dan virtual joystick; device verification pending |
| **B.4 World generation** | Implementasi prototipe tersedia | FastNoiseLite height/detail noise, seeded terrain, vertical chunk sections, gravity, dan floor collision; biome application, save persistence, serta horizontal streaming belum tersedia |
| **B.5 Chunk streaming** | Implementasi prototipe tersedia | Radius chunk dinamis, culling tetangga, load/unload dan mesh revision; generation masih sinkron hingga B.6 |
| **B.6 Async generation** | Implementasi prototipe tersedia | Worker thread menghasilkan block data; ready queue, mesh budget 2 per frame, dan upload VBO per chunk hanya saat mesh berubah pada GL thread; build/perangkat belum diverifikasi |
| **B.7 Block interaction** | Implementasi sumber tersedia | DDA raycast 5 blok, break/place, invalidasi mesh tetangga, wireframe highlight, dan tombol Android; test/build/perangkat belum diverifikasi |
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

Untuk chunk solid 16×128×16, naive meshing menghasilkan 393.216 triangle, sedangkan face culling menghasilkan 17.408 triangle pada enam permukaan luar. Ini adalah kasus uji solid penuh, bukan klaim performa untuk semua terrain.

Batas saat ini: neighbor yang belum dimuat diperlakukan sebagai udara. Saat chunk tetangga dimuat atau dibongkar, mesh kolom terkait dibangun ulang agar face di batas chunk ikut di-culling.

## Fase B.3 — First-person camera dan touch controls

- Camera native C++ menggunakan GLM untuk position, yaw, pitch, view matrix, dan perspective projection.
- Look input mengubah yaw/pitch; pitch dibatasi agar kamera tidak terbalik.
- Joystick movement memakai delta time, dengan batas maksimum 0,1 detik per frame untuk menghindari lonjakan setelah frame terlambat.
- Input kiri memakai floating virtual joystick; input kanan mengontrol look.
- Satu pointer untuk joystick dan satu pointer untuk look dapat aktif bersamaan.
- Input JNI dilindungi mutex; GL calls tetap berlangsung pada GL thread.
- Gerakan berjalan pada bidang horizontal; pitch kamera tidak membuat joystick mengangkat atau menenggelamkan pemain.
- Overlay menampilkan basis dan knob joystick saat pointer kiri aktif.

Kontrol ini kini mencakup gravity dasar dan floor collision berbasis heightmap. Jump, sprint, touch sensitivity settings, dan gesture customization belum tersedia. Build Android dan pemeriksaan input di perangkat fisik tetap perlu diverifikasi.

## Fase B.7 — Block interaction

- Amanatides-Woo DDA targets loaded world blocks up to 5 world units from the camera.
- Break removes the target block; Place creates a grass block against the hit face when the destination is air, loaded, inside world height, and outside the player's body AABB.
- Edits at chunk edges invalidate the edited chunk and the loaded cardinal neighbor so cross-chunk face culling is rebuilt.
- A black wireframe cube highlights the current target with depth testing enabled.
- Android Break and Place buttons enqueue actions; the GL render thread consumes them and owns every OpenGL call.

The highlight shaders are stored under `app/src/main/assets/shaders/` because the native renderer loads shader text through Android's `AAssetManager`. Block palette, texture atlas, save/load, Android build, CTest, and device verification remain pending.

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
│       │   ├── VoxelDims.h
│       │   ├── Camera.h / Camera.cpp
│       │   ├── WorldGenerator.h / WorldGenerator.cpp
│       │   ├── ChunkManager.h / ChunkManager.cpp
│       │   ├── Raycast.h / Raycast.cpp
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

Saat runtime berhasil, demo memuat terrain procedural 16×128×16 di sekitar pemain dengan seed 12345 dan view distance 4. Satu worker menghasilkan block data; maksimal 6 hasil dikumpulkan dan 2 mesh dibangun per frame. VBO setiap chunk hanya diunggah ulang pada GL thread ketika revision chunk berubah; renderer menggambar chunk yang sudah memiliki mesh secara terpisah. Chunk yang terlalu jauh dibongkar setelah melewati radius dengan hysteresis. Gravity dan floor collision memakai heightmap yang sama. Android build, CTest, dan pengujian perangkat masih belum diverifikasi; biome dan seed persistence belum tersedia.

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
