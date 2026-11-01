# Contributing to Project Convergence

Terima kasih atas minat Anda untuk berkontribusi! Project Convergence adalah upaya kolaboratif untuk mengeksplorasi dan membangun jembatan antara pendekatan mekanik, simulasi, dan runtime dari ekosistem game voxel. Setiap kontribusi—mulai dari laporan bug, dokumentasi, pengujian, hingga implementasi engine—sangat berarti.

Sebelum mulai, baca [README.md](README.md), [Game Design Document](docs/GDD.md), dan [Technical Architecture](docs/ARCHITECTURE.md) agar perubahan tetap sejalan dengan tujuan proyek.

---

## 📜 Code of Conduct

Kami mengikuti [Contributor Covenant Code of Conduct, versi 2.1](https://www.contributor-covenant.org/version/2/1/code_of_conduct/). Dengan berpartisipasi, Anda diharapkan membantu menjaga komunitas tetap ramah, inklusif, dan bebas dari perilaku tidak pantas.

**Prinsip inti:**

- Hargai perbedaan pendapat.
- Kritik ide dan implementasi, bukan pribadi.
- Terima kesalahan dan belajar dari umpan balik.
- Fokus pada hal yang terbaik bagi proyek dan komunitas.
- Berkomunikasilah secara profesional dan inklusif.

---

## 🚀 Cara Berkontribusi

### 1. Melaporkan Bug

Gunakan **GitHub Issues**. Sertakan informasi yang cukup agar masalah dapat direproduksi.

Template laporan:

```markdown
**Deskripsi Bug:**
[Jelaskan apa yang terjadi]

**Langkah Reproduksi:**
1. Buka atau jalankan '...'
2. Lakukan '...'
3. Amati hasilnya
4. Lihat error yang muncul

**Perilaku yang Diharapkan:**
[Apa yang seharusnya terjadi]

**Perilaku Aktual:**
[Apa yang sebenarnya terjadi]

**Environment:**
- OS: [Android 12, Windows 11, Linux, dll.]
- Device/GPU: [Model perangkat atau GPU]
- Android SDK: [Jika relevan]
- NDK Version: [Jika relevan]
- Commit Hash: [SHA commit]

**Log/Screenshot:**
[Tambahkan log yang sudah disensor dari data pribadi atau screenshot jika tersedia]
```

Jangan sertakan token, password, signing key, data pribadi, atau file dunia yang bersifat privat dalam issue publik. Untuk dugaan kerentanan keamanan, jangan langsung memublikasikan detail eksploitasi; hubungi maintainer melalui kanal privat yang tersedia.

### 2. Mengusulkan Fitur

Buka **GitHub Issue** dan gunakan label `enhancement` jika label tersebut tersedia. Sertakan:

- **Motivasi:** Mengapa fitur ini dibutuhkan?
- **Solusi yang Diusulkan:** Bagaimana cara kerjanya?
- **Alternatif:** Pendekatan lain apa yang sudah dipertimbangkan?
- **Dampak:** Apakah perubahan memengaruhi kompatibilitas mundur, format world, performa, atau API?
- **Validasi:** Bagaimana fitur ini dapat diuji?

Untuk perubahan besar pada arsitektur, format penyimpanan, simulasi, atau sistem kompatibilitas, diskusikan desainnya sebelum mulai mengerjakan implementasi.

### 3. Mengirim Pull Request

Workflow yang disarankan:

```bash
# 1. Fork repository melalui GitHub

# 2. Clone fork Anda
git clone https://github.com/<username>/Project-Convergence.git
cd Project-Convergence

# 3. Tambahkan repository utama sebagai upstream
git remote add upstream https://github.com/Kyra-devx/Project-Convergence.git
git fetch upstream

# 4. Buat branch dari main terbaru
git switch -c feature/redstone-quasi-connectivity upstream/main

# 5. Buat perubahan dan jalankan pemeriksaan yang relevan

# 6. Commit perubahan
git add .
git commit -S -m "feat(redstone): implement quasi-connectivity for pistons"

# 7. Push branch ke fork Anda
git push -u origin feature/redstone-quasi-connectivity

# 8. Buka Pull Request dari fork ke Kyra-devx/Project-Convergence
```

Gunakan branch yang fokus pada satu perubahan. Hindari menggabungkan beberapa fitur besar yang tidak saling berkaitan dalam satu pull request.

#### Konvensi nama branch

- `feature/<nama-fitur>` — Fitur baru.
- `fix/<nama-bug>` — Perbaikan bug.
- `docs/<topik>` — Perubahan dokumentasi.
- `refactor/<area>` — Refactoring tanpa mengubah perilaku yang dimaksud.
- `test/<area>` — Menambah atau memperbaiki pengujian.
- `chore/<area>` — Maintenance, build, atau CI.

#### Konvensi commit message — Conventional Commits

Gunakan format:

```text
<type>(<scope>): <ringkasan dalam bentuk perintah>

[Penjelasan tambahan jika diperlukan]

[Footer, misalnya Closes #42]
```

Type yang digunakan:

| Type | Penggunaan |
|---|---|
| `feat` | Fitur baru |
| `fix` | Perbaikan bug |
| `docs` | Perubahan dokumentasi |
| `style` | Formatting tanpa perubahan perilaku |
| `refactor` | Perubahan struktur kode tanpa fitur atau fix baru |
| `perf` | Optimasi performa |
| `test` | Menambah atau memperbaiki test |
| `chore` | Maintenance, build, dan CI |

Contoh:

```text
feat(redstone): implement quasi-connectivity for pistons

Add QPC evaluation logic to RedstoneEvaluator.cpp.
Pistons can now be activated by powered blocks one unit above them.

Closes #42
```

**Kebijakan tanda tangan commit:** kebijakan audit repository mewajibkan signed commits. Gunakan `git commit -S` setelah mengonfigurasi GPG atau SSH signing di Git, lalu pastikan GitHub menampilkan commit sebagai verified jika metode signing tersebut didukung. Jangan pernah mengunggah private signing key ke repository.

---

## 💻 Coding Standards

### Java — Android SDK

- **Style:** Ikuti [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html).
- **Formatter:** Gunakan `google-java-format` untuk menjaga format konsisten.
- **Gradle integration:** Otomatisasi formatter melalui Gradle belum dikonfigurasi pada scaffold saat ini; jangan menganggap task formatter sudah tersedia.
- **Lint:** Jalankan Android lint setelah Gradle/Android SDK terpasang dan konfigurasi proyek dapat dibangun.
- **Naming:**
  - Class: `PascalCase`, contoh `VoxelRenderer`.
  - Method/variable: `camelCase`, contoh `buildMesh()`.
  - Constant: `UPPER_SNAKE_CASE`, contoh `MAX_CHUNK_SIZE`.

Pisahkan lifecycle dan UI Android dari aturan simulasi. Jangan menjalankan operasi world generation atau meshing yang berat pada UI thread.

### C++ — Android NDK dan Engine

- **Style:** Ikuti [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
- **Formatter:** Gunakan `clang-format` dengan konfigurasi repository di [`.clang-format`](.clang-format).
- **Language:** Gunakan C++17 atau lebih baru sesuai target build.
- **Memory management:**
  - Utamakan RAII dan ownership yang eksplisit.
  - Gunakan `std::unique_ptr` atau `std::shared_ptr` jika kepemilikan dinamis memang diperlukan.
  - Hindari pasangan raw `new`/`delete`; gunakan container dan smart pointer.
  - Batasi alokasi manual pada boundary yang benar-benar memerlukannya, termasuk integrasi JNI.
- **Naming:**
  - Class/type: `PascalCase`, contoh `ChunkManager`.
  - Method/function: `camelCase`, contoh `getNeighborChunk()`.
  - Member variable: `camelCase_`, contoh `chunkSize_`.
  - Constant: `kPascalCase`, contoh `kMaxRenderDistance`.

Dokumentasikan koordinat, ownership thread, lifetime resource, serta urutan update yang memengaruhi determinisme simulasi. Hindari global mutable state yang tidak diperlukan.

### Shader — GLSL

- **Version:** OpenGL ES 3.0, gunakan `#version 300 es`.
- **Naming:**
  - Uniform: `uCamelCase`, contoh `uMVP`.
  - Attribute/input: `aCamelCase`, contoh `aPosition`.
  - Varying/output: `vCamelCase`, contoh `vColor`.
- **Precision:** Deklarasikan precision secara eksplisit pada shader fragment, misalnya `precision mediump float;`.
- Pastikan shader diuji pada perangkat GLES 3.0 yang relevan dan jangan mengasumsikan seluruh GPU memiliki karakteristik performa yang sama.

---

## 🧪 Testing

Pengujian harus relevan dengan area yang diubah. Jangan menyatakan test lulus jika perintahnya belum dijalankan.

### Unit Test — C++

- **Lokasi:** `engine/tests/`.
- **Status framework:** Test scaffold saat ini menggunakan C++ assertions dan CTest. Google Test (`gtest`) belum dikonfigurasi.
- **Jalankan test engine saat CMake tersedia:**

```bash
cmake -S engine -B build/engine -DBUILD_TESTING=ON
cmake --build build/engine
ctest --test-dir build/engine --output-on-failure
```

### Integration Test — Android

- **Framework yang dituju:** JUnit dan Espresso.
- **Lokasi yang direncanakan:** `app/src/androidTest/`.
- **Status:** Dependency dan test instrumentation belum dikonfigurasi pada scaffold awal.
- Jalankan `connectedAndroidTest` setelah test dependency, perangkat/emulator, Android SDK, dan Gradle Wrapper tersedia.

### Redstone Parity Test

- **Lokasi fixture/dokumentasi:** `tools/redstone-test-suite/`.
- **Status:** Test runner dan fixture schema belum diimplementasikan.
- **Target perintah mendatang:** `python3 tools/redstone-test-suite/run_tests.py --full`.
- Jangan jalankan atau melaporkan perintah tersebut sebagai test aktif sebelum file runner tersedia.

### Android build dan lint

Gradle Wrapper belum disertakan pada scaffold saat ini. Setelah Gradle lokal dan Android SDK yang sesuai tersedia, gunakan task Gradle yang benar-benar terdaftar pada proyek. Target build yang dituju adalah:

```bash
gradle :app:assembleDebug
gradle :app:lint
```

Jika Gradle Wrapper sudah ditambahkan, gunakan `./gradlew` sebagai pengganti `gradle`. Periksa hasil build dan lint sebelum mengklaim aplikasi telah berhasil dibangun.

### Checklist sebelum submit PR

```bash
# Format C++ sesuai konfigurasi repository
clang-format -i <file.cpp> <file.hpp>

# Native engine tests
cmake -S engine -B build/engine -DBUILD_TESTING=ON
cmake --build build/engine
ctest --test-dir build/engine --output-on-failure

# Android checks — jalankan jika toolchain tersedia
gradle :app:assembleDebug
gradle :app:lint
```

Test Android instrumentation dan Redstone parity akan ditambahkan ke checklist wajib setelah harness dan dependensinya benar-benar tersedia.

---

## 📖 Dokumentasi

Jika kontribusi menambah atau mengubah fitur, perbarui dokumentasi terkait:

- **API atau arsitektur:** Perbarui `docs/ARCHITECTURE.md`.
- **Perubahan yang terlihat pengguna:** Perbarui `README.md` atau `docs/GDD.md`.
- **Lore/narasi:** Perbarui dokumen yang relevan di `docs/lore/`.
- **Status pekerjaan:** Perbarui `ROADMAP.md` jika milestone atau status berubah.

Format dokumentasi:

- Gunakan Markdown dengan heading yang konsisten.
- Sertakan diagram Mermaid atau ASCII untuk menjelaskan arsitektur kompleks.
- Berikan contoh kode untuk API baru.
- Catat versi referensi saat membahas kompatibilitas mekanik.
- Jelaskan keterbatasan, migrasi, dan breaking changes.

Jangan menyatakan fitur telah selesai atau kompatibel penuh tanpa implementasi dan bukti pengujian yang mendukung.

---

## 🔍 Review Process

Alur review yang dituju:

1. **Automated Checks:** GitHub Actions akan menjalankan build, lint, formatting, dan test setelah workflow tersebut dikonfigurasi.
2. **Code Review:** Target minimal satu approval dari maintainer sebelum merge.
3. **Discussion:** Perubahan desain dan feedback dibahas pada pull request.
4. **Merge:** Merge dilakukan setelah approval, pemeriksaan wajib, dan kebijakan branch terpenuhi.

**Status CI saat ini:** workflow repository yang tersedia mencakup pembuatan SBOM dan SLSA attestation. Workflow lint, formatter, Android build, dan test belum seluruhnya dikonfigurasi. Persyaratan review di atas merupakan proses yang dituju dan tidak berarti seluruh pemeriksaan sudah diberlakukan otomatis.

Perkiraan waktu review berikut hanya panduan, bukan SLA:

- PR kecil (dokumentasi atau typo): sekitar 1–2 hari.
- PR menengah (bug fix atau fitur kecil): sekitar 3–5 hari.
- PR besar (sistem baru atau refactor): sekitar 1–2 minggu.

Waktu aktual bergantung pada kompleksitas perubahan, ketersediaan reviewer, dan hasil pemeriksaan.

---

## 🎯 Area yang Membutuhkan Bantuan

### Prioritas tinggi

- **Redstone Engine (C++):** Merancang dan mengimplementasikan semantik simulasi yang terdokumentasi dan deterministik.
- **Parity Testing:** Menyusun fixture dan test suite untuk memvalidasi perilaku redstone terhadap versi referensi yang ditentukan.
- **Performance Profiling:** Mengidentifikasi bottleneck pada chunk processing, meshing, dan rendering.

### Prioritas menengah

- **Mobile Combat UI:** Merancang UX dan touch controls untuk perangkat Android.
- **Documentation:** Menulis tutorial teknis untuk developer baru.
- **World Converter:** Meneliti dan membangun tooling konversi format world dengan laporan data yang tidak dapat dipertahankan.

### Prioritas rendah

- **Lore Expansion:** Mengembangkan narasi "Anomali Konvergensi".
- **Translation:** Melokalkan dokumentasi ke bahasa lain.
- **Asset Creation:** Membuat placeholder texture dan aset orisinal untuk prototipe.

Semua kontribusi terkait format atau konten game pihak ketiga harus mematuhi lisensi dan ketentuan yang berlaku. Jangan memasukkan aset proprietary atau data hasil ekstraksi tanpa izin.

---

## 💬 Komunikasi

- **GitHub Issues:** Laporan bug dan permintaan fitur.
- **GitHub Discussions:** Diskusi umum, pertanyaan, dan ide jika fitur Discussions diaktifkan pada repository.
- **Discord:** Coming soon; belum tersedia sebagai kanal resmi.

---

## 🙏 Penghargaan

Kontributor dapat dicantumkan di `CONTRIBUTORS.md` dan bagian Acknowledgments pada README setelah mekanisme atribusi tersebut ditambahkan.

Terima kasih telah membantu membangun jembatan antara dua pendekatan dunia voxel.

*Batu demi batu. Tick demi tick.*

*Sunyi yang punya arah.*
