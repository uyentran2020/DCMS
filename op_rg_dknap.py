# op_rg_dknap.py — 1-Pass Streaming Repeat Greedy for d-Knapsack
# Save-as-you-go CSV (9-field schema) + query counting

import argparse, csv, math, os, pickle, sys, time, signal
from multiprocessing import Pool, cpu_count
from typing import List, Tuple, Dict, Set
import networkx as nx

# === Submodular objectives f(G, S) ===
from functions.revenue_function import RevenueFunction
from functions.coverage_function import CoverageFunction
from functions.influence_function import InfluenceFunction
from functions.maxcut_function import MaxCutFunction

# --------------------
# d-knapsack utilities
# --------------------
def _node_costs(G: nx.Graph, e, d: int) -> Tuple[float, ...]:
    """Return d-dimensional nonnegative costs of node e."""
    c = G.nodes[e].get("costs", None)
    if c is None:
        w = float(G.nodes[e].get("weight", 0.0))
        cvec = [0.0]*d
        if d > 0: cvec[0] = w
        return tuple(float(x) for x in cvec)
    c = list(c)
    if len(c) < d: c += [0.0]*(d - len(c))
    if len(c) > d: c = c[:d]
    return tuple(float(max(0.0, x)) for x in c)

def _sum_costs(G: nx.Graph, S: Set, d: int) -> Tuple[float, ...]:
    tot = [0.0]*d
    for u in S:
        cu = _node_costs(G, u, d)
        for i in range(d): tot[i] += cu[i]
    return tuple(tot)

def _add_costs(a: Tuple[float, ...], b: Tuple[float, ...]) -> Tuple[float, ...]:
    return tuple(x+y for x, y in zip(a, b))

def _leq_componentwise(a: Tuple[float, ...], b: Tuple[float, ...]) -> bool:
    return all(x <= y + 1e-12 for x, y in zip(a, b))

def _total_costs(G: nx.Graph, d: int) -> Tuple[float, ...]:
    tot = [0.0]*d
    for u in G.nodes():
        cu = _node_costs(G, u, d)
        for i in range(d): tot[i] += cu[i]
    return tuple(tot)

def _bytes_of_sets(sets: List[Set]) -> int:
    total = 0
    for S in sets:
        total += sys.getsizeof(S)
        for x in S:
            total += sys.getsizeof(x)
    return total


# ---------------------------------------------------
# 1-Pass Streaming Repeat Greedy for d-knapsack (Alg.6)
# ---------------------------------------------------
def one_pass_repeat_greedy_dknap(
    G: nx.Graph, f, B: Tuple[float, ...], eps: float, d: int, stream_nodes=None
):
    """
    Returns: (S_best, f(S_best), cost_sum (scalar), stats_dict)
    Notes:
      - Implements a practical version of Algorithm 6.
      - Maintains S1^v, S2^v, S^v (singleton) per threshold v on-the-fly.
      - S3^v is treated as no-op (same as S^v) here.
    """
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    D = d
    m = 0.0  # max_e max_i f({e})/c_{i,e}

    S1: Dict[float, Set] = {}
    S2: Dict[float, Set] = {}
    Ssing: Dict[float, Set] = {}
    cost1: Dict[float, Tuple[float, ...]] = {}
    cost2: Dict[float, Tuple[float, ...]] = {}

    mem_bytes_peak = 0
    b_scalar = max(B) if len(B) > 0 else 0.0

    for e in stream_nodes:
        if e not in G:
            continue
        cvec = _node_costs(G, e, D)
        if all(ci <= 0 for ci in cvec):
            continue

        fe = f(G, {e})
        for i in range(D):
            if cvec[i] > 0:
                m = max(m, fe / cvec[i])

        # Build current grid Q
        if m <= 0 or b_scalar <= 0:
            Q = []
        else:
            v_min = m / (1.0 + eps)
            v_max = 2.0 * (D + 1) * b_scalar * m
            if v_min <= 0 or v_max <= 0:
                Q = []
            else:
                # compute integer k range such that (1+eps)^k in [v_min, v_max]
                log_base = math.log(1.0 + eps)
                k_lo = math.ceil(math.log(v_min, 1.0 + eps))
                k_hi = math.floor(math.log(v_max, 1.0 + eps))
                if math.isfinite(k_lo) and math.isfinite(k_hi) and k_lo <= k_hi:
                    Q = [ (1.0 + eps) ** k for k in range(int(k_lo), int(k_hi) + 1) ]
                else:
                    Q = []

        # ensure structures
        for v in Q:
            if v not in S1:
                S1[v] = set(); S2[v] = set(); Ssing[v] = set()
                cost1[v] = tuple([0.0]*D)
                cost2[v] = tuple([0.0]*D)

        # thresholds
        for v in Q:
            tau = v / (4.0 * (D + 1))

            # ---- Case 1: singleton S^v ----
            singleton_ok = False
            for i in range(D):
                Bi = B[i] if B[i] > 0 else float("inf")
                if cvec[i] >= B[i] / 2.0 - 1e-12 and cvec[i] > 0 and fe / cvec[i] >= (2.0 * tau) / Bi:
                    singleton_ok = True
                    break
            if singleton_ok:
                Ssing[v] = {e}

            # ---- Case 2: append to S1^v ----
            gain1 = f(G, S1[v] | {e}) - f(G, S1[v])
            feasible1 = _leq_componentwise(_add_costs(cost1[v], cvec), B)
            ratio_ok1 = True
            for i in range(D):
                if cvec[i] > 0:
                    Bi = B[i] if B[i] > 0 else float("inf")
                    if gain1 / cvec[i] + 1e-18 < (2.0 * tau) / Bi:
                        ratio_ok1 = False
                        break
            if feasible1 and ratio_ok1:
                S1[v].add(e)
                cost1[v] = _add_costs(cost1[v], cvec)
            else:
                # ---- Case 3: append to S2^v ----
                gain2 = f(G, S2[v] | {e}) - f(G, S2[v])
                feasible2 = _leq_componentwise(_add_costs(cost2[v], cvec), B)
                ratio_ok2 = True
                for i in range(D):
                    if cvec[i] > 0:
                        Bi = B[i] if B[i] > 0 else float("inf")
                        if gain2 / cvec[i] + 1e-18 < (2.0 * tau) / Bi:
                            ratio_ok2 = False
                            break
                if feasible2 and ratio_ok2:
                    S2[v].add(e)
                    cost2[v] = _add_costs(cost2[v], cvec)

        # track memory footprint roughly
        mem_now = _bytes_of_sets(list(S1.values()) + list(S2.values()) + list(Ssing.values()))
        if mem_now > mem_bytes_peak:
            mem_bytes_peak = mem_now

    # After stream: evaluate candidates
    best_S = set()
    best_val = float("-inf")
    cand_sets = []
    for v in list(S1.keys()):
        cand_sets += [S1[v], S2[v], Ssing[v]]  # S3^v ≈ Ssing[v]

    if not cand_sets:
        return set(), 0.0, 0.0, {"mem_bytes_peak": mem_bytes_peak}

    for S in cand_sets:
        val = f(G, S)
        if val > best_val:
            best_val = val
            best_S = set(S)

    costs_vec = _sum_costs(G, best_S, D)
    cost_scalar = float(sum(costs_vec))

    stats = {"mem_bytes_peak": mem_bytes_peak}
    return best_S, best_val, cost_scalar, stats


# ----------------
# Worker per B_factor
# ----------------
def run_one_B(args_tuple):
    G, f_class, f_name, B_factor, eps, stream_order, d = args_tuple

    f_wrapper = f_class(G)
    total_costs = _total_costs(G, d)  # per-dim sums
    B = tuple(float(B_factor) * tc for tc in total_costs)

    # count oracle queries for the whole run
    queries_before = f_wrapper.count

    start = time.time()
    S_best, f_best, c_best, stats = one_pass_repeat_greedy_dknap(
        G, f_wrapper, B=B, eps=eps, d=d, stream_nodes=stream_order
    )
    runtime = time.time() - start

    queries_used = f_wrapper.count - queries_before

    print(
        f"✅ [{f_name}] 1-Pass RG (d={d})  B_factor={B_factor}  "
        f"B={tuple(round(x,4) for x in B)}  f(S*)={f_best:.4f}  "
        f"|S*|={len(S_best)}  costΣ={c_best:.4f}  queries={queries_used}  "
        f"time={runtime:.2f}s  mem_peak={stats['mem_bytes_peak']}"
    )

    return {
        "func": f_name,
        "B_factor": B_factor,
        "B_value": sum(B),       # scalar B for CSV consistency
        "f_value": f_best,
        "size": len(S_best),
        "cost": c_best,          # scalarized sum of d-dim costs
        "queries": queries_used,
        "runtime": runtime,
        "mem_bytes_peak": stats["mem_bytes_peak"],
    }


# ----- Save-as-you-go helpers -----
def _write_header_if_needed(csv_path, writer):
    """Write header if file doesn't exist or is empty."""
    need_header = True
    if os.path.exists(csv_path):
        try:
            need_header = (os.path.getsize(csv_path) == 0)
        except Exception:
            need_header = True
    if need_header:
        writer.writeheader()

def _install_sigint_handler():
    orig_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGINT, orig_handler)


# ----- Main -----
if __name__ == "__main__":
    p = argparse.ArgumentParser(
        description="1-Pass Streaming Repeat Greedy for d-Knapsack — save-as-you-go CSV."
    )
    p.add_argument("--graph", type=str, required=True, help="Pickle graph with node costs.")
    p.add_argument("--func", type=str, default="revenue", help="revenue | coverage | influence | maxcut")
    p.add_argument("--dims", type=int, required=True, help="Number of knapsack dimensions d.")
    p.add_argument("--B_factors", type=float, nargs="+",
                   default=[0.1, 0.15, 0.2, 0.25, 0.3],
                   help="B_i = factor × (sum_i of node costs on dim i) for all i.")
    p.add_argument("--eps", type=float, default=0.1, help="epsilon in (0,1).")
    p.add_argument("--order", type=str, default="natural", help="Stream order: natural.")
    p.add_argument("--output", type=str, default="results_oprg_dknap.csv")
    p.add_argument("--n_jobs", type=int, default=-1)
    p.add_argument("--append", action="store_true")
    args = p.parse_args()

    with open(args.graph, "rb") as f_in:
        G = pickle.load(f_in)

    func_map = {"revenue": RevenueFunction, "coverage": CoverageFunction, "influence": InfluenceFunction, "maxcut": MaxCutFunction}
    if args.func not in func_map:
        raise ValueError(f"Unknown function '{args.func}'. Choose from: {list(func_map.keys())}")
    f_class = func_map[args.func]; f_name = f_class(G).name

    # Stream order
    if args.order == "natural":
        stream_order = list(G.nodes())
    else:
        raise ValueError(f"Unsupported --order '{args.order}'. Only 'natural' is implemented.")

    # Multiprocessing over B_factors
    n_jobs = cpu_count() if args.n_jobs == -1 else max(1, args.n_jobs)
    pool_inputs = [(G, f_class, f_name, b, args.eps, stream_order, args.dims) for b in args.B_factors]

    # CSV schema — 9 fields (standard)
    fieldnames = ["func","B_factor","B_value","f_value","size","cost","queries","runtime","mem_bytes_peak"]

    # Prepare file
    open_mode = "a" if args.append else "w"
    os.makedirs(os.path.dirname(args.output), exist_ok=True) if os.path.dirname(args.output) else None

    with open(args.output, open_mode, newline="", encoding="utf-8") as f_out:
        writer = csv.DictWriter(f_out, fieldnames=fieldnames)
        _write_header_if_needed(args.output, writer)

        if n_jobs == 1 or len(pool_inputs) == 1:
            # Sequential (debug-friendly) — write each result immediately
            for item in pool_inputs:
                try:
                    row = run_one_B(item)
                    writer.writerow(row)
                    f_out.flush(); os.fsync(f_out.fileno())
                except KeyboardInterrupt:
                    print("⛔️ Bị huỷ bởi người dùng. Các kết quả đã viết vẫn được giữ lại.")
                    break
                except Exception as ex:
                    print(f"❌ Lỗi ở B_factor={item[3]}: {ex}", file=sys.stderr)
            print(f"📂 Results saved to {args.output}")
            sys.exit(0)

        # Parallel — still write each result immediately
        try:
            with Pool(processes=min(len(pool_inputs), n_jobs), initializer=_install_sigint_handler) as pool:
                for row in pool.imap_unordered(run_one_B, pool_inputs):
                    if row is not None:
                        writer.writerow(row)
                        f_out.flush(); os.fsync(f_out.fileno())
        except KeyboardInterrupt:
            print("\n⛔️ Bị huỷ bởi người dùng. Các kết quả đã viết vẫn được giữ lại.")
        except Exception as ex:
            print(f"❌ Lỗi khi chạy multiprocessing: {ex}", file=sys.stderr)

    print(f"📄 Đã ghi kết quả vào: {args.output}")
