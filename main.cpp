// src/main.cpp (Linux-only) - SimpleGreedy + FairGreedy + kGreedyIS + kGreedyTS + FkSM-Alg under Partition Matroid
#include <iostream>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <vector>
#include <cctype>
#include <cstdint>
#include <cmath>      // std::floor
#include <algorithm>  // std::max

#include <sys/resource.h>
#include <sys/stat.h>

#include "mygraph.h"
#include "kfunctions.h"
#include "kfunctions_impl.h"

#include "matroid.h" // includes partition matroid implementation

#include "algs/result.h"
#include "algs/simple_greedy.h"
#include "algs/fair_greedy.h"
#include "algs/k_greedy_is.h"
#include "algs/k_greedy_ts.h"   // NEW
#include "algs/fksm_alg.h"

// peak RSS (KB) on Linux
static long getPeakRSS_KB() {
    struct rusage r;
    if (getrusage(RUSAGE_SELF, &r) == 0) return r.ru_maxrss;
    return 0;
}

// CSV helpers
static bool file_is_empty_or_missing(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return true;
    return st.st_size == 0;
}

static std::string csv_escape(const std::string &s) {
    bool need_quote = false;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') { need_quote = true; break; }
    }
    if (!need_quote) return s;

    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static void append_csv_row(const std::string &csv_path,
                           const std::string &header,
                           const std::string &row)
{
    if (csv_path.empty()) return;

    const bool write_header = file_is_empty_or_missing(csv_path);

    std::ofstream fout(csv_path, std::ios::out | std::ios::app);
    if (!fout.good()) {
        std::cerr << "[WARN] Cannot open CSV for append: " << csv_path << "\n";
        return;
    }
    if (write_header) fout << header << "\n";
    fout << row << "\n";
}

// FairError(x) = sum_{i in [k]} max{ |X_i|-u_i, l_i-|X_i|, 0 }
static double compute_fair_error(const ksub::Assignment& x,
                                 std::size_t K,
                                 const std::vector<std::size_t>& l_limits,
                                 const std::vector<std::size_t>& u_limits)
{
    std::vector<std::size_t> cnt(K + 1, 0);
    for (std::size_t u = 0; u < x.size(); ++u) {
        const auto lab = static_cast<std::size_t>(x[u]);
        if (lab >= 1 && lab <= K) cnt[lab]++;
    }
    long long sum = 0;
    for (std::size_t i = 1; i <= K; ++i) {
        const long long a = static_cast<long long>(cnt[i]) - static_cast<long long>(u_limits[i]);
        const long long b = static_cast<long long>(l_limits[i]) - static_cast<long long>(cnt[i]);
        long long m = 0;
        if (a > m) m = a;
        if (b > m) m = b;
        sum += m;
    }
    return static_cast<double>(sum);
}

static bool ends_with_ci(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    const std::size_t off = s.size() - suf.size();
    for (std::size_t i = 0; i < suf.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(s[off + i]);
        const unsigned char b = static_cast<unsigned char>(suf[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

static void print_usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " graph.bin matroid_factor B_factor algo [options] [csv_path]\n\n"
        << "  graph.bin       : tinyGraph .bin (NEW FORMAT includes part_id)\n"
        << "  matroid_factor  : cap[j] = floor(matroid_factor * |P_j|)\n"
        << "  B_factor        : B = floor(B_factor * |V|) (for fairness bounds)\n"
        << "  algo            : sg|simple_greedy | fg|fair_greedy | kgis|k_greedy_is | kgts|k_greedy_ts | fksm|fksm_alg\n\n"
        << "Options:\n"
        << "  --eps <double>          (FkSM only, default 0.1)\n"
        << "  --seed <uint64>         (FkSM + kGreedyIS + kGreedyTS, default 42)\n"
        << "  --nonmonotone <0|1>     (default 0)   // set 1 for Max-k-Cut, etc.\n"
        << "  csv_path                (optional) append results\n\n"
        << "Notes:\n"
        << "  - Fairness bounds are set by paper rule using B:\n"
        << "      l_i = floor(0.8 * B / K),  u_i = floor(1.4 * B / K).\n"
        << "  - Partition matroid is enforced on supp(x) = {u : x[u] != 0}.\n"
        << "  - FairGreedy enforces ONLY the upper bounds |S_i|<=u_i during selection;\n"
        << "    it does not try to satisfy the lower bounds l_i.\n"
        << "  - kGreedyIS: preprocess picks a random maximum independent set V1 under the\n"
        << "    partition matroid, then greedy selects only from V1 with |S_i|<=u_i.\n"
        << "  - kGreedyTS: preprocess picks V1 similarly, then greedy selects B = sum_i u_i elements\n"
        << "    from V1 (no per-label bounds; only |supp(x)|=B).\n";
}

// cap[j] = floor(matroid_factor * |P_j|)
static matroid::Cap build_cap_from_factor(const mygraph::tinyGraph& g, double matroid_factor) {
    const std::size_t p = matroid::derive_p(g);
    matroid::Cap cap(p, 0);

    std::vector<std::size_t> part_size(p, 0);
    for (std::size_t u = 0; u < g.n; ++u) {
        const std::size_t pid = static_cast<std::size_t>(g.part_id[u]);
        if (pid < p) part_size[pid]++;
    }

    for (std::size_t j = 0; j < p; ++j) {
        const double raw = matroid_factor * static_cast<double>(part_size[j]);
        std::size_t cj = static_cast<std::size_t>(std::floor(raw));
        if (cj > part_size[j]) cj = part_size[j];
        cap[j] = cj;
    }
    return cap;
}

static std::size_t sum_cap_local(const matroid::Cap& cap) {
    std::size_t s = 0;
    for (auto c : cap) s += c;
    return s;
}

enum class AlgoId : int {
    UNKNOWN = 0,
    SIMPLE_GREEDY = 1,
    FAIR_GREEDY = 2,
    K_GREEDY_IS = 3,
    K_GREEDY_TS = 4,
    FKSM = 5,
};

static AlgoId parse_algo_id(const std::string &algo) {
    if (algo == "sg" || algo == "simple_greedy") return AlgoId::SIMPLE_GREEDY;
    if (algo == "fg" || algo == "fair_greedy")   return AlgoId::FAIR_GREEDY;
    if (algo == "kgis" || algo == "k_greedy_is" || algo == "kgreedyis") return AlgoId::K_GREEDY_IS;
    if (algo == "kgts" || algo == "k_greedy_ts" || algo == "kgreedyts") return AlgoId::K_GREEDY_TS;
    if (algo == "fksm" || algo == "fksm_alg")    return AlgoId::FKSM;
    return AlgoId::UNKNOWN;
}

int main(int argc, char** argv) {
    using namespace std;
    using namespace mygraph;

    if (argc < 5) {
        print_usage(argv[0]);
        return 1;
    }

    string graphFile = argv[1];
    const double matroid_factor = std::stod(argv[2]);
    const double B_factor = std::stod(argv[3]);
    string algo_str = argv[4];

    // options (default)
    double eps = 0.1;                 // FkSM
    std::uint64_t seed = 42ULL;       // FkSM + kGreedyIS + kGreedyTS (V1 randomization)
    bool is_non_monotone = false;     // affects FkSM internal behavior; others can ignore

    string csv_path;

    // parse remaining args
    for (int i = 5; i < argc; ++i) {
        string tok = argv[i];

        auto need_next = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) {
                cerr << "Error: missing value for " << opt << "\n";
                exit(1);
            }
            return argv[++i];
        };

        if (tok == "--eps") {
            eps = std::strtod(need_next("--eps"), nullptr);
        } else if (tok == "--seed") {
            seed = static_cast<std::uint64_t>(std::strtoull(need_next("--seed"), nullptr, 10));
        } else if (tok == "--nonmonotone") {
            const int v = std::atoi(need_next("--nonmonotone"));
            is_non_monotone = (v != 0);
        } else if (!tok.empty() && tok[0] != '-' && ends_with_ci(tok, ".csv")) {
            csv_path = tok;
        } else if (!tok.empty() && tok[0] != '-' && csv_path.empty() && i == argc - 1) {
            csv_path = tok;
        } else {
            cerr << "[WARN] Unknown arg ignored: " << tok << "\n";
        }
    }

    tinyGraph g;
    if (!g.read_binary(graphFile)) {
        cerr << "Error: cannot read binary graph from " << graphFile << "\n";
        return 1;
    }

    cout << "Graph loaded: n = " << g.n
         << ", m = " << g.m
         << ", K = " << g.K
         << ", undirected = " << (g.undirected ? "true" : "false")
         << "\n";
    cout << "algo = " << algo_str << "\n";
    cout << "matroid_factor = " << matroid_factor << "\n";
    cout << "B_factor = " << B_factor << "\n";
    cout << "is_non_monotone = " << (is_non_monotone ? "true" : "false") << "\n";

    // Build partition matroid cap
    const std::size_t p = matroid::derive_p(g);
    matroid::Cap cap = build_cap_from_factor(g, matroid_factor);
    const std::size_t Bm = sum_cap_local(cap);

    cout << "Partition parts p = " << p << "\n";
    cout << "Matroid rank budget Bm = sum_j cap[j] = " << Bm << "\n";

    // Paper-style budget B (for fairness bounds only)
    const std::size_t B = static_cast<std::size_t>(std::floor(B_factor * static_cast<double>(g.n)));
    cout << "Fairness budget B = floor(B_factor * |V|) = " << B << "\n";

    // fairness bounds from paper: l_i = floor(0.8*B/K), u_i = floor(1.4*B/K)
    std::vector<std::size_t> l_limits(g.K + 1, 0), u_limits(g.K + 1, 0);
    if (g.K > 0) {
        const std::size_t li = static_cast<std::size_t>(std::floor(0.8 * (double)B / (double)g.K));
        const std::size_t ui = static_cast<std::size_t>(std::floor(1.4 * (double)B / (double)g.K));
        for (std::size_t i = 1; i <= g.K; ++i) {
            l_limits[i] = li;
            u_limits[i] = ui;
        }
    }

    cout << "Fairness bounds (uniform): ";
    if (g.K > 0) {
        cout << "l_i=" << l_limits[1] << ", u_i=" << u_limits[1] << " for all i in [k]\n";
    } else {
        cout << "K=0 (no labels)\n";
    }

    // peak RSS after graph load (to subtract graph memory)
    const long peak_before_kb = getPeakRSS_KB();

    // dispatch
    algs::Result res;
    const AlgoId algo_id = parse_algo_id(algo_str);

    switch (algo_id) {
        case AlgoId::SIMPLE_GREEDY:
            res = algs::run_simple_greedy(g, cap);
            break;

        case AlgoId::FAIR_GREEDY: {
            // fair_greedy takes only upper bounds per label: size K with indices 0..K-1
            std::vector<std::size_t> u_upper(g.K, 0);
            for (std::size_t i = 1; i <= g.K; ++i) u_upper[i - 1] = u_limits[i];
            res = algs::run_fair_greedy(g, cap, u_upper);
            break;
        }

        case AlgoId::K_GREEDY_IS: {
            // k_greedy_is uses upper bounds B_i = u_i, and internal random V1 controlled by seed
            std::vector<std::size_t> u_upper(g.K, 0);
            for (std::size_t i = 1; i <= g.K; ++i) u_upper[i - 1] = u_limits[i];
            res = algs::run_k_greedy_is(g, cap, u_upper);
            break;
        }

        case AlgoId::K_GREEDY_TS: {
            // k_greedy_ts uses B = sum_i u_i (no per-label bounds), and internal random V1 controlled by seed
            std::vector<std::size_t> u_upper(g.K, 0);
            for (std::size_t i = 1; i <= g.K; ++i) u_upper[i - 1] = u_limits[i];
            res = algs::run_k_greedy_ts(g, cap, u_upper);
            break;
        }

        case AlgoId::FKSM:
            res = algs::run_fksm_alg(g, cap, u_limits, l_limits, eps, seed);
            break;

        default:
            cerr << "Error: unknown algo = " << algo_str << "\n";
            print_usage(argv[0]);
            return 1;
    }

    // memory used by algorithm (peak delta after load)
    const long peak_after_kb = getPeakRSS_KB();
    if (peak_after_kb > peak_before_kb) res.mem_mb = (peak_after_kb - peak_before_kb) / 1024.0;
    else res.mem_mb = 0.0;

    // Error metrics (use the same reporting metric everywhere)
    res.fair_error = compute_fair_error(res.x, g.K, l_limits, u_limits);

    // main verifies matroid violation once (accounting)
    res.matroid_error = static_cast<double>(matroid::matroid_violation(g, res.x, cap));
    res.matroid_checks += 1;

    res.total_error = res.fair_error + res.matroid_error;

    cout << fixed << setprecision(6);
    cout << "-----------------------------\n";
    cout << res.algo << " finished.\n";
    cout << "constraint          = " << res.constraint << "\n";
    cout << "f(x)                = " << res.f_value << "\n";
    cout << "FairError(x)        = " << res.fair_error << "\n";
    cout << "MatroidError(x)     = " << res.matroid_error << "\n";
    cout << "TotalError(x)       = " << res.total_error << "\n";
    cout << "#calls (f/marg)     = " << res.queries << "\n";
    cout << "#calls (matroid)    = " << res.matroid_checks << "\n";
    cout << "elapsed time (s)    = " << res.time_sec << "\n";
    cout << "memory used (MB)    = " << res.mem_mb << "\n";

    // CSV append
    if (!csv_path.empty()) {
        const string header =
            "algo,constraint,matroid_factor,B_factor,B,Bm,is_non_monotone,eps,seed,"
            "f_value,fair_error,matroid_error,total_error,queries,matroid_checks,time_sec,mem_mb";

        ostringstream row;
        row << csv_escape(res.algo) << ","
            << csv_escape(res.constraint) << ","
            << setprecision(17) << matroid_factor << ","
            << setprecision(17) << B_factor << ","
            << B << ","
            << Bm << ","
            << (is_non_monotone ? 1 : 0) << ","
            << setprecision(17) << eps << ","
            << seed << ","
            << setprecision(17) << res.f_value << ","
            << setprecision(17) << res.fair_error << ","
            << setprecision(17) << res.matroid_error << ","
            << setprecision(17) << res.total_error << ","
            << res.queries << ","
            << res.matroid_checks << ","
            << setprecision(10) << res.time_sec << ","
            << setprecision(6) << res.mem_mb;

        append_csv_row(csv_path, header, row.str());
    }

    return 0;
}
