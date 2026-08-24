#!/bin/bash
#SBATCH --job-name=assign2_32_240
#SBATCH -N 1
#SBATCH --ntasks-per-node=32
#SBATCH --output=out_32_240_%j.out
#SBATCH --error=err_32_240_%j.err
#SBATCH --partition=cpu
#SBATCH --time=00:10:00

module load compiler/oneapi-2024/mpi

for i in {1..5}
do
  echo "Run $i"
  mpirun -np 32 ./src 7 32 4 4 2 240 240 240 5 1000 2 500
done
