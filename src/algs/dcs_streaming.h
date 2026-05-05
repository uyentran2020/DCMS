// src/algs/dcs_streaming.h
#ifndef ALGS_DCS_STREAMING_H
#define ALGS_DCS_STREAMING_H

#include <vector>
#include <deque>
#include <cstdint>
#include <limits>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

#include "mygraph.h"
#include "sfunctions.h"
#include "algs/result.h"

namespace algs {

using Clock = std::chrono::high_resolution_clock;
using mygraph::node_id;
using subm::Solution;

// ===== internal helpers (avoid collisions across headers) =====
namespace dcs_detail {

inline double node_cost(const mygraph::tinyGraph &g, node_id u) {
    return g.nodes[u].weight;
}

inline double cost_of(const mygraph::tinyGraph &g, const Solution &inS) {
    double c = 0.0;
    for (std::size_t u = 0; u < g.n; ++u) if (inS[u]) c += g.nodes[u].weight;
    return c;
}

inline std::size_t solution_size(const Solution &inS) {
    std::size_t s = 0;
    for (auto b : inS) if (b) ++s;
    return s;
}

// Candidate for DCMS (keep members list to iterate later if needed)
struct Cand {
    Solution inS;
    double f = 0.0;
    double c = 0.0;
    std::vector<node_id> members;

    void init(std::size_t n, double f_empty) {
        inS.assign(n, 0);
        f = f_empty;
        c = 0.0;
        members.clear();
        members.reserve(n / 10 + 8);
    }
};

} // namespace dcs_detail

// =============================================================
// 1) DCS (Dual Candidates, 1-pass streaming) – union(X) vs union(Y) only
// =============================================================
struct DCSCandidate {
    Solution inS;                    // union of kept segments
    double   fS = 0.0;               // f(inS)
    double   cS = 0.0;               // cost(inS)

    std::deque<std::vector<node_id>> segments;
    std::deque<double> seg_cost;

    void init(std::size_t n) {
        inS.assign(n, 0);
        fS = 0.0;
        cS = 0.0;
        segments.clear();
        seg_cost.clear();
        segments.emplace_back();
        seg_cost.emplace_back(0.0);
    }

    std::size_t num_segments() const { return segments.size(); }

    void new_segment() {
        segments.emplace_back();
        seg_cost.emplace_back(0.0);
    }

    void drop_first_segments_rebuild(const mygraph::tinyGraph &g,
                                     std::uint64_t &queries,
                                     std::size_t k)
    {
        while (k > 0 && !segments.empty()) {
            segments.pop_front();
            seg_cost.pop_front();
            --k;
        }

        const std::size_t n = inS.size();
        inS.assign(n, 0);
        cS = 0.0;

        for (const auto &seg : segments) {
            for (node_id u : seg) {
                if (!inS[u]) {
                    inS[u] = 1;
                    cS += dcs_detail::node_cost(g, u);
                }
            }
        }

        fS = subm::sfunc_evaluate(g, inS);
        queries += 1;

        if (segments.empty()) {
            segments.emplace_back();
            seg_cost.emplace_back(0.0);
        }
    }

    bool can_add(const mygraph::tinyGraph &g, node_id e, double B) const {
        const double ce = dcs_detail::node_cost(g, e);
        if (ce <= 0.0) return false;
        if (inS[e]) return false;
        return (cS + ce <= B);
    }

    void add(const mygraph::tinyGraph &g, node_id e, double delta) {
        const double ce = dcs_detail::node_cost(g, e);
        inS[e] = 1;
        cS += ce;
        fS += delta;

        if (segments.empty()) {
            segments.emplace_back();
            seg_cost.emplace_back(0.0);
        }
        segments.back().push_back(e);
        seg_cost.back() += ce;
    }
};

struct DCSUnions {
    Solution X;
    Solution Y;
    double fX = 0.0;
    double fY = 0.0;
};

// DCS first pass returning unions (needed by DCMS)
inline DCSUnions run_dcs_unions_first_pass(const mygraph::tinyGraph &g,
                                          double B,
                                          std::size_t w,
                                          std::uint64_t &queries)
{
    if (w < 1) throw std::invalid_argument("DCS: w must be >= 1.");

    DCSCandidate Xc, Yc;
    Xc.init(g.n);
    Yc.init(g.n);

    Xc.fS = subm::sfunc_evaluate(g, Xc.inS); queries += 1;
    Yc.fS = subm::sfunc_evaluate(g, Yc.inS); queries += 1;

    for (node_id e = 0; e < g.n; ++e) {
        const double ce = dcs_detail::node_cost(g, e);
        if (ce <= 0.0 || ce > B) continue;

        double best_md = -std::numeric_limits<double>::infinity();
        int choose = 0;
        double best_delta = 0.0;

        if (Xc.can_add(g, e, B)) {
            const double d = subm::sfunc_marginal(g, Xc.inS, e, Xc.fS);
            queries += 1;
            const double md = d / ce;
            if (md > best_md) { best_md = md; choose = 1; best_delta = d; }
        }
        if (Yc.can_add(g, e, B)) {
            const double d = subm::sfunc_marginal(g, Yc.inS, e, Yc.fS);
            queries += 1;
            const double md = d / ce;
            if (md > best_md) { best_md = md; choose = 2; best_delta = d; }
        }
        if (choose == 0) continue;

        const double fU = (choose == 1) ? Xc.fS : Yc.fS;
        const double rhs = fU / B;

        if (best_md >= rhs) {
            if (choose == 1) {
                Xc.add(g, e, best_delta);
                if (Xc.num_segments() >= 2 * w) Xc.drop_first_segments_rebuild(g, queries, w);
                if (Xc.cS >= B) Xc.new_segment();
            } else {
                Yc.add(g, e, best_delta);
                if (Yc.num_segments() >= 2 * w) Yc.drop_first_segments_rebuild(g, queries, w);
                if (Yc.cS >= B) Yc.new_segment();
            }
        }
    }

    DCSUnions out;
    out.X = std::move(Xc.inS);
    out.Y = std::move(Yc.inS);
    out.fX = subm::sfunc_evaluate(g, out.X); queries += 1;
    out.fY = subm::sfunc_evaluate(g, out.Y); queries += 1;
    return out;
}

inline Result run_dcs_streaming(const mygraph::tinyGraph &g,
                               double B,
                               std::size_t w)
{
    Result res;
    res.algo = "DCS";
    {
        std::ostringstream oss;
        oss << "B=" << B << ", w=" << w;
        res.constraint = oss.str();
    }

    res.inS.assign(g.n, 0);
    res.queries = 0;

    auto t0 = Clock::now();

    if (g.n == 0 || B <= 0.0) {
        res.f_value = subm::sfunc_evaluate(g, res.inS);
        res.queries += 1;
        res.time_sec = std::chrono::duration<double>(Clock::now() - t0).count();
        return res;
    }

    auto u = run_dcs_unions_first_pass(g, B, w, res.queries);

    if (u.fY > u.fX) {
        res.inS = std::move(u.Y);
        res.f_value = u.fY;
    } else {
        res.inS = std::move(u.X);
        res.f_value = u.fX;
    }

    auto t1 = Clock::now();
    res.time_sec = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

// =============================================================
// 2) DCMS (Dual Candidates for Multi-pass Streaming) – Algorithm 2 in the image
//
// Inputs: w>=1, eps in (0,1), alpha in Z+
// Streams: 3 passes over the same order V = (0..n-1)
// Uses DCS outputs from the first stream.
// =============================================================
inline Result run_dcms_multipass(const mygraph::tinyGraph &g,
                                double B,
                                std::size_t w,
                                double eps,
                                std::size_t alpha)
{
    Result res;
    res.algo = "DCMS";
    {
        std::ostringstream oss;
        oss << "B=" << B << ", w=" << w << ", eps=" << eps << ", alpha=" << alpha;
        res.constraint = oss.str();
    }

    res.inS.assign(g.n, 0);
    res.queries = 0;

    auto t0 = Clock::now();

    if (g.n == 0 || B <= 0.0) {
        res.f_value = subm::sfunc_evaluate(g, res.inS);
        res.queries += 1;
        res.time_sec = std::chrono::duration<double>(Clock::now() - t0).count();
        return res;
    }
    if (w < 1) throw std::invalid_argument("DCMS: w must be >= 1.");
    if (!(eps > 0.0 && eps < 1.0)) throw std::invalid_argument("DCMS: eps must be in (0,1).");
    if (alpha < 1) throw std::invalid_argument("DCMS: alpha must be >= 1.");

    // ---- First stream: DCS ----
    auto unions = run_dcs_unions_first_pass(g, B, w, res.queries);

    // Map to Algorithm-2 notations:
    // (M1, M, M') <- DCS(...)
    // Under our “union-only” DCS, we take:
    //   M  = union(X), M' = union(Y), and M1 = argmax{f(M), f(M')}.
    Solution M  = unions.X;
    Solution Mp = unions.Y;
    double fM  = unions.fX;
    double fMp = unions.fY;

    Solution M1;
    double fM1;
    if (fMp > fM) { M1 = Mp; fM1 = fMp; }
    else          { M1 = M;  fM1 = fM;  }

    // M2 = argmax_{M2 in {M, M'}} f(M2)  (same as M1 here)
    Solution M2 = M1;
    double fM2 = fM1;

    // eps' <- eps/5
    const double eps_p = eps / 1.0;//const double eps_p = eps / 5.0;
    if (!(eps_p > 0.0 && eps_p < 1.0)) throw std::invalid_argument("DCMS: eps/5 must be in (0,1).");

    // Build R:
    // R = { (1-eps')^{-i} | i in Z and  f(M1)/(2B) <= (1-eps')^{-i} <= alpha*f(M2)/(5 eps' B) }
    const double lower = (1.0 - eps_p) * eps_p * fM1 / (2.0 * B);
    const double upper = (static_cast<double>(alpha) * fM2) / (5.0 * eps_p * B);

    std::vector<double> R;
    if (upper >= lower && upper > 0.0) {
        const double ratio = 1.0 / (1.0 - eps_p); // > 1
        // generate rho values geometrically around powers of ratio
        // i_min = ceil(log(lower)/log(ratio)), i_max = floor(log(upper)/log(ratio))
        auto safe_log = [](double x) -> double {
            if (x <= 0.0) return -std::numeric_limits<double>::infinity();
            return std::log(x);
        };
        const double log_r = std::log(ratio);

        long long i_min = static_cast<long long>(std::ceil(safe_log(lower) / log_r));
        long long i_max = static_cast<long long>(std::floor(safe_log(upper) / log_r));

        // clamp to avoid pathological huge loops
        const long long LIM = 1000000LL;
        if (i_min < -LIM) i_min = -LIM;
        if (i_max >  LIM) i_max =  LIM;

        for (long long i = i_min; i <= i_max; ++i) {
            double rho = std::pow(ratio, static_cast<double>(i));
            if (rho + 1e-15 < lower) continue;
            if (rho > upper * (1.0 + 1e-15)) continue;
            R.push_back(rho);
        }
    }

    // If R empty, Algorithm-2 degenerates to returning M1
    if (R.empty()) {
        res.inS = std::move(M1);
        res.f_value = subm::sfunc_evaluate(g, res.inS);
        res.queries += 1;
        res.time_sec = std::chrono::duration<double>(Clock::now() - t0).count();
        return res;
    }

    // Prepare candidates X_rho, Y_rho for each rho
    Solution empty(g.n, 0);
    const double f_empty = subm::sfunc_evaluate(g, empty);
    res.queries += 1;

    struct PairXY {
        double rho;
        dcs_detail::Cand X;
        dcs_detail::Cand Y;
    };

    std::vector<PairXY> pairs;
    pairs.reserve(R.size());
    for (double rho : R) {
        PairXY p;
        p.rho = rho;
        p.X.init(g.n, f_empty);
        p.Y.init(g.n, f_empty);
        pairs.push_back(std::move(p));
    }

    // L* <- M1
    Solution L = M1;
    double cL = dcs_detail::cost_of(g, L);
    double fL = subm::sfunc_evaluate(g, L);
    res.queries += 1;

    // ---- Second stream ----
    for (node_id e = 0; e < g.n; ++e) {
        const double ce = dcs_detail::node_cost(g, e);
        if (ce <= 0.0 || ce > B) continue;

        for (auto &pr : pairs) {
            // A_rho = argmax_{A in {X_rho, Y_rho}} f(e | A)
            // compute both marginals and choose larger
            double dX = 0.0, dY = 0.0;

            if (!pr.X.inS[e] && pr.X.c + ce <= B) {
                dX = subm::sfunc_marginal(g, pr.X.inS, e, pr.X.f);
                res.queries += 1;
            } else {
                // still count 0 without query (feasibility cuts the oracle call)
                dX = -std::numeric_limits<double>::infinity();
            }

            if (!pr.Y.inS[e] && pr.Y.c + ce <= B) {
                dY = subm::sfunc_marginal(g, pr.Y.inS, e, pr.Y.f);
                res.queries += 1;
            } else {
                dY = -std::numeric_limits<double>::infinity();
            }

            dcs_detail::Cand *A = nullptr;
            double dA = 0.0;

            if (dX >= dY) { A = &pr.X; dA = dX; }
            else          { A = &pr.Y; dA = dY; }

            if (!A) continue;
            if (dA == -std::numeric_limits<double>::infinity()) continue;

            // if c(A)+c(e) <= B and f(e|A) >= rho*c(e) then A <- A U {e}
            if (A->c + ce <= B && dA >= pr.rho * ce) {
                A->inS[e] = 1;
                A->c += ce;
                A->f += dA;
                A->members.push_back(e);

                // if f(A) > f(L*) then L* <- A
                if (A->f > fL) {
                    L = A->inS;   // copy bitset
                    cL = A->c;
                    fL = A->f;
                }
            }
        }
    }

    // ---- Third stream ----
    for (node_id e = 0; e < g.n; ++e) {
        const double ce = dcs_detail::node_cost(g, e);
        if (ce <= 0.0 || ce > B) continue;

        for (auto &pr : pairs) {
            // if e notin X_rho and feasible and f(X_rho U {e}) >= f(L*) then L* <- X_rho U {e}
            if (!pr.X.inS[e] && pr.X.c + ce <= B) {
                const double d = subm::sfunc_marginal(g, pr.X.inS, e, pr.X.f);
                res.queries += 1;
                if (pr.X.f + d >= fL) {
                    Solution tmp = pr.X.inS;
                    tmp[e] = 1;
                    L = std::move(tmp);
                    cL = pr.X.c + ce;
                    fL = pr.X.f + d;
                }
            }
            // similarly for Y_rho
            if (!pr.Y.inS[e] && pr.Y.c + ce <= B) {
                const double d = subm::sfunc_marginal(g, pr.Y.inS, e, pr.Y.f);
                res.queries += 1;
                if (pr.Y.f + d >= fL) {
                    Solution tmp = pr.Y.inS;
                    tmp[e] = 1;
                    L = std::move(tmp);
                    cL = pr.Y.c + ce;
                    fL = pr.Y.f + d;
                }
            }
        }
    }

    // ---- Final augmentation (lines 29..40): add elements from X_rho and Y_rho into L* if feasible ----
    for (auto &pr : pairs) {
        for (node_id e : pr.X.members) {
            if (L[e]) continue;
            const double ce = dcs_detail::node_cost(g, e);
            if (ce <= 0.0) continue;
            if (cL + ce > B) continue;

            const double d = subm::sfunc_marginal(g, L, e, fL);
            res.queries += 1;

            L[e] = 1;
            cL += ce;
            fL += d;
        }
        for (node_id e : pr.Y.members) {
            if (L[e]) continue;
            const double ce = dcs_detail::node_cost(g, e);
            if (ce <= 0.0) continue;
            if (cL + ce > B) continue;

            const double d = subm::sfunc_marginal(g, L, e, fL);
            res.queries += 1;

            L[e] = 1;
            cL += ce;
            fL += d;
        }
    }

    res.inS = std::move(L);
    res.f_value = subm::sfunc_evaluate(g, res.inS);
    res.queries += 1;

    auto t1 = Clock::now();
    res.time_sec = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

} // namespace algs

#endif // ALGS_DCS_STREAMING_H
