#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <errno.h>
#include <unistd.h>

#define REQUIRED_PROCS 5
#define LINE_SIZE 1024

static void die_rank0(const char *message, int rank) {
    if (rank == 0) fprintf(stderr, "Error: %s\n", message);
    MPI_Abort(MPI_COMM_WORLD, 1);
}

static int read_csv_values(const char *filename, double **out_values) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;

    int capacity = 128;
    int count = 0;
    double *values = (double *)malloc((size_t)capacity * sizeof(double));
    if (!values) {
        fclose(fp);
        return -2;
    }

    char line[LINE_SIZE];
    int line_no = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_no++;

        // Lewati baris kosong
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;

        // Lewati header, contoh: NIM,Nilai
        if (line_no == 1 && (strstr(line, "Nilai") != NULL || strstr(line, "nilai") != NULL)) {
            continue;
        }

        char *comma = strchr(line, ',');
        if (!comma) {
            fprintf(stderr, "Peringatan: baris %d dilewati karena tidak ada koma: %s", line_no, line);
            continue;
        }

        errno = 0;
        char *endptr = NULL;
        double nilai = strtod(comma + 1, &endptr);
        if (errno != 0 || endptr == comma + 1) {
            fprintf(stderr, "Peringatan: baris %d dilewati karena nilai tidak valid: %s", line_no, line);
            continue;
        }

        if (count >= capacity) {
            capacity *= 2;
            double *tmp = (double *)realloc(values, (size_t)capacity * sizeof(double));
            if (!tmp) {
                free(values);
                fclose(fp);
                return -2;
            }
            values = tmp;
        }
        values[count++] = nilai;
    }

    fclose(fp);
    *out_values = values;
    return count;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "unknown-host");
    }
    hostname[sizeof(hostname) - 1] = '\0';

    if (size != REQUIRED_PROCS) {
        if (rank == 0) {
            fprintf(stderr, "Program wajib dijalankan dengan -np %d. Contoh:\n", REQUIRED_PROCS);
            fprintf(stderr, "mpirun --hostfile hosts.txt -np %d ./mpi_nilai data_nilai.csv\n", REQUIRED_PROCS);
        }
        MPI_Finalize();
        return 1;
    }

    const char *filename = (argc >= 2) ? argv[1] : "data_nilai.csv";

    double *all_values = NULL;
    int total_count = 0;

    // Rank 0 bertindak sebagai Master: membaca CSV dan menyiapkan data.
    if (rank == 0) {
        total_count = read_csv_values(filename, &all_values);
        if (total_count < 0) {
            char msg[512];
            snprintf(msg, sizeof(msg), "gagal membaca file '%s'. Pastikan file ada dan formatnya NIM,Nilai", filename);
            die_rank0(msg, rank);
        }
        if (total_count == 0) {
            free(all_values);
            die_rank0("file CSV tidak memiliki data nilai yang valid", rank);
        }
    }

    // Semua rank perlu tahu jumlah data agar bisa menghitung ukuran potongan masing-masing.
    MPI_Bcast(&total_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int *sendcounts = (int *)malloc((size_t)size * sizeof(int));
    int *displs = (int *)malloc((size_t)size * sizeof(int));
    if (!sendcounts || !displs) die_rank0("alokasi memori gagal", rank);

    int base = total_count / size;
    int remainder = total_count % size;
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base + (i < remainder ? 1 : 0); // pembagian adil
        displs[i] = offset;
        offset += sendcounts[i];
    }

    int local_count = sendcounts[rank];
    double *local_values = NULL;
    if (local_count > 0) {
        local_values = (double *)malloc((size_t)local_count * sizeof(double));
        if (!local_values) die_rank0("alokasi local_values gagal", rank);
    }

    // Master menyebarkan potongan array nilai ke semua proses, termasuk dirinya sendiri.
    MPI_Scatterv(all_values, sendcounts, displs, MPI_DOUBLE,
                 local_values, local_count, MPI_DOUBLE,
                 0, MPI_COMM_WORLD);

    // Setiap rank menghitung hasil lokal.
    double local_sum = 0.0;
    double local_min = DBL_MAX;
    double local_max = -DBL_MAX;

    for (int i = 0; i < local_count; i++) {
        double x = local_values[i];
        local_sum += x;
        if (x < local_min) local_min = x;
        if (x > local_max) local_max = x;
    }

    // Tampilkan pembagian kerja tiap rank secara berurutan agar mudah di-screenshot.
    // Baris data dihitung setelah header CSV, jadi data ke-1 berarti baris nilai pertama.
    for (int r = 0; r < size; r++) {
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == r) {
            int start_data = displs[rank] + 1;
            int end_data = displs[rank] + local_count;
            const char *role = (rank == 0) ? "Master" : "Worker";
            printf("[INFO] Rank %d/%d (%s) berjalan di host %s, memproses data ke-%d s.d. %d (%d baris)\n",
                   rank, size, role, hostname, start_data, end_data, local_count);
            fflush(stdout);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);

    // Gabungkan hasil lokal menjadi hasil global di Rank 0.
    double global_sum = 0.0;
    double global_min = 0.0;
    double global_max = 0.0;

    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_min, &global_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_max, &global_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double average = global_sum / total_count;
        printf("===========================================\n");
        printf("HASIL ANALISIS NILAI MAHASISWA (MPI)\n");
        printf("===========================================\n");
        printf("Total Data Diproses : %d baris\n", total_count);
        printf("Nilai Tertinggi     : %.2f\n", global_max);
        printf("Nilai Terendah      : %.2f\n", global_min);
        printf("Rata-rata Kelas     : %.2f\n", average);
        printf("===========================================\n");
    }

    free(local_values);
    free(sendcounts);
    free(displs);
    free(all_values);

    MPI_Finalize();
    return 0;
}
