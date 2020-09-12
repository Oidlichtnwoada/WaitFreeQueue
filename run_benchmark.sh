#!/bin/bash

make clean
make

OUTFILE=benchmark.csv
touch $OUTFILE

CORES=1
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 1 -w -l -g >> $OUTFILE

CORES=2
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 1 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 2 -w -l -g >> $OUTFILE

CORES=4
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 1 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 2 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 3 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 4 -w -l -g >> $OUTFILE

CORES=8
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 2 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 4 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 6 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 8 -w -l -g >> $OUTFILE

CORES=16
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 4 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 8 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 12 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 16 -w -l -g >> $OUTFILE

CORES=32
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 8 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 16 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 24 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 32 -w -l -g >> $OUTFILE

CORES=64
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 0 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 16 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 32 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 48 -w -l -g >> $OUTFILE
srun -p q_student -N 1 --ntasks=1 --cpus-per-task=$CORES --time=5:00 Benchmark -t $CORES -e 64 -w -l -g >> $OUTFILE
