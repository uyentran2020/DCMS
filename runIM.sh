#!/bin/bash
#SBATCH --job-name=Uyen-IM
#SBATCH --partition=gpu
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=5
#SBATCH --output=%x.%j.out
#SBATCH --error=%x.%j.err

python dcms_multipass.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --w 2 --output FB_results_dcms_multipass.csv --append
python dcs_streaming.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --w 2 --output FB_results_dcs.csv --append
python edl_1k.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --output FB_results_edl1k.csv --append
python op_rg_dknap.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --output FB_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --output FB_results_op_rg_dknap.csv --append

python dcms_multipass.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --w 2 --output Email_results_dcms_multipass.csv --append
python dcs_streaming.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --w 2 --output Email_results_dcs.csv --append
python edl_1k.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --output Email_results_edl1k.csv --append
python mp_rgmax_1k.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1  --output Email_results_op_rg_dknap.csv --append