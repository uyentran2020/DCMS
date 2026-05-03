// src/sfunctions.h  (SUBMODULAR-ONLY ORACLE INTERFACE)
#ifndef SFUNCTIONS_H
#define SFUNCTIONS_H

#include <vector>
#include <cstdint>
#include <stdexcept>

#include "mygraph.h"

namespace subm {

using mygraph::node_id;

// Solution representation: inS[u] ∈ {0,1}
using Solution = std::vector<std::uint8_t>;

// Must be provided by sfunctions_impl.h (selected by -DSFUNC_*)
double sfunc_evaluate(const mygraph::tinyGraph &g, const Solution &inS);

// Marginal gain of adding u (assume inS[u]==0)
double sfunc_marginal(const mygraph::tinyGraph &g,
                      const Solution &inS,
                      node_id u,
                      double fS);

// helpers
inline std::size_t solution_size(const Solution &inS) {
    std::size_t c = 0;
    for (auto b : inS) c += (b != 0);
    return c;
}

inline void solution_check_size(const mygraph::tinyGraph &g, const Solution &inS) {
    if (inS.size() != g.n) {
        throw std::invalid_argument("Solution size must equal g.n");
    }
}

} // namespace subm

#endif // SFUNCTIONS_H
