#!/bin/bash
#SBATCH --job-name=assign2_48_120
#SBATCH -N 1
#SBATCH --ntasks-per-node=48
#SBATCH --output=out_48_120_%j.out
#SBATCH --error=err_48_120_%j.err
#SBATCH --partition=cpu
#SBATCH --time=00:10:00

module load compiler/oneapi-2024/mpi

for i in {1..5}
do
  echo "Run $i"
  mpirun -np 48 ./src 7 48 6 4 2 120 120 120 5 1000 2 500
done

