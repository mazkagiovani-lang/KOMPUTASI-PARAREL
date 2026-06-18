# Tugas Komputasi Paralel: Analisis Nilai Mahasiswa dengan OpenMPI

Project ini berisi solusi tugas **Komputasi Paralel Berbasis Cluster dengan OpenMPI**.

## Isi File

- `mpi_nilai.c` — source code C OpenMPI.
- `data_nilai.csv` — contoh data 100 baris dengan kolom `NIM,Nilai`.
- `hosts.txt` — hostfile 3 container dengan total 6 slot, dipakai 5 proses MPI.
- `hostfile.txt` — salinan `hosts.txt` jika ingin memakai nama hostfile.txt.
- `hosts_multi_node_example.txt` — contoh hostfile multi-node 1 master + 4 worker.
- `Makefile` — perintah kompilasi dan run.
- `laporan.md` — draft laporan siap diedit nama anggota dan screenshot.

## Masuk ke container master dulu

Sebelum build atau run, masuk ke container master:

```bash
docker exec -it kelas-b-master bash
```

Lalu masuk ke folder project, contoh:

```bash
cd /workspace/kelompok3
```

## Isi hosts.txt yang digunakan

```text
kelas-b-master slots=2
kelas-b-worker1 slots=2
kelas-b-worker2 slots=2
```

Artinya ada 3 container/node:

- `kelas-b-master` menyediakan 2 slot proses.
- `kelas-b-worker1` menyediakan 2 slot proses.
- `kelas-b-worker2` menyediakan 2 slot proses.

Program tetap dijalankan dengan `-np 5`, jadi total proses/rank MPI yang berjalan adalah **5 proses**, bukan 6. OpenMPI akan mengambil 5 dari total 6 slot yang tersedia.

## Compile / Build

```bash
mpicc -Wall -Wextra -O2 -o mpi_nilai mpi_nilai.c
```

Atau menggunakan Makefile:

```bash
make
```

## Run

Karena container berjalan sebagai `root`, gunakan opsi `--allow-run-as-root`:

```bash
mpirun --allow-run-as-root --hostfile hosts.txt -np 5 ./mpi_nilai data_nilai.csv
```

Atau jika memakai nama `hostfile.txt`:

```bash
mpirun --allow-run-as-root --hostfile hostfile.txt -np 5 ./mpi_nilai data_nilai.csv
```

Dengan Makefile:

```bash
make run
```

## Output tambahan pembagian proses

Program sudah diupdate agar menampilkan rank, role, hostname, dan bagian data yang diproses. Contoh bentuk output:

```text
[INFO] Rank 0/5 (Master) berjalan di host kelas-b-master, memproses data ke-1 s.d. 20 (20 baris)
[INFO] Rank 1/5 (Worker) berjalan di host kelas-b-master, memproses data ke-21 s.d. 40 (20 baris)
[INFO] Rank 2/5 (Worker) berjalan di host kelas-b-worker1, memproses data ke-41 s.d. 60 (20 baris)
[INFO] Rank 3/5 (Worker) berjalan di host kelas-b-worker1, memproses data ke-61 s.d. 80 (20 baris)
[INFO] Rank 4/5 (Worker) berjalan di host kelas-b-worker2, memproses data ke-81 s.d. 100 (20 baris)
```

Hostname/rank bisa sedikit berbeda tergantung mapping OpenMPI, tapi jumlah rank harus tetap 5.

## Output hasil akhir yang diharapkan

```text
===========================================
HASIL ANALISIS NILAI MAHASISWA (MPI)
===========================================
Total Data Diproses : 100 baris
Nilai Tertinggi     : 100.00
Nilai Terendah      : 45.00
Rata-rata Kelas     : 71.66
===========================================
```

## Cek mapping proses dari OpenMPI

Kalau ingin bukti tambahan proses ditempatkan di host mana, jalankan:

```bash
mpirun --allow-run-as-root --display-map --hostfile hosts.txt -np 5 ./mpi_nilai data_nilai.csv
```

atau:

```bash
make run-map
```
