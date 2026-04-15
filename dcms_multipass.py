# dcms_multipass.py  — save-as-you-go CSV (9-field schema), robust writes

import pickle, argparse, time, csv, os, sys, math, signal
from multiprocessing import Pool, cpu_count
import networkx as nx

# Hàm f đã theo SubmodularFunction, nhận S là set
from functions.revenue_function import RevenueFunction
from functions.coverage_function import CoverageFunction
from functions.influence_function import InfluenceFunction
from functions.maxcut_function import MaxCutFunction

# DCS (file trước)
from dcs_streaming import DCS_streaming

# =========================
# Tiện ích chung
# =========================
def node_cost(G, e): return float(G.nodes[e].get("weight", 1.0))
def set_cost(G, S): return sum(node_cost(G, e) for e in S)
def total_node_cost(G): return sum(node_cost(G, u) for u in G.nodes())

def argmax_by_value(cands):
    """
    cands: list[ (name, S, fval, cost) ]
    Trả về tuple có fval lớn nhất.
    """
    return max(cands, key=lambda x: x[2]) if cands else None

# =========================
# Ước lượng byte lưu LỜI GIẢI DỰ TUYỂN (chỉ X_rho, Y_rho)
# =========================
def _sets_bytes(dict_of_sets: dict) -> int:
    total = 0
    for S in dict_of_sets.values():
        total += sys.getsizeof(S)
        for e in S:
            total += sys.getsizeof(e)
    return total

# =========================
# DCMS: Dual Candidates Multi-pass Streaming
# =========================
def DCMS_multipass(
    G: nx.DiGraph, f, B: float, w: int, eps: float, alpha: int,
    stream_nodes=None
):
    """
    Cài đặt theo giả mã:
      - Pass 1: DCS_streaming lấy (M1, M, M') ~ (Q*, X_union, Y_union).
      - Tạo tập ngưỡng R theo eps', M1, M2.
      - Pass 2 & 3: cập nhật ứng viên X_rho, Y_rho và L*.
      - Fill-in các phần tử còn lại nếu còn ngân sách.
    Trả về (L_star, f(L_star), cost(L_star), stats)
    """
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    # ===== Pass 1: DCS =====
    DCS_Q, DCS_XU, DCS_YU, fQ, _, stats_dcs = DCS_streaming(
        G, f, B=B, w=w, stream_nodes=stream_nodes
    )
    M1 = set(DCS_Q)
    M  = set(DCS_XU)
    Mp = set(DCS_YU)

    # Theo giả mã: M2 = argmax{ f(M), f(M') }
    fM  = f(G, M)
    fMp = f(G, Mp)
    M2 = M if fM >= fMp else Mp
    fM2 = max(fM, fMp)

    eps_p = eps / 5.0

    # ===== Tập ngưỡng R =====
    # R = { (1 - eps')^{-i} | i ∈ Z, lower <= (1 - eps')^{-i} <= upper },
    # lower = ((1 - eps') eps' f(M1)) / (2B), upper = (alpha f(M2)) / (5 eps' B)
    lower = ((1.0 - eps_p) * eps_p * f(G, M1)) / (2.0 * B) if B > 0 else 0.0
    upper = (alpha * fM2) / (5.0 * eps_p * B) if (B > 0 and eps_p > 0) else 0.0

    R = []
    if lower > 0 and upper >= lower and (1.0 - eps_p) > 0.0:
        base = 1.0 - eps_p
        log_base = math.log(base)  # < 0
        i_min = math.ceil( math.log(lower) / (-log_base) )
        i_max = math.floor( math.log(upper) / (-log_base) )
        for i in range(i_min, i_max + 1):
            rho = base ** (-i)
            R.append(rho)

    # Nếu R rỗng (ngưỡng không hợp lệ), trả về luôn M1 (an toàn)
    if not R:
        L_star = set(M1)
        stats = {
            "queries_total": f.count,             # gồm cả DCS
            "mem_bytes_peak": 0,                  # chưa tạo ứng viên DCMS
            "num_rho": 0,
            "pass2_added": 0,
            "pass3_updates": 0,
            "fill_in_added": 0,
        }
        return L_star, f(G, L_star), set_cost(G, L_star), stats

    # Khởi tạo ứng viên cho mỗi rho
    X = {rho: set() for rho in R}
    Y = {rho: set() for rho in R}

    # L* khởi tạo = M1
    L_star = set(M1)

    # Bộ nhớ peak cho LỜI GIẢI DỰ TUYỂN (X_rho, Y_rho) trong suốt DCMS
    mem_bytes_peak = _sets_bytes(X) + _sets_bytes(Y)

    pass2_added = 0
    pass3_updates = 0
    fill_in_added = 0

    # ===== Pass 2 =====
    for e in stream_nodes:
        if e not in G:
            continue
        c_e = node_cost(G, e)
        if c_e <= 0:
            continue
        for rho in R:
            # A_rho = argmax_{A in {X_rho, Y_rho}} f(e | A)
            AX = X[rho]; AY = Y[rho]
            gainX = f(G, AX | {e}) - f(G, AX)
            gainY = f(G, AY | {e}) - f(G, AY)
            A = AX if gainX >= gainY else AY

            # Nếu A có thể thêm e (không vượt B) và gain >= rho * c(e)
            if (set_cost(G, A) + c_e) <= B and ( (f(G, A | {e}) - f(G, A)) >= rho * c_e ):
                A.add(e)
                pass2_added += 1

            # Cập nhật L*
            if f(G, A) > f(G, L_star):
                L_star = set(A)

        # cập nhật peak bytes ứng viên DCMS
        cur_bytes = _sets_bytes(X) + _sets_bytes(Y)
        if cur_bytes > mem_bytes_peak:
            mem_bytes_peak = cur_bytes

    # ===== Pass 3 =====
    for e in stream_nodes:
        if e not in G:
            continue
        c_e = node_cost(G, e)
        if c_e <= 0:
            continue

        fL = f(G, L_star)
        for rho in R:
            # thử cập nhật từ X_rho
            if (e not in X[rho]) and (set_cost(G, X[rho]) + c_e <= B) and (f(G, X[rho] | {e}) >= fL):
                L_star = set(X[rho] | {e})
                fL = f(G, L_star)
                pass3_updates += 1

            # thử từ Y_rho
            if (e not in Y[rho]) and (set_cost(G, Y[rho]) + c_e <= B) and (f(G, Y[rho] | {e}) >= fL):
                L_star = set(Y[rho] | {e})
                fL = f(G, L_star)
                pass3_updates += 1

        # cập nhật peak bytes ứng viên DCMS
        cur_bytes = _sets_bytes(X) + _sets_bytes(Y)
        if cur_bytes > mem_bytes_peak:
            mem_bytes_peak = cur_bytes

    # ===== Fill-in cuối =====
    for rho in R:
        for e in X[rho]:
            if e not in L_star and (set_cost(G, L_star) + node_cost(G, e) <= B):
                L_star.add(e); fill_in_added += 1
        for e in Y[rho]:
            if e not in L_star and (set_cost(G, L_star) + node_cost(G, e) <= B):
                L_star.add(e); fill_in_added += 1

    stats = {
        "queries_total": f.count,      # gồm cả DCS
        "mem_bytes_peak": mem_bytes_peak,
        "num_rho": len(R),
        "pass2_added": pass2_added,
        "pass3_updates": pass3_updates,
        "fill_in_added": fill_in_added,
    }
    return L_star, f(G, L_star), set_cost(G, L_star), stats

# =========================
# Worker cho 1 giá trị B_factor
# =========================
def run_one_B(args_tuple):
    G, f_class, f_name, B_factor, w, eps, alpha, stream_order = args_tuple

    f_wrapper = f_class(G)   # SubmodularFunction
    total_cost = total_node_cost(G)
    B = float(B_factor) * total_cost

    start = time.time()
    L_star, fL, costL, stats = DCMS_multipass(
        G, f_wrapper, B=B, w=w, eps=eps, alpha=alpha, stream_nodes=stream_order
    )
    runtime = time.time() - start

    print(
        f"✅ [{f_name}] DCMS B={B:.4f} (factor={B_factor}, total_cost={total_cost:.4f}) "
        f"f(L*)={fL:.4f}, |L*|={len(L_star)}, cost(L*)={costL:.4f}, "
        f"ρ={stats.get('num_rho',0)}, q_total={stats.get('queries_total',0)}, "
        f"time={runtime:.2f}s, mem_bytes_peak={stats.get('mem_bytes_peak',0)}"
    )

    # Trả về đúng 9 tiêu chí chuẩn; map queries_total -> queries
    return {
        "func": f_name,
        "B_factor": B_factor,
        "B_value": B,
        "f_value": fL,
        "size": len(L_star),
        "cost": costL,
        "queries": stats.get("queries_total", 0),
        "runtime": runtime,
        "mem_bytes_peak": stats.get("mem_bytes_peak", 0),
    }

# =========================
# Main
# =========================
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
    p = argparse.ArgumentParser(
        description="DCMS (Dual Candidates Multi-pass Streaming) — chạy nhiều B song song, save-as-you-go CSV."
    )
    p.add_argument("--graph", type=str, required=True, help="File .pkl chứa đồ thị")
    p.add_argument("--func", type=str, default="revenue", help="revenue | coverage | influence | maxcut")
    p.add_argument("--B_factors", type=float, nargs="+", default=[0.1, 0.15, 0.2, 0.25, 0.3],
                   help="Danh sách hệ số B = factor × (tổng chi phí nút)")
    p.add_argument("--w", type=int, default=2, help="Tham số w (truyền xuống DCS)")
    p.add_argument("--eps", type=float, default=0.1, help="ε trong (0,1) (dùng ε' = ε/5)")
    p.add_argument("--alpha", type=int, default=12, help="α ∈ Z⁺ (cận trên trong tập ngưỡng)")
    p.add_argument("--order", type=str, default="natural",
                   help="Thứ tự stream: natural | shuffled (giữ natural như file trước)")
    p.add_argument("--output", type=str, default="results_dcms.csv")
    p.add_argument("--n_jobs", type=int, default=-1)
    p.add_argument("--append", action="store_true")
    args = p.parse_args()

    # Load graph
    with open(args.graph, "rb") as f_in:
        G = pickle.load(f_in)

    # Chọn hàm f
    func_map = {"revenue": RevenueFunction, "coverage": CoverageFunction,
                "influence": InfluenceFunction, "maxcut": MaxCutFunction}
    if args.func not in func_map:
        raise ValueError(f"Unknown function '{args.func}'. Choose from: {list(func_map.keys())}")
    f_class = func_map[args.func]
    f_name = f_class(G).name

    # Thứ tự stream: giữ natural giống file trước
    stream_order = list(G.nodes())

    # Multiprocessing trên các B_factor
    n_jobs = cpu_count() if args.n_jobs == -1 else max(1, args.n_jobs)
    pool_inputs = [
        (G, f_class, f_name, b, args.w, args.eps, args.alpha, stream_order)
        for b in args.B_factors
    ]

    # CSV save-as-you-go — đúng bộ 9 tiêu chí
    fieldnames = ["func","B_factor","B_value","f_value","size","cost","queries","runtime","mem_bytes_peak"]

    # Mở file CSV theo chế độ
    open_mode = "a" if args.append else "w"
    os.makedirs(os.path.dirname(args.output), exist_ok=True) if os.path.dirname(args.output) else None

    with open(args.output, open_mode, newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        _write_header_if_needed(args.output, writer)

        # Nếu chỉ 1 job → chạy tuần tự (an toàn log/debug) + save-as-you-go
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

#python dcms_multipass.py --graph GrQc.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output dcms_multipass.csv --append