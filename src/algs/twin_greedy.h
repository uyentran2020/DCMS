// src/algs/twin_greedy.h
#ifndef ALGS_TWIN_GREEDY_H
#define ALGS_TWIN_GREEDY_H

#include <vector>
#include <cstdint>
#include <limits>
#include <chrono>
#include <sstream>

#include "mygraph.h"
#include "sfunctions.h"
#include "algs/result.h"

namespace algs {

using Clock = std::chrono::high_resolution_clock;
using mygraph::node_id;
using subm::Solution;

// Twin Greedy for Knapsack (Algorithm 3 in the image)
// Input: N (V), f, c, B
// Maintain two solutions S1, S2; each element can be selected at most once into either S1 or S2.
// Choose (k,u) maximizing f(u | S_k) / c(u) among feasible u for active sets.
// Stop if best marginal <= 0.
// Return argmax{f(S1), f(S2)}.
//
// Assumptions (important):
//  (A1) We enforce feasibility when considering candidates:
//       only consider u if cost(S_k) + c(u) <= B.
//       This keeps S1,S2 always feasible knapsack solutions.
//  (A2) c(u) = g.nodes[u].weight. If c(u) <= 0, skip u.
//
// Note: queries counting: +1 for each sfunc_marginal call, +1 for each sfunc_evaluate call.
inline Result run_twin_greedy_knapsack(const mygraph::tinyGraph &g, double B) {
    Result res;
    res.algo = "TwinGreedy(Knapsack)";

    {
        std::ostringstream oss;
        oss << "B=" << B;
        res.constraint = oss.str();
    }

    const std::size_t n = g.n;
    Solution S1(n, 0), S2(n, 0);

    // N: remaining elements
    std::vector<std::uint8_t> inN(n, 1); // 1 => available

    res.queries = 0;
    auto t0 = Clock::now();

    // start values
    double f1 = subm::sfunc_evaluate(g, S1); res.queries += 1;
    double f2 = subm::sfunc_evaluate(g, S2); res.queries += 1;
    double c1 = 0.0, c2 = 0.0;

    bool J1 = (c1 < B); // active if cost < B
    bool J2 = (c2 < B);

    const double NEG_INF = -std::numeric_limits<double>::infinity();

    while (true) {
        // stop if N empty or both sets inactive
        bool anyN = false;
        for (std::size_t u = 0; u < n; ++u) { if (inN[u]) { anyN = true; break; } }
        if (!anyN) break;
        if (!J1 && !J2) break;

        int best_k = 0;          // 1 or 2
        node_id best_u = 0;
        double best_delta = NEG_INF;
        double best_ratio = NEG_INF;

        // Scan candidates
        for (node_id u = 0; u < n; ++u) {
            if (!inN[u]) continue;

            const double cu = g.nodes[u].weight;
            if (cu <= 0.0) continue; // invalid cost

            if (J1 && c1 + cu <= B) {
                const double d = subm::sfunc_marginal(g, S1, u, f1);
                res.queries += 1;
                const double ratio = d / cu;
                if (ratio > best_ratio) {
                    best_ratio = ratio;
                    best_delta = d;
                    best_k = 1;
                    best_u = u;
                }
            }

            if (J2 && c2 + cu <= B) {
                const double d = subm::sfunc_marginal(g, S2, u, f2);
                res.queries += 1;
                const double ratio = d / cu;
                if (ratio > best_ratio) {
                    best_ratio = ratio;
                    best_delta = d;
                    best_k = 2;
                    best_u = u;
                }
            }
        }

        // if no feasible candidate found
        if (best_k == 0 || best_ratio == NEG_INF) break;

        // if marginal <= 0 then break (as in pseudo-code)
        if (best_delta <= 0.0) break;

        // add best_u to S_best_k
        const double cu = g.nodes[best_u].weight;

        if (best_k == 1) {
            S1[best_u] = 1;
            c1 += cu;
            f1 += best_delta;
            if (c1 >= B) J1 = false; // reached budget (still feasible because we enforced c1+cu<=B)
        } else {
            S2[best_u] = 1;
            c2 += cu;
            f2 += best_delta;
            if (c2 >= B) J2 = false;
        }

        // remove u from N
        inN[best_u] = 0;
    }

    // Return argmax{f(S1), f(S2)} (re-evaluate for robustness)
    const double val1 = subm::sfunc_evaluate(g, S1); res.queries += 1;
    const double val2 = subm::sfunc_evaluate(g, S2); res.queries += 1;

    if (val2 > val1) {
        res.inS = std::move(S2);
        res.f_value = val2;
    } else {
        res.inS = std::move(S1);
        res.f_value = val1;
    }

    auto t1 = Clock::now();
    res.time_sec = std::chrono::duration<double>(t1 - t0).count();
    return res;
}

} // namespace algs

#endif // ALGS_TWIN_GREEDY_H
