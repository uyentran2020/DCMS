# Knapsack-Constrained Submodular Maximization via Dual Cumulative Sets in Streaming

This repository is the official experimental implementation of **Knapsack-Constrained Submodular Maximization via Dual Cumulative Sets in Streaming**.

The code implements two proposed streaming algorithms for General Submodular Maximization under a Knapsack constraint (\GSMK):

- **DCS**: Dual Cumulative Sets-aided Streaming
- **DCMS**: Dual Cumulative Sets-based Multi-Stream

The implementation evaluates the algorithms on three applications:

- **IMK**: Influence Maximization under a Knapsack constraint
- **MCK**: Maximum Weighted Cut under a Knapsack constraint
- **RMK**: Revenue Maximization under a Knapsack constraint

The compared baselines include:

- **EDL**
- **op\_rg\_dknap**
- **mp\_rgmax**
- other methods implemented in the repository

## Dependencies

- Python 3.x
- NetworkX
- Standard Python utilities for reading `.txt`, `.csv`, `.pkl`, and writing result files

## The source code is organized as follows:

`data/` folder:
- `generate_graph_networkx.py`: used to construct `.pkl` graph files from raw `.txt` or `.csv` edge-list datasets

Main experiment scripts:
- `dcms_multipass.py`: runs the **DCMS** algorithm
- `dcs_streaming.py`: runs the **DCS** algorithm
- `edl_1k.py`: runs the **EDL** baseline
- `op_rg_dknap.py`: runs the **op_rg_dknap** baseline
- `mp_rgmax_1k.py`: runs the **mp_rgmax** baseline

Input datasets:
- `facebook.txt`, `Email.txt`: datasets for **Influence Maximization**
- `Astro.txt`, `ER.txt`: datasets for **Maximum Cut**
- `GrQc.txt`, `Hept.txt`: datasets for **Revenue Maximization**

Generated graph files:
- `FB.pkl`, `Email.pkl`
- `Astro.pkl`, `ER.pkl`
- `GrQc.pkl`, `Hept.pkl`

Output files:
- CSV result files for each dataset and each algorithm, e.g.
  - `FB_results_dcms_multipass.csv`
  - `Astro_results_dcs.csv`
  - `GrQc_results_edl1k.csv`

## Format of input graph

The raw input graph is given as an edge list stored in `.txt` or `.csv` format.

### Step 1: Generate graph files
Use `data/generate_graph_networkx.py` to convert raw edge lists into `.pkl` graph files.

### Important notes
- For **Influence Maximization (IM)** under the IC model, the total incoming edge weight of each node \(u\) must be normalized to 1. Therefore, use:
  - `--normalize`
- For **Maximum Cut**, the graph must be symmetric, i.e.
  \[
  w(u,v)=w(v,u).
  \]
  Therefore, use:
  - `--symmetric`

## To run the source code, follow these steps:

### Step 1: Generate datasets

#### Influence Maximization (IMK): Facebook, Email
```bash
python data/generate_graph_networkx.py --filename facebook.txt --output FB.pkl --normalize
python data/generate_graph_networkx.py --filename Email.txt --output Email.pkl --normalize
```

#### Maximum Cut (MCK): Astro, ER
```bash
python data/generate_graph_networkx.py --filename ER.txt --output ER.pkl --symmetric
python data/generate_graph_networkx.py --filename Astro.txt --output Astro.pkl --symmetric
```

#### Revenue Maximization (RMK): GrQc, Hept
```bash
python data/generate_graph_networkx.py --filename GrQc.txt --output GrQc.pkl
python data/generate_graph_networkx.py --filename Hept.txt --output Hept.pkl
```

### Step 2: Run the experiments

## Influence Maximization (IMK)

### Facebook
```bash
python dcms_multipass.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --w 2 --output FB_results_dcms_multipass.csv --append
python dcs_streaming.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --w 2 --output FB_results_dcs.csv --append
python edl_1k.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --output FB_results_edl1k.csv --append
python op_rg_dknap.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --output FB_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph FB.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --output FB_results_mp_rgmax.csv --append
```

### Email
```bash
python dcms_multipass.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --w 2 --output Email_results_dcms_multipass.csv --append
python dcs_streaming.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --w 2 --output Email_results_dcs.csv --append
python edl_1k.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --output Email_results_edl1k.csv --append
python op_rg_dknap.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --output Email_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph Email.pkl --func influence --B_factors 0.02 0.04 0.06 0.08 0.1 --output Email_results_mp_rgmax.csv --append
```

## Maximum Cut (MCK)

### Astro
```bash
python dcms_multipass.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output Astro_results_dcms_multipass.csv --append
python dcs_streaming.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output Astro_results_dcs.csv --append
python edl_1k.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output Astro_results_edl1k.csv --append
python op_rg_dknap.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output Astro_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph Astro.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output Astro_results_mp_rgmax.csv --append
```

### ER
```bash
python dcms_multipass.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output ER_results_dcms_multipass.csv --append
python dcs_streaming.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output ER_results_dcs.csv --append
python edl_1k.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output ER_results_edl1k.csv --append
python op_rg_dknap.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output ER_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph ER.pkl --func maxcut --B_factors 0.1 0.15 0.2 0.25 0.3 --output ER_results_mp_rgmax.csv --append
```

## Revenue Maximization (RMK)

### GrQc
```bash
python dcms_multipass.py --graph GrQc.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output GrQc_results_dcms_multipass.csv --append
python dcs_streaming.py --graph GrQc.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output GrQc_results_dcs.csv --append
python edl_1k.py --graph GrQc.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --output GrQc_results_edl1k.csv --append
python op_rg_dknap.py --graph GrQc.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --output GrQc_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph GrQc.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --output GrQc_results_mp_rgmax.csv --append
```

### Hept
```bash
python dcms_multipass.py --graph Hept.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output Hept_results_dcms_multipass.csv --append
python dcs_streaming.py --graph Hept.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output Hept_results_dcs.csv --append
python edl_1k.py --graph Hept.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --output Hept_results_edl1k.csv --append
python op_rg_dknap.py --graph Hept.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --output Hept_results_op_rg_dknap.csv --append
python mp_rgmax_1k.py --graph Hept.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --output Hept_results_mp_rgmax.csv --append
```

## Parameters

```text
Options:
--graph <input graph file in .pkl format>
--func <objective function: influence | maxcut | revenue>
--B_factors <list of budget factors>
--w <window parameter used by DCS/DCMS>
--output <output CSV filename>
--append <append results to an existing CSV file>
```

## Experimental Setup

The experiments are conducted on three benchmark applications:

- **IMK** on Facebook and Email
- **MCK** on Astro and ER
- **RMK** on GrQc and Hept

### Budget factors
- **IMK**: `0.02, 0.04, 0.06, 0.08, 0.1`
- **MCK**: `0.1, 0.15, 0.2, 0.25, 0.3`
- **RMK**: `0.1, 0.15, 0.2, 0.25, 0.3`

### Algorithms compared
- `dcms_multipass.py`
- `dcs_streaming.py`
- `edl_1k.py`
- `op_rg_dknap.py`
- `mp_rgmax_1k.py`

### Notes
- For IMK under the IC model, use normalized incoming edge weights.
- For MCK, use symmetric edge weights.
- The parameter `--w 2` is used in the DCS/DCMS experiments shown above.

## Citation

If you use this repository, please cite the corresponding paper.
