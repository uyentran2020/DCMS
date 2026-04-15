# mp_rgmax_1k.py  — Multi-pass RG+Max for 1-knapsack, save-as-you-go (9-field schema)
import argparse, csv, math, os, pickle, sys, time, signal
from multiprocessing import Pool, cpu_count
import networkx as nx

# === Hàm f theo SubmodularFunction (f(G, S) với S là set) ===
from functions.revenue_function import RevenueFunction
from functions.coverage_function import CoverageFunction
from functions.influence_function import InfluenceFunction
from functions.maxcut_function import MaxCutFunction


# =========================
# Tiện ích chung
# =========================
def node_cost(G, e):
    return float(G.nodes[e].get("weight", 1.0))

def set_cost(G, S):
    return sum(node_cost(G, u) for u in S)

def total_node_cost(G):
    return sum(node_cost(G, u) for u in G.nodes())

def _bytes_of_sets(sets):
    """Ước lượng byte của danh sách/tuple các set."""
    total = 0
    for S in sets:
        total += sys.getsizeof(S)
        for x in S:
            total += sys.getsizeof(x)
    return total


# =========================
# Algorithm 9: Multi-pass streaming (1-knapsack)
# =========================
def multi_pass_streaming_1k(G: nx.DiGraph, f, B: float, m_init: float, eps: float,
                            stream_nodes=None):
    """
    Cài đặt đúng theo Algorithm 9 (đã rút gọn cho 1-knapsack).
    Trả về: (A, G_sel, stats) với:
      - G_sel: nghiệm sau giai đoạn thresholding (danh sách theo thứ tự thêm),
      - A: nghiệm sau giai đoạn augmentation,
      - stats: các chỉ số phụ trợ (không dùng queries ở đây; queries đo ở thuật toán 10).
    """
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    # Khởi tạo
    tau = max(m_init, 0.0)
    G_sel_order = []   # giữ thứ tự chọn trong thresholding
    G_sel = set()      # bản set của G_sel_order

    mem_bytes_peak = _bytes_of_sets([G_sel])  # theo dõi bộ nhớ ứng viên (G_sel và a_i)

    # --- Thresholding stage ---
    # d=1 => dừng khi tau <= m/b (m là m_init)
    stop_tau = (m_init / B) if B > 0 else 0.0

    while tau > stop_tau:
        # Một pass qua stream
        for e in stream_nodes:
            if e in G_sel:   # đã chọn
                continue
            if e not in G:
                continue
            ce = node_cost(G, e)
            if ce <= 0:
                continue
            # kiểm tra mật độ & knapsack
            gain = f(G, G_sel | {e}) - f(G, G_sel)
            if (gain / ce) >= tau and (set_cost(G, G_sel) + ce) <= B:
                G_sel.add(e)
                G_sel_order.append(e)

                # cập nhật peak bộ nhớ (ứng viên)
                cur_bytes = _bytes_of_sets([G_sel])
                if cur_bytes > mem_bytes_peak:
                    mem_bytes_peak = cur_bytes
        # giảm ngưỡng
        tau = tau / (1.0 + eps)

    # G_i: i phần tử đầu trong G_sel_order (i từ 1..|G_sel_order|)
    # a_i: phần tử augmentation đi kèm G_{i-1}
    k = len(G_sel_order)
    a = [set() for _ in range(k + 1)]  # a[0]..a[k]; dùng a[i] cho G_{i-1}
    # cập nhật peak (ứng viên = G_sel + các a_i)
    cur_bytes = _bytes_of_sets([G_sel] + a)
    if cur_bytes > mem_bytes_peak:
        mem_bytes_peak = cur_bytes

    # --- Augmentation stage ---
    # Một pass mới qua stream
    for e in stream_nodes:
        if e not in G:
            continue
        ce = node_cost(G, e)
        if ce <= 0:
            continue

        # l = max{i | c(G_i) + c(e) <= B}
        # chú ý: G_i dùng FIRST i items trong G_sel_order
        total = 0.0
        l = 0
        for i in range(1, k + 1):
            total += node_cost(G, G_sel_order[i - 1])
            if total + ce <= B:
                l = i
            else:
                break

        # duyệt i = 1..l, nếu f(G_{i-1} ∪ a_i) < f(G_{i-1} ∪ {e}) thì a_i = {e}
        # G_{i-1} là FIRST (i-1) items
        prefix = set()  # sẽ tăng dần
        for i in range(1, l + 1):
            # thêm phần tử thứ (i-1) vào prefix
            if i > 1:
                prefix.add(G_sel_order[i - 2])

            if f(G, prefix | a[i]) < f(G, prefix | {e}):
                a[i] = {e}

            # theo dõi bộ nhớ (ứng viên augmentation)
            cur_bytes = _bytes_of_sets([G_sel] + a)
            if cur_bytes > mem_bytes_peak:
                mem_bytes_peak = cur_bytes

    # A = argmax_i f(G_{i-1} ∪ a_i)
    best_A = set()
    best_val = float("-inf")
    prefix = set()
    for i in range(1, k + 1):
        if i > 1:
            prefix.add(G_sel_order[i - 2])   # build G_{i-1}
        S = prefix | a[i]
        val = f(G, S)
        if val > best_val:
            best_val = val
            best_A = set(S)

    stats = {
        "tau_stages": max(0, int(math.ceil(math.log(max(1.0, m_init / max(stop_tau, 1e-12)),
                                                    1.0 + eps)))) if B > 0 else 0,
        "len_G": len(G_sel_order),
        "mem_bytes_peak": mem_bytes_peak,
    }
    return best_A, set(G_sel), stats


# =========================
# Algorithm 10: Multi-pass streaming repeat Greedy+Max (1-knapsack)
# =========================
def mp_repeat_greedy_max_1k(G: nx.DiGraph, f, B: float, eps: float, stream_nodes=None):
    """
    Cài đặt theo Algorithm 10, sử dụng Algorithm 9 (hàm multi_pass_streaming_1k ở trên).
    Trả về (S_best, f(S_best), cost(S_best), stats) với stats chứa:
      - queries: tổng số truy vấn oracle sử dụng cho toàn bộ thuật toán 10 (gồm cả hai lần gọi Alg.9),
      - mem_bytes_peak: peak từ 2 lượt Alg.9,
      - các chỉ số phụ khác (m1, m2, tau_stages_1/2, len_G1/2).
    """
    if stream_nodes is None:
        stream_nodes = list(G.nodes())

    # Đếm số truy vấn oracle tổng thể cho toàn bộ thuật toán 10
    queries_before = f.count

    # Pass đầu: tính m1 = max_e f({e})
    m1 = 0.0
    for e in stream_nodes:
        if e in G:
            m1 = max(m1, f(G, {e}))

    # Gọi Multi-Pass Streaming lần 1
    S1, Gsel1, stats1 = multi_pass_streaming_1k(G, f, B=B, m_init=m1, eps=eps, stream_nodes=stream_nodes)

    # Pass tiếp theo: chạy trên N \ Gsel1
    stream2 = [e for e in stream_nodes if e not in Gsel1]

    # m2 = max_e f({e}) trên stream2
    m2 = 0.0
    for e in stream2:
        if e in G:
            m2 = max(m2, f(G, {e}))

    # Gọi Multi-Pass Streaming lần 2 trên phần còn lại
    S2, Gsel2, stats2 = multi_pass_streaming_1k(G, f, B=B, m_init=m2, eps=eps, stream_nodes=stream2)

    # Unconstrained(G,f): no-op đơn giản — chọn Gsel1 (có thể thay bằng thủ tục riêng)
    S3 = set(Gsel1)

    # Chọn nghiệm tốt nhất
    cand = [
        ("S1", S1, f(G, S1), set_cost(G, S1)),
        ("S2", S2, f(G, S2), set_cost(G, S2)),
        ("S3", S3, f(G, S3), set_cost(G, S3)),
    ]
    name, S_best, f_best, c_best = max(cand, key=lambda x: x[2])

    # Truy vấn oracle đã dùng
    queries_used = f.count - queries_before

    stats = {
        "m1": m1, "m2": m2,
        "len_G1": len(Gsel1), "len_G2": len(Gsel2),
        "mem_bytes_peak": max(stats1["mem_bytes_peak"], stats2["mem_bytes_peak"]),
        "tau_stages_1": stats1["tau_stages"], "tau_stages_2": stats2["tau_stages"],
        "queries": queries_used,
    }
    return S_best, f_best, c_best, stats


# =========================
# Worker cho 1 giá trị B_factor
# =========================
def run_one_B(args_tuple):
    G, f_class, f_name, B_factor, eps, stream_order = args_tuple

    f_wrapper = f_class(G)
    total_cost = total_node_cost(G)
    B = float(B_factor) * total_cost

    start = time.time()
    S_best, f_best, c_best, stats = mp_repeat_greedy_max_1k(
        G, f_wrapper, B=B, eps=eps, stream_nodes=stream_order
    )
    runtime = time.time() - start

    print(
        f"✅ [{f_name}] MP-RG+Max B={B:.4f} (factor={B_factor}, total_cost={total_cost:.4f}) "
        f"f(S*)={f_best:.4f}, |S*|={len(S_best)}, cost(S*)={c_best:.4f}, "
        f"passes1={stats['tau_stages_1']}, passes2={stats['tau_stages_2']}, "
        f"queries={stats['queries']}, time={runtime:.2f}s, mem_bytes_peak={stats['mem_bytes_peak']}"
    )

    # Trả về đúng 9 cột chuẩn
    return {
        "func": f_name,
        "B_factor": B_factor,
        "B_value": B,
        "f_value": f_best,
        "size": len(S_best),
        "cost": c_best,
        "queries": stats["queries"],
        "runtime": runtime,
        "mem_bytes_peak": stats["mem_bytes_peak"],
    }


# =========================
# Save-as-you-go CSV helpers
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


# =========================
# Main
# =========================
if __name__ == "__main__":
    p = argparse.ArgumentParser(
        description="Multi-pass Streaming Repeat Greedy + Max (1-Knapsack) — save-as-you-go CSV."
    )
    p.add_argument("--graph", type=str, required=True, help="File .pkl chứa đồ thị")
    p.add_argument("--func", type=str, default="revenue", help="revenue | coverage | influence | maxcut")
    p.add_argument("--B_factors", type=float, nargs="+",
                   default=[0.1, 0.15, 0.2, 0.25, 0.3],
                   help="B = factor × (tổng chi phí nút)")
    p.add_argument("--eps", type=float, default=0.1, help="epsilon > 0, ví dụ 0.1")
    p.add_argument("--order", type=str, default="natural",
                   help="Thứ tự stream: natural (giữ nguyên)")
    p.add_argument("--output", type=str, default="results_mprgmax1k.csv")
    p.add_argument("--n_jobs", type=int, default=-1)
    p.add_argument("--append", action="store_true")
    args = p.parse_args()

    # Load graph
    with open(args.graph, "rb") as f_in:
        G = pickle.load(f_in)

    # Chọn hàm f
    func_map = {"revenue": RevenueFunction, "coverage": CoverageFunction, "influence": InfluenceFunction, "maxcut": MaxCutFunction}
    if args.func not in func_map:
        raise ValueError(f"Unknown function '{args.func}'. Choose from: {list(func_map.keys())}")
    f_class = func_map[args.func]
    f_name = f_class(G).name

    # Thứ tự stream
    if args.order == "natural":
        stream_order = list(G.nodes())
    else:
        # Có thể mở rộng thêm: random, degree, weight-desc, v.v.
        raise ValueError(f"Unsupported --order '{args.order}'. Hiện chỉ hỗ trợ 'natural'.")

    # Multiprocessing theo các B_factors
    n_jobs = cpu_count() if args.n_jobs == -1 else max(1, args.n_jobs)
    pool_inputs = [(G, f_class, f_name, b, args.eps, stream_order) for b in args.B_factors]

    # CSV save-as-you-go — đúng bộ 9 tiêu chí
    fieldnames = ["func","B_factor","B_value","f_value","size","cost","queries","runtime","mem_bytes_peak"]

    # Chuẩn bị file
    open_mode = "a" if args.append else "w"
    os.makedirs(os.path.dirname(args.output), exist_ok=True) if os.path.dirname(args.output) else None

    with open(args.output, open_mode, newline="", encoding="utf-8") as f_out:
        writer = csv.DictWriter(f_out, fieldnames=fieldnames)
        _write_header_if_needed(args.output, writer)

        if n_jobs == 1 or len(pool_inputs) == 1:
            # Chạy tuần tự (debug/đơn giản)
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

        # Chạy song song, ghi ngay từng kết quả
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
