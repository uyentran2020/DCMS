#!/bin/bash
#SBATCH --job-name=RM-U
#SBATCH --partition=small
#SBATCH --ntasks=8
#SBATCH --output=%x.%j.out
#SBATCH --error=%x.%j.err

#./revenue --graph Hept.bin --B_factor 0.1 --alg dcs --w 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.1 --alg dcms --w 2 --alpha 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.1 --alg algo9 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.1 --alg algo10 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.1 --alg edl --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.1 --alg twin_greedy --csv Hept.csv

#./revenue --graph Hept.bin --B_factor 0.15 --alg dcs --w 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.15 --alg dcms --w 2 --alpha 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.15 --alg algo9 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.15 --alg algo10 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.15 --alg edl --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.15 --alg twin_greedy --csv Hept.csv

#./revenue --graph Hept.bin --B_factor 0.2 --alg dcs --w 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.2 --alg dcms --w 2 --alpha 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.2 --alg algo9 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.2 --alg algo10 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.2 --alg edl --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.2 --alg twin_greedy --csv Hept.csv

#./revenue --graph Hept.bin --B_factor 0.25 --alg dcs --w 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.25 --alg dcms --w 2 --alpha 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.25 --alg algo9 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.25 --alg algo10 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.25 --alg edl --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.25 --alg twin_greedy --csv Hept.csv

#./revenue --graph Hept.bin --B_factor 0.3 --alg dcs --w 2 --csv Hept.csv
#./revenue --graph Hept.bin --B_factor 0.3 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.3 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.3 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.3 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.3 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.35 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.35 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.35 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.35 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.35 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.35 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.4 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.4 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.4 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.4 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.4 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.4 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.45 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.45 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.45 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.45 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.45 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.45 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.5 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.5 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.5 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.5 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.5 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.5 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.55 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.55 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.55 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.55 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.55 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.55 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.6 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.6 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.6 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.6 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.6 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.6 --alg twin_greedy --csv Hept.csv

./revenue --graph Hept.bin --B_factor 0.65 --alg dcs --w 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.65 --alg dcms --w 2 --alpha 2 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.65 --alg algo9 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.65 --alg algo10 --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.65 --alg edl --csv Hept.csv
./revenue --graph Hept.bin --B_factor 0.65 --alg twin_greedy --csv Hept.csv


