// src/kfunctions.h
#ifndef KFUNCTIONS_H
#define KFUNCTIONS_H

#include <vector>
#include "mygraph.h"

namespace ksub {

// Nhãn: 0 = chưa gán, 1..K = các nhãn hợp lệ
using Label = unsigned char;

// Assignment: x[u] là nhãn của đỉnh u
using Assignment = std::vector<Label>;

// Giá trị hàm k-submodular tại cấu hình x
double kfunc_evaluate(const mygraph::tinyGraph &g,
                      const Assignment &x);

// Biên lợi ích khi gán nhãn new_label cho đỉnh u
double kfunc_marginal(const mygraph::tinyGraph &g,
                      mygraph::node_id u,
                      Label new_label,
                      const Assignment &x,
                      double f_x);

} // namespace ksub

#endif // KFUNCTIONS_H
