#!/bin/bash
#SBATCH --job-name=assign2_96_240
#SBATCH -N 2
#SBATCH --ntasks-per-node=48
#SBATCH --output=out_96_240_%j.out
#SBATCH --error=err_96_240_%j.err
#SBATCH --partition=cpu
#SBATCH --time=00:10:00

module load compiler/oneapi-2024/mpi

for i in {1..5}
do
  echo "Run $i"
  mpirun -np 96 ./src 7 48 6 4 4 240 240 240 5 1000 2 500
done

