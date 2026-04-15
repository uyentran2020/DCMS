# edl_1k.py  — EDL for 1-knapsack (with LA), now reporting queries instead of rounds
import argparse, csv, math, os, pickle, sys, time, signal
from multiprocessing import Pool, cpu_count
import networkx as nx
from functions.revenue_function import RevenueFunction
from functions.coverage_function import CoverageFunction
from functions.influence_function import InfluenceFunction
from functions.maxcut_function import MaxCutFunction

def node_cost(G, e): return float(G.nodes[e].get("weight", 1.0))
def set_cost(G, S): return sum(node_cost(G, u) for u in S)
def total_node_cost(G): return sum(node_cost(G, u) for u in G.nodes())
def _bytes_of_sets(*sets):
    total = 0
    for S in sets:
        total += sys.getsizeof(S)
        for x in S:
            total += sys.getsizeof(x)
    return total

def LA_1k(G: nx.DiGraph, f, B: float, stream_nodes=None):
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    V1 = [e for e in stream_nodes if e in G and node_cost(G, e) <= B / 2.0]
    e_max, f_e_max = None, float("-inf")
    for e in stream_nodes:
        if e in G:
            val = f(G, {e})
            if val > f_e_max:
                f_e_max, e_max = val, e

    X, Y = set(), set()
    X_order, Y_order = [], []
    for e in V1:
        ce = node_cost(G, e)
        if ce <= 0: continue
        dens_X = (f(G, X | {e}) - f(G, X)) / ce
        dens_Y = (f(G, Y | {e}) - f(G, Y)) / ce
        thr_X = f(G, X) / B if B > 0 else float("inf")
        thr_Y = f(G, Y) / B if B > 0 else float("inf")
        cand = []
        if dens_X >= thr_X: cand.append(("X", dens_X))
        if dens_Y >= thr_Y: cand.append(("Y", dens_Y))
        if not cand: continue
        Z = max(cand, key=lambda t: t[1])[0]
        if Z == "X": X.add(e); X_order.append(e)
        else:        Y.add(e); Y_order.append(e)

    def best_suffix(T_set, T_order):
        best_S, best_val = set(), float("-inf")
        cur, cur_cost = [], 0.0
        for e in reversed(T_order):
            ce = node_cost(G, e)
            if cur_cost + ce <= B:
                cur.append(e); cur_cost += ce
                S = set(reversed(cur))
                val = f(G, S)
                if val > best_val:
                    best_val, best_S = val, set(S)
        if best_val < 0: best_val, best_S = 0.0, set()
        return best_S, best_val

    Xp, fXp = best_suffix(X, X_order)
    Yp, fYp = best_suffix(Y, Y_order)
    cand = [("X'", Xp, fXp),
            ("Y'", Yp, fYp),
            ("e_max", {e_max} if e_max is not None else set(),
                      f_e_max if e_max is not None else -float("inf"))]
    _, S_best, f_best = max(cand, key=lambda x: x[2])
    return S_best, f_best

def EDL_1k(G: nx.DiGraph, f, B: float, eps: float, stream_nodes=None):
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    # === start counting oracle queries (includes LA) ===
    queries_before = f.count

    # Step 1: LA
    S_prime, M = LA_1k(G, f, B, stream_nodes=stream_nodes)

    eps_p = eps / 14.0
    rounds = int(math.ceil(math.log(19.0 / (eps_p ** 2), 1.0 / (1.0 - eps_p)))) + 1 if 0 < eps_p < 1 else 1

    X, Y = set(), set()
    mem_bytes_peak = _bytes_of_sets(X, Y)

    for i in range(rounds + 1):
        theta = 19.0 * M * ((1.0 - eps_p) ** i) / (5.0 * eps_p * B) if B > 0 and eps_p > 0 else 0.0
        for e in stream_nodes:
            if e not in G or e in X or e in Y: continue
            ce = node_cost(G, e)
            if ce <= 0: continue
            densX = (f(G, X | {e}) - f(G, X)) / ce
            densY = (f(G, Y | {e}) - f(G, Y)) / ce
            choices = []
            if (densX >= theta) and (set_cost(G, X) + ce <= B): choices.append(("X", densX))
            if (densY >= theta) and (set_cost(G, Y) + ce <= B): choices.append(("Y", densY))
            if choices:
                Z = max(choices, key=lambda t: t[1])[0]
                if Z == "X": X.add(e)
                else:        Y.add(e)
            cur_bytes = _bytes_of_sets(X, Y)
            if cur_bytes > mem_bytes_peak: mem_bytes_peak = cur_bytes

    fX, fY = f(G, X), f(G, Y)
    if fX >= fY: S, fS = X, fX
    else:        S, fS = Y, fY

    # === total oracle queries used by EDL (including LA pre-processing) ===
    queries_used = f.count - queries_before

    return S, fS, set_cost(G, S), {
        "f_LA": M,
        "queries": queries_used,
        "mem_bytes_peak": mem_bytes_peak,
    }

def run_one_B(args_tuple):
    G, f_class, f_name, B_factor, eps, stream_order = args_tuple
    f_wrapper = f_class(G)
    total_cost = total_node_cost(G); B = float(B_factor) * total_cost

    start = time.time()
    S, fS, cS, stats = EDL_1k(G, f_wrapper, B=B, eps=eps, stream_nodes=stream_order)
    runtime = time.time() - start

    print(
        f"✅ [{f_name}] EDL-1K B={B:.4f} (factor={B_factor}, total_cost={total_cost:.4f}) "
        f"f(S)={fS:.4f}, |S|={len(S)}, cost(S)={cS:.4f}, "
        f"queries={stats['queries']}, f_LA={stats['f_LA']:.4f}, "
        f"time={runtime:.2f}s, mem_bytes_peak={stats['mem_bytes_peak']}"
    )

    # Trả về đúng 9 tiêu chí chuẩn
    return {
        "func": f_name,
        "B_factor": B_factor,
        "B_value": B,
        "f_value": fS,
        "size": len(S),
        "cost": cS,
        "queries": stats["queries"],
        "runtime": runtime,
        "mem_bytes_peak": stats["mem_bytes_peak"],
    }

# ============== save-as-you-go CSV utils ==============
def _write_header_if_needed(csv_path, writer):
    """Ghi header nếu file chưa tồn tại hoặc rỗng."""
    need_header = True
    if os.path.exists(csv_path):
        try:
            need_header = (os.path.getsize(csv_path) == 0)
        except Exception:
            need_header = True
    if need_header:
        writer.writeheader()

def _install_sigint_handler():
    # Đảm bảo KeyboardInterrupt được propagate gọn gàng khi dùng multiprocessing
    orig_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGINT, orig_handler)

if __name__ == "__main__":
    p = argparse.ArgumentParser(description="EDL (with LA) for 1-Knapsack — save-as-you-go CSV.")
    p.add_argument("--graph", type=str, required=True)
    p.add_argument("--func", type=str, default="revenue", help="revenue | coverage | influence | maxcut")
    p.add_argument("--B_factors", type=float, nargs="+", default=[0.1, 0.15, 0.2, 0.25, 0.3])
    p.add_argument("--eps", type=float, default=0.1)
    p.add_argument("--order", type=str, default="natural")
    p.add_argument("--output", type=str, default="results_edl1k.csv")
    p.add_argument("--n_jobs", type=int, default=-1)
    p.add_argument("--append", action="store_true")
    args = p.parse_args()

    with open(args.graph, "rb") as f_in:
        G = pickle.load(f_in)

    func_map = {"revenue": RevenueFunction, "coverage": CoverageFunction, "influence": InfluenceFunction, "maxcut": MaxCutFunction}
    if args.func not in func_map:
        raise ValueError(f"Unknown function '{args.func}'. Choose from: {list(func_map.keys())}")
    f_class = func_map[args.func]; f_name = f_class(G).name

    stream_order = list(G.nodes())
    n_jobs = cpu_count() if args.n_jobs == -1 else max(1, args.n_jobs)
    pool_inputs = [(G, f_class, f_name, b, args.eps, stream_order) for b in args.B_factors]

    # Chuẩn schema 9 trường để copy nhanh
    fieldnames = ["func","B_factor","B_value","f_value","size","cost","queries","runtime","mem_bytes_peak"]
    open_mode = "a" if args.append else "w"
    os.makedirs(os.path.dirname(args.output), exist_ok=True) if os.path.dirname(args.output) else None

    with open(args.output, open_mode, newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        _write_header_if_needed(args.output, writer)

        # Nếu chỉ 1 job → chạy tuần tự + save-as-you-go
        if n_jobs == 1 or len(pool_inputs) == 1:
            for inp in pool_inputs:
                try:
                    res = run_one_B(inp)
                    writer.writerow(res)
                    csvfile.flush(); os.fsync(csvfile.fileno())
                except KeyboardInterrupt:
                    print("⛔️ Bị huỷ bởi người dùng. Các kết quả đã viết vẫn được giữ lại.")
                    break
                except Exception as ex:
                    print(f"❌ Lỗi ở B_factor={inp[3]}: {ex}", file=sys.stderr)
            print(f"📂 Results saved to {args.output}")
            sys.exit(0)

        # Chạy song song, ghi ngay từng kết quả
        try:
            with Pool(processes=min(len(pool_inputs), n_jobs), initializer=_install_sigint_handler) as pool:
                for res in pool.imap_unordered(run_one_B, pool_inputs):
                    if res is not None:
                        writer.writerow(res)
                        csvfile.flush(); os.fsync(csvfile.fileno())
        except KeyboardInterrupt:
            print("\n⛔️ Bị huỷ bởi người dùng. Các kết quả đã viết vẫn được giữ lại.")
        except Exception as ex:
            print(f"❌ Lỗi khi chạy multiprocessing: {ex}", file=sys.stderr)

    print(f"📂 Results saved to {args.output}")

# python edl_1k.py --graph graph.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --eps 0.1 --output results_edl1k.csv --append

