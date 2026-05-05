// src/algs/multipass_repeat_greedy_max.h
#ifndef ALGS_MULTIPASS_REPEAT_GREEDY_MAX_H
#define ALGS_MULTIPASS_REPEAT_GREEDY_MAX_H

#include <cstdint>
#include <limits>
#include <chrono>
#include <sstream>

#include "mygraph.h"
#include "sfunctions.h"
#include "algs/result.h"
#include "algs/multipass_streaming.h"

namespace algs {

using Clock = std::chrono::high_resolution_clock;
using mygraph::node_id;
using subm::Solution;

// ============================================================
// Algorithm 10: Multi-pass streaming repeat Greedy+Max algorithm
//
// Input: stream N (nodes 0..n-1), capacity B, cost c, f, eps>0
// Steps:
//  1) pass over N: m1 = max f({e})
//  2) (S1, G) = Algorithm9(streaming(N), B, c, f, m1, eps)
//  3) pass over N \ G: m2 = max f({e})
//  4) (S2, T) = Algorithm9(streaming(N\G), B, c, f, m2, eps)
//  5) S3 = Unconstrained(G,f) using Deterministic USM (as you provided)
//  6) return argmax{ f(S1), f(S2), f(S3) }.
//
// Note: Here d=1 (ordinary knapsack).
// ============================================================
inline Result run_multipass_repeat_greedy_max_algo10_knapsack(const mygraph::tinyGraph &g,
                                                              double B,
                                                              double eps)
{
    Result res;
    res.algo = "MpStreamGreedy";
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
        throw std::invalid_argument("Algorithm10: eps must be in (0,1).");
    }

    // f(empty) once
    Solution empty(g.n, 0);
    const double f_empty = subm::sfunc_evaluate(g, empty);
    res.queries += 1;

    // Pass 1: m1 = max f({e})
    double m1 = 0.0;
    for (node_id e = 0; e < g.n; ++e) {
        // f({e}) = f(∅) + marginal(∅, e)
        const double delta = subm::sfunc_marginal(g, empty, e, f_empty);
        res.queries += 1;
        const double f_single = f_empty + delta;
        if (f_single > m1) m1 = f_single;
    }

    // (S1, G) = Algorithm 9 on N
    MP9_Output out1 = run_multipass_streaming_algo9_knapsack(g, B, m1, eps, nullptr, res.queries);
    const Solution &S1 = out1.A;
    const Solution &G  = out1.G;

    // Pass 2: m2 = max f({e}) over N \ G
    double m2 = 0.0;
    for (node_id e = 0; e < g.n; ++e) {
        if (G[e]) continue; // stream N \ G
        const double delta = subm::sfunc_marginal(g, empty, e, f_empty);
        res.queries += 1;
        const double f_single = f_empty + delta;
        if (f_single > m2) m2 = f_single;
    }

    // (S2, T) = Algorithm 9 on N \ G
    MP9_Output out2 = run_multipass_streaming_algo9_knapsack(g, B, m2, eps, &G, res.queries);
    const Solution &S2 = out2.A;
    // const Solution &T = out2.G; // if you need T later

    // S3 = Unconstrained(G, f) using Deterministic USM
    Solution S3 = mp_detail::deterministic_usm(g, G, res.queries);

    // Choose best among S1,S2,S3
    const double fS1 = subm::sfunc_evaluate(g, S1); res.queries += 1;
    const double fS2 = subm::sfunc_evaluate(g, S2); res.queries += 1;
    const double fS3 = subm::sfunc_evaluate(g, S3); res.queries += 1;

    if (fS1 >= fS2 && fS1 >= fS3) {
        res.inS = S1;
        res.f_value = fS1;
    } else if (fS2 >= fS1 && fS2 >= fS3) {
        res.inS = S2;
        res.f_value = fS2;
    } else {
        res.inS = std::move(S3);
        res.f_value = fS3;
    }

    res.time_sec = std::chrono::duration<double>(Clock::now() - t0).count();
    return res;
}

} // namespace algs

#endif // ALGS_MULTIPASS_REPEAT_GREEDY_MAX_H
