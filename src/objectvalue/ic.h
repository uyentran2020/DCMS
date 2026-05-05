// src/objectvalue/ic.h  (submodular IC objective with OpenMP, truly-random seed if KIC_SEED not set)
#ifndef OBJECTVALUE_IC_H
#define OBJECTVALUE_IC_H

#include "objectvalue_common.h"

#include <vector>
#include <utility>
#include <random>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <chrono>

#ifndef _OPENMP
#error "IC requires OpenMP. Please compile with -fopenmp (and link with OpenMP)."
#endif
#include <omp.h>

namespace subm_obj_ic {

using mygraph::node_id;
using mygraph::edge_id;

namespace detail_ic {

inline double clamp01(double p) {
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 1.0;
    return p;
}

inline bool env_exists(const char* name) {
    return (std::getenv(name) != nullptr);
}

inline std::size_t read_env_size_t(const char* name, std::size_t defv) {
    if (const char* s = std::getenv(name)) {
        char* end = nullptr;
        unsigned long long v = std::strtoull(s, &end, 10);
        if (end && *end == '\0') return static_cast<std::size_t>(v);
    }
    return defv;
}

inline std::uint64_t read_env_u64(const char* name, std::uint64_t defv) {
    if (const char* s = std::getenv(name)) {
        char* end = nullptr;
        unsigned long long v = std::strtoull(s, &end, 10);
        if (end && *end == '\0') return static_cast<std::uint64_t>(v);
    }
    return defv;
}

inline double read_env_double(const char* name, double defv) {
    if (const char* s = std::getenv(name)) {
        char* end = nullptr;
        double v = std::strtod(s, &end);
        if (end && *end == '\0') return v;
    }
    return defv;
}

// splitmix64 để seed theo iteration (ổn định theo it)
inline std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// “ngẫu nhiên thực sự” theo lần chạy: entropy OS (random_device) + trộn thời gian.
inline std::uint64_t runtime_seed64() {
    std::random_device rd;
    std::uint64_t r = (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());

    const std::uint64_t t =
        static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());

    std::uint64_t addr_mix = reinterpret_cast<std::uintptr_t>(&rd);
    return splitmix64(r ^ t ^ addr_mix);
}

// Cache adjacency (single-topic scalar IC):
// adjs[u] = [(v, p(u,v)), ...]
struct ICCache {
    const mygraph::tinyGraph* gptr = nullptr;
    std::size_t n = 0;
    std::size_t m = 0;
    bool undirected = true;

    std::vector<std::vector<std::pair<node_id,double>>> adjs;

    void rebuild(const mygraph::tinyGraph& g) {
        gptr = &g;
        n = g.n;
        m = g.m;
        undirected = g.undirected;

        adjs.assign(n, {});

        // dùng edges.size() để an toàn ngay cả khi g.m lệch (nếu có)
        for (edge_id eid = 0; eid < g.edges.size(); ++eid) {
            const auto& E = g.edges[eid];
            node_id u = E.u;
            node_id v = E.v;
            if (u >= n || v >= n) continue;

            double p = clamp01(E.weight);
            if (p <= 0.0) continue;

            // directed: u -> v
            adjs[u].push_back({v, p});

            // undirected: thêm v -> u
            if (undirected) adjs[v].push_back({u, p});
        }
    }
};

inline const std::vector<std::vector<std::pair<node_id,double>>>&
get_adjs(const mygraph::tinyGraph& g) {
    static ICCache cache;
    if (cache.gptr != &g ||
        cache.n != g.n || cache.m != g.m ||
        cache.undirected != g.undirected)
    {
        cache.rebuild(g);
    }
    return cache.adjs;
}

} // namespace detail_ic

// ======================================================
// Submodular IC (OpenMP Monte-Carlo)
//   f(S) = E[ |A(S)| ] - lambda * |S|
//
// env:
//   KIC_MC      (default 100)
//   KIC_SEED    (if set -> reproducible; if NOT set -> truly-random per run)
//   KIC_LAMBDA  (default 1.0)
// ======================================================

inline double evaluate(const mygraph::tinyGraph &g,
                       const subm::Solution &inS)
{
    const std::size_t n = g.n;
    if (n == 0) return 0.0;

    // seeds + |S|
    std::vector<node_id> seeds;
    seeds.reserve(n);
    std::size_t S_card = 0;
    for (node_id u = 0; u < static_cast<node_id>(n); ++u) {
        if (inS[u]) {
            ++S_card;
            seeds.push_back(u);
        }
    }

    const std::size_t mc = detail_ic::read_env_size_t("KIC_MC", 100);
    const double lambda  = detail_ic::read_env_double("KIC_LAMBDA", 1.0);

    if (mc == 0) {
        return -lambda * static_cast<double>(S_card);
    }

    // Seed:
    // - Nếu user set KIC_SEED -> reproducible.
    // - Nếu không set -> random theo lần chạy.
    const std::uint64_t base_seed =
        detail_ic::env_exists("KIC_SEED")
            ? detail_ic::read_env_u64("KIC_SEED", 42ULL)
            : detail_ic::runtime_seed64();

    const auto& adjs = detail_ic::get_adjs(g);

    double sum_spread = 0.0;

#pragma omp parallel
    {
        // Mỗi thread có visited/stamp riêng
        std::vector<std::uint32_t> seen(n, 0);
        std::uint32_t stamp = 1;

        auto bump_stamp = [](std::uint32_t& st, std::vector<std::uint32_t>& arr) {
            ++st;
            if (st == 0) { // overflow wrap
                std::fill(arr.begin(), arr.end(), 0);
                st = 1;
            }
        };

        std::uniform_real_distribution<double> uni(0.0, 1.0);

#pragma omp for schedule(static) reduction(+:sum_spread)
        for (std::size_t it = 0; it < mc; ++it) {
            // RNG theo it: ổn định theo iteration và không phụ thuộc schedule/num_threads
            std::mt19937_64 rng(detail_ic::splitmix64(base_seed ^ static_cast<std::uint64_t>(it)));

            bump_stamp(stamp, seen);
            std::size_t activated_cnt = 0;

            std::vector<node_id> frontier;
            frontier.reserve(seeds.size());

            // init from seeds
            for (node_id s : seeds) {
                if (s >= n) continue;
                if (seen[s] == stamp) continue;
                seen[s] = stamp;
                frontier.push_back(s);
                ++activated_cnt;
            }

            // IC BFS-like
            while (!frontier.empty()) {
                std::vector<node_id> nxt;
                nxt.reserve(frontier.size());

                for (node_id uu : frontier) {
                    const auto& out = adjs[uu];
                    for (const auto& vp : out) {
                        node_id v = vp.first;
                        const double p = vp.second;
                        if (v >= n) continue;
                        if (seen[v] == stamp) continue;

                        if (uni(rng) < p) {
                            seen[v] = stamp;
                            nxt.push_back(v);
                            ++activated_cnt;
                        }
                    }
                }
                frontier.swap(nxt);
            }

            sum_spread += static_cast<double>(activated_cnt);
        }
    } // omp parallel

    const double expected_spread = sum_spread / static_cast<double>(mc);
    return expected_spread - lambda * static_cast<double>(S_card);
}

// ======================================================
// marginal = f(S ∪ {x}) - f(S)   (fS được truyền vào)
// ======================================================
inline double marginal(const mygraph::tinyGraph &g,
                       const subm::Solution &inS,
                       mygraph::node_id x,
                       double fS)
{
    if (x >= g.n) return 0.0;
    if (inS[x]) return 0.0;

    subm::Solution inS_after = inS;
    inS_after[x] = 1;

    const double after = evaluate(g, inS_after);
    return after - fS;
}

} // namespace subm_obj_ic

#endif // OBJECTVALUE_IC_H
