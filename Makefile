CC=mpicc
CFLAGS=-Wall -Wextra -O2
TARGET=mpi_nilai
SRC=mpi_nilai.c
HOSTFILE=hosts.txt
DATA=data_nilai.csv

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	mpirun --allow-run-as-root --hostfile $(HOSTFILE) -np 5 ./$(TARGET) $(DATA)

run-map: $(TARGET)
	mpirun --allow-run-as-root --display-map --hostfile $(HOSTFILE) -np 5 ./$(TARGET) $(DATA)

clean:
	rm -f $(TARGET)
