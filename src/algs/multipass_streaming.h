// src/algs/multipass_streaming.h
#ifndef ALGS_MULTIPASS_STREAMING_H
#define ALGS_MULTIPASS_STREAMING_H

#include <vector>
#include <cstdint>
#include <cmath>
#include <limits>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#include "mygraph.h"
#include "sfunctions.h"

namespace algs {

using mygraph::node_id;
using subm::Solution;

// ---------------- helpers ----------------
namespace mp_detail {

inline double cost(const mygraph::tinyGraph &g, node_id e) {
    return g.nodes[e].weight;
}

inline double eval(const mygraph::tinyGraph &g, const Solution &S, std::uint64_t &queries) {
    queries += 1;
    return subm::sfunc_evaluate(g, S);
}

inline double marg_add(const mygraph::tinyGraph &g,
                       const Solution &S,
                       node_id e,
                       double fS,
                       std::uint64_t &queries)
{
    queries += 1;
    return subm::sfunc_marginal(g, S, e, fS);
}

// Deterministic USM(f,N) as in your image (used by Algorithm 10 elsewhere)
inline void extract_elements(const Solution &S, std::vector<node_id> &elems) {
    elems.clear();
    elems.reserve(S.size() / 8 + 8);
    for (std::size_t u = 0; u < S.size(); ++u) if (S[u]) elems.push_back(static_cast<node_id>(u));
}

inline Solution deterministic_usm(const mygraph::tinyGraph &g,
                                 const Solution &Nset,
                                 std::uint64_t &queries)
{
    const std::size_t n = g.n;
    std::vector<node_id> U;
    extract_elements(Nset, U); // increasing node_id order

    Solution X(n, 0);
    Solution Y = Nset;

    double fX = eval(g, X, queries);
    double fY = eval(g, Y, queries);

    for (node_id u : U) {
        // a_i = f(X+u) - f(X)
        double ai = 0.0;
        if (!X[u]) ai = marg_add(g, X, u, fX, queries);

        // b_i = f(Y-u) - f(Y)
        double bi = 0.0;
        double fY_minus = fY;
        if (Y[u]) {
            Y[u] = 0;
            fY_minus = eval(g, Y, queries);
            bi = fY_minus - fY;
        }

        if (ai >= bi) {
            if (!X[u]) { X[u] = 1; fX += ai; }
            if (!Y[u]) { Y[u] = 1; } // restore (Yi unchanged)
        } else {
            // keep removal
            fY = fY_minus;
        }
    }
    return X;
}

} // namespace mp_detail

// ============================================================
// Algorithm 9: Multi-pass streaming algorithm (d=1 -> knapsack)
// Input: stream e1..en (nodes 0..n-1), capacity B, cost c, f,
//        m = max_{e in stream} f({e}), eps>0
// Output: (A, G) as in the pseudocode.
//
// Exclusion: you may pass an exclude mask (bitset) to represent stream N \ exclude.
// If exclude == nullptr => stream is all nodes.
// ============================================================
struct MP9_Output {
    Solution A;   // returned A
    Solution G;   // constructed G
};

inline MP9_Output run_multipass_streaming_algo9_knapsack(const mygraph::tinyGraph &g,
                                                         double B,
                                                         double m,
                                                         double eps,
                                                         const Solution *exclude_mask,
                                                         std::uint64_t &queries)
{
    if (g.n == 0 || B <= 0.0) {
        MP9_Output out{Solution(g.n, 0), Solution(g.n, 0)};
        return out;
    }
    if (!(eps > 0.0 && eps < 1.0)) {
        throw std::invalid_argument("Algorithm9: eps must be in (0,1).");
    }
    if (!(m >= 0.0)) m = 0.0;

    const double base = 1.0 + eps;

    // ---- Thresholding stage ----
    Solution empty(g.n, 0);
    const double f_empty = mp_detail::eval(g, empty, queries);

    Solution G(g.n, 0);
    double cG = 0.0;
    double fG = f_empty;

    std::vector<node_id> orderG;
    std::vector<double> prefix_cost; // cost of first i items, i=0..|G|
    std::vector<double> prefix_f;    // f(G_i)
    prefix_cost.reserve(1024);
    prefix_f.reserve(1024);
    prefix_cost.push_back(0.0);
    prefix_f.push_back(f_empty);

    double tau = m;
    const double tau_min = (B > 0.0 ? (m / B) : std::numeric_limits<double>::infinity());

    while (tau >= tau_min && tau > 0.0) {
        // new pass over stream
        for (node_id e = 0; e < g.n; ++e) {
            if (exclude_mask && (*exclude_mask)[e]) continue;

            const double ce = mp_detail::cost(g, e);
            if (ce <= 0.0) continue;
            if (cG + ce > B) continue;

            // marginal density f(e|G)/c(e)
            const double delta = mp_detail::marg_add(g, G, e, fG, queries);
            if (delta / ce >= tau) {
                // select e
                if (!G[e]) {
                    G[e] = 1;
                    cG += ce;
                    fG += delta;
                    orderG.push_back(e);
                    prefix_cost.push_back(cG);
                    prefix_f.push_back(fG);
                }
            }
        }
        tau /= base;
    }

    const std::size_t k = orderG.size(); // |G|

    // ---- Build prefix bitsets G_i for augmentation stage ----
    // G_prefix[i] = first i items of G in construction order, i=0..k
    // NOTE: This may be memory-heavy: (k+1)*n bytes.
    std::vector<Solution> G_prefix(k + 1);
    G_prefix[0].assign(g.n, 0);
    for (std::size_t i = 1; i <= k; ++i) {
        G_prefix[i] = G_prefix[i - 1];
        G_prefix[i][orderG[i - 1]] = 1;
    }

    // ---- Augmentation stage ----
    // Maintain best singleton a_{i} for each prefix index (i = 1..k+1), where prefix is G_{i-1}
    const std::size_t I = k + 1;
    std::vector<long long> best_elem(I + 1, -1); // 1..I
    std::vector<double> best_val(I + 1, -std::numeric_limits<double>::infinity());

    for (std::size_t i = 1; i <= I; ++i) {
        // a_i = empty initially => value = f(G_{i-1})
        best_val[i] = prefix_f[i - 1];
        best_elem[i] = -1;
    }

    // one pass over stream
    for (node_id e = 0; e < g.n; ++e) {
        if (exclude_mask && (*exclude_mask)[e]) continue;

        const double ce = mp_detail::cost(g, e);
        if (ce <= 0.0) continue;

        // l = max i such that cost(G_i) + ce <= B
        // prefix_cost is increasing (positive costs assumed)
        auto it = std::upper_bound(prefix_cost.begin(), prefix_cost.end(), B - ce);
        if (it == prefix_cost.begin()) continue;
        std::size_t l = static_cast<std::size_t>(it - prefix_cost.begin()) - 1; // 0..k

        // for i = 1..l+1 => prefix index j = i-1 runs 0..l
        for (std::size_t j = 0; j <= l; ++j) {
            // compare f(G_{j} ∪ a_{j+1}) < f(G_{j} ∪ {e})
            if (G_prefix[j][e]) continue; // already in prefix => no improvement

            const double f_pref = prefix_f[j];
            const double delta = mp_detail::marg_add(g, G_prefix[j], e, f_pref, queries);
            const double val = f_pref + delta;

            const std::size_t idx = j + 1; // a_idx corresponds to prefix j
            if (val > best_val[idx]) {
                best_val[idx] = val;
                best_elem[idx] = static_cast<long long>(e);
            }
        }
    }

    // A = argmax_i f(G_{i-1} ∪ a_i)
    double bestA = -std::numeric_limits<double>::infinity();
    Solution A_best(g.n, 0);

    for (std::size_t i = 1; i <= I; ++i) {
        Solution A = G_prefix[i - 1];
        double cA = prefix_cost[i - 1];

        if (best_elem[i] != -1) {
            node_id e = static_cast<node_id>(best_elem[i]);
            const double ce = mp_detail::cost(g, e);
            if (cA + ce <= B && !A[e]) {
                A[e] = 1;
                cA += ce;
            }
        }

        // value: we already tracked best_val[i] for the chosen singleton, but due to feasibility checks above,
        // recompute safely (1 oracle call) to keep correctness in corner cases.
        const double val = mp_detail::eval(g, A, queries);
        if (val > bestA) {
            bestA = val;
            A_best = std::move(A);
        }
    }

    MP9_Output out;
    out.A = std::move(A_best);
    out.G = std::move(G);
    return out;
}

} // namespace algs

#endif // ALGS_MULTIPASS_STREAMING_H
