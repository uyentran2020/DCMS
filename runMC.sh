#!/bin/bash
#SBATCH --job-name=Uyen-MC
#SBATCH --partition=small
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=5
#SBATCH --output=%x.%j.out
#SBATCH --error=%x.%j.err

python dcms_multipass.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output Astro_results_dcms_multipass.csv --append
python dcs_streaming.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output Astro_results_dcs.csv --append
python edl_1k.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output Astro_results_edl1k.csv --append
python op_rg_dknap.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output Astro_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output Astro_results_op_rg_dknap.csv --append

python dcms_multipass.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output ER_results_dcms_multipass.csv --append
python dcs_streaming.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output ER_results_dcs.csv --append
python edl_1k.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output ER_results_edl1k.csv --append
python op_rg_dknap.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output ER_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output ER_results_op_rg_dknap.csv --append