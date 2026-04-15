# dcs_streaming.py (save-as-you-go, robust CSV write, peak memory metric)

import pickle, argparse, time, csv, os, sys, signal
from multiprocessing import Pool, cpu_count
import networkx as nx

from functions.revenue_function import RevenueFunction
from functions.coverage_function import CoverageFunction
from functions.influence_function import InfluenceFunction
from functions.maxcut_function import MaxCutFunction


# =========================
# Tiện ích chung
# =========================
def node_cost(G, e): return float(G.nodes[e].get("weight", 1.0))
def set_cost(G, S): return sum(node_cost(G, e) for e in S)
def total_node_cost(G): return sum(node_cost(G, u) for u in G.nodes())


# =========================
# DualCandidate (cửa sổ [j..i])
# =========================
class DualCandidate:
    def __init__(self):
        self.segments = [[]]
        self.seg_costs = [0.0]
        self.j = 0
        self.i = 0

    def union_set(self):
        U = set()
        for t in range(self.j, self.i + 1):
            U.update(self.segments[t])
        return U

    def union_order(self):
        order = []
        for t in range(self.j, self.i + 1):
            order.extend(self.segments[t])
        return order

    def current_cost(self): return self.seg_costs[self.i]

    def add(self, G, e):
        self.segments[self.i].append(e)
        self.seg_costs[self.i] += node_cost(G, e)

    def advance_segment(self):
        self.segments.append([]); self.seg_costs.append(0.0); self.i += 1

    def delete_first_w_segments(self, w):
        del self.segments[self.j:self.j+w]
        del self.seg_costs[self.j:self.j+w]
        self.i -= w

    def window_len(self): return self.i - self.j + 1

    def bounded_suffix(self, G, B):
        picked, cost = [], 0.0
        for e in reversed(self.union_order()):
            c = node_cost(G, e)
            if cost + c <= B:
                picked.append(e); cost += c
        return set(reversed(picked))


def _candidate_bytes(dc: DualCandidate) -> int:
    """Ước lượng byte lưu các segment của ứng viên trong cửa sổ [j..i]."""
    total = 0
    for t in range(dc.j, dc.i + 1):
        seg = dc.segments[t]
        total += sys.getsizeof(seg)
        for e in seg:
            total += sys.getsizeof(e)
    return total


# =========================
# Thuật toán DCS
# =========================
def DCS_streaming(G: nx.DiGraph, f, B: float, w: int, stream_nodes=None):
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    X, Y = DualCandidate(), DualCandidate()
    e_star, e_star_val = None, float("-inf")

    mem_bytes_peak = _candidate_bytes(X) + _candidate_bytes(Y)
    queries_before = f.count

    for e in stream_nodes:
        if e not in G: 
            continue
        c_e = node_cost(G, e)
        if c_e <= 0: 
            continue

        UX = X.union_set(); gainX = f(G, UX | {e}) - f(G, UX)
        UY = Y.union_set(); gainY = f(G, UY | {e}) - f(G, UY)

        if gainX >= gainY:
            chosen, sigma, U = X, gainX, UX
        else:
            chosen, sigma, U = Y, gainY, UY

        # Quy tắc nhận phần tử
        thresh = (f(G, U) / B) if B > 0 else float("inf")
        if (sigma / c_e) >= thresh:
            chosen.add(G, e)
            if chosen.current_cost() >= B:
                if chosen.window_len() == 2 * w:
                    chosen.delete_first_w_segments(w)
                chosen.advance_segment()

        # Theo dõi e* (phần tử đơn tốt nhất)
        val_e = f(G, {e})
        if val_e > e_star_val:
            e_star_val, e_star = val_e, e

        # Cập nhật peak bytes
        cur_bytes = _candidate_bytes(X) + _candidate_bytes(Y)
        if cur_bytes > mem_bytes_peak:
            mem_bytes_peak = cur_bytes

    # Hậu xử lý: chọn lời giải
    XU, YU = X.union_set(), Y.union_set()
    cost_XU, cost_YU = set_cost(G, XU), set_cost(G, YU)

    if cost_XU <= B and cost_YU <= B:
        fX, fY = f(G, XU), f(G, YU)
        if fX >= fY: Q_star, fQ, cost_Q = XU, fX, cost_XU
        else:        Q_star, fQ, cost_Q = YU, fY, cost_YU
    else:
        X_u = XU if cost_XU <= B else X.bounded_suffix(G, B)
        Y_u = YU if cost_YU <= B else Y.bounded_suffix(G, B)
        cand = [
            ("X(u)", X_u, f(G, X_u), set_cost(G, X_u)),
            ("Y(u)", Y_u, f(G, Y_u), set_cost(G, Y_u)),
            ("e*", {e_star} if e_star is not None else set(),
                   f(G, {e_star}) if e_star is not None else float("-inf"),
                   node_cost(G, e_star) if e_star is not None else 0.0)
        ]
        _, Q_star, fQ, cost_Q = max(cand, key=lambda x: x[2])

    stats = {"queries": f.count - queries_before, "mem_bytes_peak": mem_bytes_peak}
    return Q_star, XU, YU, fQ, cost_Q, stats


def run_one_B(args_tuple):
    """Wrapper chạy 1 mốc B. Trả về dict đúng fieldnames để ghi CSV ngay."""
    G, f_class, f_name, B_factor, w, stream_order = args_tuple
    f_wrapper = f_class(G)
    total_cost = total_node_cost(G)
    B = float(B_factor) * total_cost

    start = time.time()
    Q_star, XU, YU, fQ, cost_Q, stats = DCS_streaming(
        G, f_wrapper, B=B, w=w, stream_nodes=stream_order
    )
    runtime = time.time() - start

    print(
        f"✅ [{f_name}] B={B:.4f} (factor={B_factor}, total_cost={total_cost:.4f}) "
        f"f(Q*)={fQ:.4f}, |Q*|={len(Q_star)}, cost(Q*)={cost_Q:.4f}, "
        f"queries={stats['queries']}, time={runtime:.2f}s, mem_bytes_peak={stats['mem_bytes_peak']}"
    )

    # Trả về đúng tiêu chí thống kê yêu cầu
    return {
        "func": f_name,
        "B_factor": B_factor,
        "B_value": B,
        "f_value": fQ,
        "size": len(Q_star),
        "cost": cost_Q,
        "queries": stats["queries"],
        "runtime": runtime,
        "mem_bytes_peak": stats["mem_bytes_peak"],
    }


# =========================
# Main
# =========================
def _write_header_if_needed(csv_path, writer, fieldnames):
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
    p = argparse.ArgumentParser(description="DCS (Dual Candidates 1-pass Streaming) — save-as-you-go CSV.")
    p.add_argument("--graph", type=str, required=True)
    p.add_argument("--func", type=str, default="revenue", help="revenue | coverage | influence | maxcut")
    p.add_argument("--B_factors", type=float, nargs="+", default=[0.1, 0.15, 0.2, 0.25, 0.3])
    p.add_argument("--w", type=int, default=2)
    p.add_argument("--order", type=str, default="natural",
                   help="Thứ tự stream: natural | shuffled (shuffled cần random seed riêng nếu muốn tái lập)")
    p.add_argument("--output", type=str, default="results_dcs.csv")
    p.add_argument("--n_jobs", type=int, default=-1,
                   help="-1: dùng hết core; 1: chạy tuần tự (an toàn nhất cho debug/log).")
    p.add_argument("--append", action="store_true",
                   help="Ghi nối vào file có sẵn, không xoá kết quả cũ.")
    args = p.parse_args()

    with open(args.graph, "rb") as f_in:
        G = pickle.load(f_in)

    func_map = {
        "revenue": RevenueFunction,
        "coverage": CoverageFunction,
        "influence": InfluenceFunction,
        "maxcut": MaxCutFunction
    }
    if args.func not in func_map:
        raise ValueError(f"Unknown function '{args.func}'. Choose from: {list(func_map.keys())}")

    f_class = func_map[args.func]
    f_name = f_class(G).name

    # Thứ tự stream (hiện giữ natural)
    stream_order = list(G.nodes())

    # Chuẩn bị inputs cho Pool
    n_jobs = cpu_count() if args.n_jobs == -1 else max(1, args.n_jobs)
    pool_inputs = [(G, f_class, f_name, b, args.w, stream_order) for b in args.B_factors]

    # Tiêu chí thống kê (đúng như yêu cầu để copy nhanh)
    fieldnames = ["func", "B_factor", "B_value", "f_value", "size", "cost", "queries", "runtime", "mem_bytes_peak"]

    # Mở file CSV theo chế độ
    open_mode = "a" if args.append else "w"
    os.makedirs(os.path.dirname(args.output), exist_ok=True) if os.path.dirname(args.output) else None

    # Save-as-you-go: ghi từng dòng ngay khi xong 1 mốc B (+ flush + fsync)
    with open(args.output, open_mode, newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        _write_header_if_needed(args.output, writer, fieldnames)

        # Chạy tuần tự (đề phòng môi trường không thân thiện multiprocessing)
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
                    # Không ghi dòng lỗi để giữ đúng schema; tiếp tục mốc khác
                    print(f"❌ Lỗi ở B_factor={inp[3]}: {ex}", file=sys.stderr)
            print(f"📂 Results saved to {args.output}")
            sys.exit(0)

        # Chạy song song an toàn, ghi kết quả ngay khi có
        try:
            with Pool(processes=min(len(pool_inputs), n_jobs), initializer=_install_sigint_handler) as pool:
                for res in pool.imap_unordered(run_one_B, pool_inputs):
                    # Có thể res None nếu worker gặp lỗi và đã được bắt; kiểm tra chắc chắn
                    if res is not None:
                        writer.writerow(res)
                        csvfile.flush(); os.fsync(csvfile.fileno())
        except KeyboardInterrupt:
            print("\n⛔️ Bị huỷ bởi người dùng. Các kết quả đã viết vẫn được giữ lại.")
        except Exception as ex:
            print(f"❌ Lỗi khi chạy multiprocessing: {ex}", file=sys.stderr)

    print(f"📂 Results saved to {args.output}")

#python dcs_streaming.py --graph graph.pkl --func revenue --B_factors 0.1 0.15 0.2 0.25 0.3 --w 2 --output results_dcs.csv --append