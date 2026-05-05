// src/algs/repeat_greedy_streaming.h
#ifndef ALGS_REPEAT_GREEDY_STREAMING_H
#define ALGS_REPEAT_GREEDY_STREAMING_H

#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <limits>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#include "mygraph.h"
#include "sfunctions.h"
#include "algs/result.h"

namespace algs {

using Clock = std::chrono::high_resolution_clock;
using mygraph::node_id;
using subm::Solution;

// ===================== internal helpers =====================
namespace rg_detail {

inline double cost(const mygraph::tinyGraph &g, node_id e) {
    return g.nodes[e].weight;
}

inline double evaluate(const mygraph::tinyGraph &g, const Solution &S, std::uint64_t &queries) {
    queries += 1;
    return subm::sfunc_evaluate(g, S);
}

// f(S ∪ {e}) - f(S) using cached fS
inline double marginal_add(const mygraph::tinyGraph &g,
                           const Solution &S,
                           node_id e,
                           double fS,
                           std::uint64_t &queries)
{
    queries += 1;
    return subm::sfunc_marginal(g, S, e, fS);
}

inline void extract_elements(const Solution &S, std::vector<node_id> &elems) {
    elems.clear();
    elems.reserve(S.size() / 8 + 8);
    for (std::size_t u = 0; u < S.size(); ++u) if (S[u]) elems.push_back(static_cast<node_id>(u));
}

// -------------------------------------------------------------
// Deterministic USM(f, N) on a given ground set N (as bitset Nset)
// as in your image:
//
// X0 = ∅, Y0 = N
// for i=1..n:
//   ai = f(X_{i-1}+u_i) - f(X_{i-1})
//   bi = f(Y_{i-1}-u_i) - f(Y_{i-1})
//   if ai >= bi: Xi = Xi-1 + u_i, Yi = Yi-1
//   else:        Xi = Xi-1,      Yi = Yi-1 - u_i
// return Xn
//
// Here u_i are elements of N in increasing node_id order.
// -------------------------------------------------------------
inline Solution unconstrained_deterministic_usm(const mygraph::tinyGraph &g,
                                               const Solution &Nset,
                                               std::uint64_t &queries)
{
    const std::size_t n = g.n;
    std::vector<node_id> U;
    extract_elements(Nset, U);

    Solution X(n, 0);
    Solution Y = Nset;

    double fX = evaluate(g, X, queries);
    double fY = evaluate(g, Y, queries);

    for (node_id u : U) {
        // a_i = f(X + u) - f(X)
        double ai = 0.0;
        if (!X[u]) {
            ai = marginal_add(g, X, u, fX, queries);
        }

        // b_i = f(Y - u) - f(Y)
        double bi = 0.0;
        double fY_minus = fY;
        if (Y[u]) {
            Y[u] = 0;
            fY_minus = evaluate(g, Y, queries);
            bi = fY_minus - fY; // <= 0 typically
        } else {
            // u ∈ N => normally Y[u]=1 at start and only possibly removed earlier
            bi = 0.0;
        }

        if (ai >= bi) {
            // Xi <- Xi-1 + u, Yi <- Yi-1  (restore Y[u] if we toggled)
            if (!X[u]) {
                X[u] = 1;
                fX += ai;
            }
            if (!Y[u]) {
                // restore since Yi unchanged
                Y[u] = 1;
            }
            // fY unchanged
        } else {
            // Xi unchanged, Yi <- Yi-1 - u (keep removal)
            // Y[u] already 0
            fY = fY_minus;
        }
    }

    return X;
}

} // namespace rg_detail

// =====================================================================
// Algorithm 6: 1-Pass streaming repeat greedy for d-knapsack (set d=1)
// =====================================================================
//
// Input: stream e1..en (here nodes 0..n-1), knapsack capacity B,
//        cost c(e)=node.weight, nonnegative submodular f, epsilon in (0,1).
//
// Maintain candidates for v in Q = {(1+eps)^k | m/(1+eps) <= v <= 4Bm}
// where m = max_seen f({e})/c(e).
//
// For each v: S1^v, S2^v, S'^v (singleton), tau = v/(4(d+1)) = v/8.
// Rules (d=1):
//   if c(e) >= B/2 and f({e})/c(e) >= 2*tau/B:      S'^v = {e_best}
//   else if f(e|S1^v)/c(e) >= 2*tau/B and feasible: S1^v <- S1^v ∪ {e}
//   else if f(e|S2^v)/c(e) >= 2*tau/B and feasible: S2^v <- S2^v ∪ {e}
//
// After stream: S3^v = Unconstrained(S1^v) using Deterministic USM on ground S1^v.
// Return best among {S1^v, S2^v, S3^v, S'^v} over v∈Q.
// ---------------------------------------------------------------------
inline Result run_repeat_greedy_streaming_knapsack(const mygraph::tinyGraph &g,
                                                  double B,
                                                  double eps)
{
    Result res;
    res.algo = "RepeatGreedyStreaming";
    {
        std::ostringstream oss;
        oss << "B=" << B << ", eps=" << eps;
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
    if (!(eps > 0.0 && eps < 1.0)) {
        throw std::invalid_argument("RepeatGreedyStreaming: eps must be in (0,1).");
    }

    const double base = 1.0 + eps;
    const double log_base = std::log(base);

    struct Cand {
        double v = 0.0;
        Solution S1, S2, Sprime;
        double c1 = 0.0, c2 = 0.0, cprime = 0.0;
        double f1 = 0.0, f2 = 0.0, fprime = -std::numeric_limits<double>::infinity();

        void init(std::size_t n, double v_, double f_empty) {
            v = v_;
            S1.assign(n, 0);
            S2.assign(n, 0);
            Sprime.assign(n, 0);
            c1 = c2 = cprime = 0.0;
            f1 = f_empty;
            f2 = f_empty;
            fprime = -std::numeric_limits<double>::infinity();
        }
    };

    std::unordered_map<long long, Cand> mp; // k -> candidate for v=(1+eps)^k
    mp.reserve(1024);

    // f(∅)
    Solution empty(g.n, 0);
    const double f_empty = rg_detail::evaluate(g, empty, res.queries);

    // m = max_{seen e} f({e})/c(e)
    double m = 0.0;

    auto ensure_Q = [&](double lower, double upper) {
        if (upper < lower || upper <= 0.0) return;

        const long long k_min = static_cast<long long>(std::ceil(std::log(lower) / log_base));
        const long long k_max = static_cast<long long>(std::floor(std::log(upper) / log_base));

        for (long long k = k_min; k <= k_max; ++k) {
            if (mp.find(k) != mp.end()) continue;
            const double v = std::pow(base, static_cast<double>(k));
            Cand c;
            c.init(g.n, v, f_empty);
            mp.emplace(k, std::move(c));
        }

        // prune outside current range (keeps memory bounded)
        for (auto it = mp.begin(); it != mp.end(); ) {
            const double v = it->second.v;
            if (v + 1e-15 < lower || v > upper * (1.0 + 1e-15)) it = mp.erase(it);
            else ++it;
        }
    };

    // Stream nodes 0..n-1
    for (node_id e = 0; e < g.n; ++e) {
        const double ce = rg_detail::cost(g, e);
        if (ce <= 0.0) continue;

        // f({e}) = f(∅) + marginal(∅, e)
        const double f_single = f_empty + rg_detail::marginal_add(g, empty, e, f_empty, res.queries);
        const double dens_single = f_single / ce;
        if (dens_single > m) m = dens_single;

        if (m <= 0.0) continue;

        // Q bounds (d=1):  lower = m/(1+eps), upper = 2(d+1) B m = 4 B m
        const double lower = m / base;
        const double upper = 4.0 * B * m;
        ensure_Q(lower, upper);

        if (mp.empty()) continue;

        for (auto &kv : mp) {
            Cand &C = kv.second;

            // tau = v/8, rhs = 2*tau/B
            const double tau = C.v / 8.0;
            const double rhs = (2.0 * tau) / B;

            // Singleton candidate S'^v
            if (ce >= B / 2.0 && dens_single >= rhs) {
                if (f_single > C.fprime) {
                    std::fill(C.Sprime.begin(), C.Sprime.end(), 0);
                    C.Sprime[e] = 1;
                    C.cprime = ce;
                    C.fprime = f_single;
                }
                continue;
            }

            // Try add to S1^v
            if (!C.S1[e] && C.c1 + ce <= B) {
                const double delta = rg_detail::marginal_add(g, C.S1, e, C.f1, res.queries);
                if (delta / ce >= rhs) {
                    C.S1[e] = 1;
                    C.c1 += ce;
                    C.f1 += delta;
                    continue;
                }
            }

            // Try add to S2^v
            if (!C.S2[e] && C.c2 + ce <= B) {
                const double delta = rg_detail::marginal_add(g, C.S2, e, C.f2, res.queries);
                if (delta / ce >= rhs) {
                    C.S2[e] = 1;
                    C.c2 += ce;
                    C.f2 += delta;
                    continue;
                }
            }
        }
    }

    // Final selection
    double best_val = -std::numeric_limits<double>::infinity();
    Solution bestS(g.n, 0);

    auto try_update = [&](const Solution &S) {
        const double val = rg_detail::evaluate(g, S, res.queries);
        if (val > best_val) {
            best_val = val;
            bestS = S;
        }
    };

    if (mp.empty()) {
        try_update(empty);
    } else {
        for (auto &kv : mp) {
            Cand &C = kv.second;

            // S1^v, S2^v
            try_update(C.S1);
            try_update(C.S2);

            // S3^v = Unconstrained(S1^v) via Deterministic USM
            {
                Solution S3 = rg_detail::unconstrained_deterministic_usm(g, C.S1, res.queries);
                try_update(S3);
            }

            // S'^v
            if (C.fprime > -std::numeric_limits<double>::infinity()) {
                try_update(C.Sprime);
            }
        }
    }

    res.inS = std::move(bestS);
    res.f_value = best_val;

    auto t1 = Clock::now();
    res.time_sec = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

} // namespace algs

#endif // ALGS_REPEAT_GREEDY_STREAMING_H
