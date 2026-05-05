// src/algs/edl.h
#ifndef ALGS_EDL_H
#define ALGS_EDL_H

#include <vector>
#include <cstdint>
#include <limits>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>

#include "mygraph.h"
#include "sfunctions.h"
#include "algs/result.h"

namespace algs {

using Clock = std::chrono::high_resolution_clock;
using mygraph::node_id;
using subm::Solution;

// -------------------------
// Helpers: cost, singleton
// -------------------------
inline double cost_of(const mygraph::tinyGraph &g, const Solution &inS) {
    double c = 0.0;
    for (std::size_t u = 0; u < g.n; ++u) {
        if (inS[u]) c += g.nodes[u].weight;
    }
    return c;
}

inline double node_cost(const mygraph::tinyGraph &g, node_id u) {
    return g.nodes[u].weight;
}

inline double eval_singleton(const mygraph::tinyGraph &g,
                             Solution &tmp,
                             node_id e,
                             std::uint64_t &queries)
{
    tmp[e] = 1;
    const double val = subm::sfunc_evaluate(g, tmp);
    queries += 1;
    tmp[e] = 0;
    return val;
}

// =====================================================
// LA Algorithm (Algorithm 1 in the first image)
// Input: (f, V, B)  with knapsack budget B
// Output: a feasible solution S (bitset inS)
// Notes:
//  - c(e) = node.weight
//  - Uses 2 sequences X, Y; then chooses best feasible suffix of each.
// =====================================================
inline Solution run_LA_preprocess(const mygraph::tinyGraph &g,
                                  double B,
                                  std::uint64_t &queries,
                                  double &out_fS)
{
    if (B < 0.0) throw std::invalid_argument("LA: B must be non-negative.");

    const std::size_t n = g.n;
    Solution X(n, 0), Y(n, 0);
    std::vector<node_id> Xseq, Yseq;
    Xseq.reserve(n);
    Yseq.reserve(n);

    // f(empty) (count as 1 query for correctness)
    double fX = subm::sfunc_evaluate(g, X); queries += 1;
    double fY = subm::sfunc_evaluate(g, Y); queries += 1;

    double cX = 0.0, cY = 0.0;

    // e_max = argmax_{e in V, c(e)<=B} f({e})
    Solution tmp(n, 0);
    double best_single_val = -std::numeric_limits<double>::infinity();
    node_id best_single = 0;
    bool has_single = false;

    for (node_id e = 0; e < n; ++e) {
        const double ce = node_cost(g, e);
        if (ce <= 0.0 || ce > B) continue;
        const double val = eval_singleton(g, tmp, e, queries);
        if (!has_single || val > best_single_val) {
            has_single = true;
            best_single_val = val;
            best_single = e;
        }
    }

    Solution S_single(n, 0);
    if (has_single) S_single[best_single] = 1;

    // V1 = { e : c(e) <= B/2 }
    const double halfB = B / 2.0;

    for (node_id e = 0; e < n; ++e) {
        const double ce = node_cost(g, e);
        if (ce <= 0.0 || ce > halfB) continue;

        const double densX = (cX > 0.0) ? (fX / cX) : 0.0;
        const double densY = (cY > 0.0) ? (fY / cY) : 0.0;

        // marginal densities
        const double dX = subm::sfunc_marginal(g, X, e, fX); queries += 1;
        const double dY = subm::sfunc_marginal(g, Y, e, fY); queries += 1;

        const double mdX = dX / ce;
        const double mdY = dY / ce;

        // Choose Z in {X,Y} with mdZ >= densZ and maximal mdZ
        bool canX = (mdX >= densX);
        bool canY = (mdY >= densY);

        if (!canX && !canY) continue;

        if (canX && (!canY || mdX >= mdY)) {
            // add to X
            X[e] = 1;
            Xseq.push_back(e);
            cX += ce;
            fX += dX;
        } else {
            // add to Y
            Y[e] = 1;
            Yseq.push_back(e);
            cY += ce;
            fY += dY;
        }
    }

    // Best feasible suffix of a sequence Tseq:
    auto best_suffix = [&](const std::vector<node_id> &Tseq,
                           double &best_val,
                           std::size_t &best_j) {
        best_val = -std::numeric_limits<double>::infinity();
        best_j = 0;

        Solution cur(n, 0);
        double cur_cost = 0.0;

        // j = 0 (empty suffix)
        best_val = subm::sfunc_evaluate(g, cur); queries += 1;
        best_j = 0;

        // add from the end: last 1, last 2, ...
        for (std::size_t t = 0; t < Tseq.size(); ++t) {
            node_id u = Tseq[Tseq.size() - 1 - t];
            const double cu = node_cost(g, u);
            if (cu <= 0.0) continue;
            cur_cost += cu;
            if (cur_cost > B) break; // costs are positive; further will exceed too
            cur[u] = 1;

            const double val = subm::sfunc_evaluate(g, cur);
            queries += 1;

            const std::size_t j = t + 1;
            if (val > best_val) {
                best_val = val;
                best_j = j;
            }
        }
    };

    double bestX_val, bestY_val;
    std::size_t bestX_j, bestY_j;

    best_suffix(Xseq, bestX_val, bestX_j);
    best_suffix(Yseq, bestY_val, bestY_j);

    // rebuild X' and Y' from best suffix sizes
    auto build_suffix_solution = [&](const std::vector<node_id> &seq, std::size_t j) {
        Solution sol(n, 0);
        for (std::size_t t = 0; t < j; ++t) {
            node_id u = seq[seq.size() - j + t];
            sol[u] = 1;
        }
        return sol;
    };

    Solution Xp = build_suffix_solution(Xseq, bestX_j);
    Solution Yp = build_suffix_solution(Yseq, bestY_j);

    // Choose S = argmax among {X', Y', {e_max}}
    // (use already computed bestX_val, bestY_val, best_single_val; ensure feasibility)
    Solution S = Xp;
    double fS = bestX_val;

    if (bestY_val > fS) { S = Yp; fS = bestY_val; }
    if (has_single && best_single_val > fS) { S = S_single; fS = best_single_val; }

    out_fS = fS;
    return S;
}

// =====================================================
// EDL Algorithm (Algorithm 1 in the second image)
// Input: (f, V, B), eps
// Step 1: S' = LA(f,V,B), M = f(S'), eps' = eps/14
// Step 2: for i=0..Imax: theta_i = 19 M (1-eps')^i / (5 eps' B)
//         scan e in V\(X∪Y) and add e to argmax_{T∈{X,Y}, f(e|T)/c(e) >= theta} density
// Return: argmax_{T∈{X,Y}} f(T)
// =====================================================
inline Result run_EDL(const mygraph::tinyGraph &g, double B, double eps) {
    Result res;
    res.algo = "EDL";

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
        throw std::invalid_argument("EDL: eps must be in (0,1).");
    }

    // Step 1: LA preprocessing
    double M = 0.0;
    Solution S_prime = run_LA_preprocess(g, B, res.queries, M);

    // If M <= 0, EDL thresholds become non-positive; still handle safely.
    const double eps_p = eps / 14.0;
    if (!(eps_p > 0.0 && eps_p < 1.0)) {
        throw std::invalid_argument("EDL: eps/14 must be in (0,1).");
    }

    // Step 2: build candidates X, Y
    Solution X(g.n, 0), Y(g.n, 0);

    double fX = subm::sfunc_evaluate(g, X); res.queries += 1;
    double fY = subm::sfunc_evaluate(g, Y); res.queries += 1;

    double cX = 0.0, cY = 0.0;

    // Imax = floor( log_{1/(1-eps')} (19/eps'^2) ) + 1
    const double base = 1.0 / (1.0 - eps_p);
    const double target = 19.0 / (eps_p * eps_p);

    int Imax = 0;
    if (target <= 1.0) {
        Imax = 1;
    } else {
        Imax = static_cast<int>(std::floor(std::log(target) / std::log(base))) + 1;
        if (Imax < 0) Imax = 0;
    }

    for (int i = 0; i <= Imax; ++i) {
        const double theta = (B > 0.0)
            ? (19.0 * M * std::pow(1.0 - eps_p, static_cast<double>(i)) / (5.0 * eps_p * B))
            : std::numeric_limits<double>::infinity();

        for (node_id e = 0; e < g.n; ++e) {
            if (X[e] || Y[e]) continue;

            const double ce = node_cost(g, e);
            if (ce <= 0.0 || ce > B) continue;

            // choose between X and Y by best density that meets threshold
            bool chooseX = false, chooseY = false;
            double best_md = -std::numeric_limits<double>::infinity();
            double best_delta = 0.0;

            if (cX + ce <= B) {
                const double d = subm::sfunc_marginal(g, X, e, fX);
                res.queries += 1;
                const double md = d / ce;
                if (md >= theta && md > best_md) {
                    best_md = md;
                    best_delta = d;
                    chooseX = true;
                    chooseY = false;
                }
            }

            if (cY + ce <= B) {
                const double d = subm::sfunc_marginal(g, Y, e, fY);
                res.queries += 1;
                const double md = d / ce;
                if (md >= theta && md > best_md) {
                    best_md = md;
                    best_delta = d;
                    chooseX = false;
                    chooseY = true;
                }
            }

            if (!(chooseX || chooseY)) continue;

            if (chooseX) {
                X[e] = 1;
                cX += ce;
                fX += best_delta;
            } else {
                Y[e] = 1;
                cY += ce;
                fY += best_delta;
            }
        }
    }

    // Return best of X, Y (re-evaluate for robustness)
    const double valX = subm::sfunc_evaluate(g, X); res.queries += 1;
    const double valY = subm::sfunc_evaluate(g, Y); res.queries += 1;

    if (valY > valX) {
        res.inS = std::move(Y);
        res.f_value = valY;
    } else {
        res.inS = std::move(X);
        res.f_value = valX;
    }

    auto t1 = Clock::now();
    res.time_sec = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

} // namespace algs

#endif // ALGS_EDL_H
