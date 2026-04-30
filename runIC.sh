#!/bin/bash
#SBATCH --job-name=IC-U
#SBATCH --partition=gpu
#SBATCH --ntasks=32
#SBATCH --output=%x.%j.out
#SBATCH --error=%x.%j.err

./ic --graph email.bin --B_factor 0.01 --alg dcs --w 2 --csv email.csv
./ic --graph email.bin --B_factor 0.01 --alg dcms --w 2 --alpha 2 --csv email.csv
./ic --graph email.bin --B_factor 0.01 --alg algo9 --csv email.csv
./ic --graph email.bin --B_factor 0.01 --alg algo10 --csv email.csv
./ic --graph email.bin --B_factor 0.01 --alg edl --csv email.csv
./ic --graph email.bin --B_factor 0.01 --alg twin_greedy --csv email.csv

./ic --graph email.bin --B_factor 0.02 --alg dcs --w 2 --csv email.csv
./ic --graph email.bin --B_factor 0.02 --alg dcms --w 2 --alpha 2 --csv email.csv
./ic --graph email.bin --B_factor 0.02 --alg algo9 --csv email.csv
./ic --graph email.bin --B_factor 0.02 --alg algo10 --csv email.csv
./ic --graph email.bin --B_factor 0.02 --alg edl --csv email.csv
./ic --graph email.bin --B_factor 0.02 --alg twin_greedy --csv email.csv

./ic --graph email.bin --B_factor 0.03 --alg dcs --w 2 --csv email.csv
./ic --graph email.bin --B_factor 0.03 --alg dcms --w 2 --alpha 2 --csv email.csv
./ic --graph email.bin --B_factor 0.03 --alg algo9 --csv email.csv
./ic --graph email.bin --B_factor 0.03 --alg algo10 --csv email.csv
./ic --graph email.bin --B_factor 0.03 --alg edl --csv email.csv
./ic --graph email.bin --B_factor 0.03 --alg twin_greedy --csv email.csv

./ic --graph email.bin --B_factor 0.04 --alg dcs --w 2 --csv email.csv
./ic --graph email.bin --B_factor 0.04 --alg dcms --w 2 --alpha 2 --csv email.csv
./ic --graph email.bin --B_factor 0.04 --alg algo9 --csv email.csv
./ic --graph email.bin --B_factor 0.04 --alg algo10 --csv email.csv
./ic --graph email.bin --B_factor 0.04 --alg edl --csv email.csv
./ic --graph email.bin --B_factor 0.04 --alg twin_greedy --csv email.csv

./ic --graph email.bin --B_factor 0.05 --alg dcs --w 2 --csv email.csv
./ic --graph email.bin --B_factor 0.05 --alg dcms --w 2 --alpha 2 --csv email.csv
./ic --graph email.bin --B_factor 0.05 --alg algo9 --csv email.csv
./ic --graph email.bin --B_factor 0.05 --alg algo10 --csv email.csv
./ic --graph email.bin --B_factor 0.05 --alg edl --csv email.csv
./ic --graph email.bin --B_factor 0.05 --alg twin_greedy --csv email.csv

./ic --graph fb.bin --B_factor 0.01 --alg dcs --w 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.01 --alg dcms --w 2 --alpha 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.01 --alg algo9 --csv fb.csv
./ic --graph fb.bin --B_factor 0.01 --alg algo10 --csv fb.csv
./ic --graph fb.bin --B_factor 0.01 --alg edl --csv fb.csv
./ic --graph fb.bin --B_factor 0.01 --alg twin_greedy --csv fb.csv

./ic --graph fb.bin --B_factor 0.02 --alg dcs --w 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.02 --alg dcms --w 2 --alpha 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.02 --alg algo9 --csv fb.csv
./ic --graph fb.bin --B_factor 0.02 --alg algo10 --csv fb.csv
./ic --graph fb.bin --B_factor 0.02 --alg edl --csv fb.csv
./ic --graph fb.bin --B_factor 0.02 --alg twin_greedy --csv fb.csv

./ic --graph fb.bin --B_factor 0.03 --alg dcs --w 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.03 --alg dcms --w 2 --alpha 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.03 --alg algo9 --csv fb.csv
./ic --graph fb.bin --B_factor 0.03 --alg algo10 --csv fb.csv
./ic --graph fb.bin --B_factor 0.03 --alg edl --csv fb.csv
./ic --graph fb.bin --B_factor 0.03 --alg twin_greedy --csv fb.csv

./ic --graph fb.bin --B_factor 0.04 --alg dcs --w 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.04 --alg dcms --w 2 --alpha 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.04 --alg algo9 --csv fb.csv
./ic --graph fb.bin --B_factor 0.04 --alg algo10 --csv fb.csv
./ic --graph fb.bin --B_factor 0.04 --alg edl --csv fb.csv
./ic --graph fb.bin --B_factor 0.04 --alg twin_greedy --csv fb.csv

./ic --graph fb.bin --B_factor 0.05 --alg dcs --w 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.05 --alg dcms --w 2 --alpha 2 --csv fb.csv
./ic --graph fb.bin --B_factor 0.05 --alg algo9 --csv fb.csv
./ic --graph fb.bin --B_factor 0.05 --alg algo10 --csv fb.csv
./ic --graph fb.bin --B_factor 0.05 --alg edl --csv fb.csv
./ic --graph fb.bin --B_factor 0.05 --alg twin_greedy --csv fb.csv



