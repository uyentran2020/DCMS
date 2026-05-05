// algs/result.h
#ifndef ALGS_RESULT_H
#define ALGS_RESULT_H

#include <string>
#include <vector>
#include <cstddef>

#include "kfunctions.h" // ksub::Assignment

namespace algs {

struct Result {
    std::string algo;        // tên thuật toán
    std::string constraint;  // mô tả ràng buộc (partition matroid, fairness, ...)

    double f_value = 0.0;    // giá trị hàm mục tiêu f(x)

    // ====== Error metrics ======
    double fair_error = 0.0;    // vi phạm fairness: sum_i max{|X_i|-u_i, l_i-|X_i|, 0}

    // vi phạm matroid (partition matroid):
    // gợi ý đo lường chuẩn: sum_j max{|S ∩ P_j| - b_j, 0}
    // với S = supp(x) hoặc S = ⋃_{i=1..K} X_i (tùy định nghĩa nghiệm trong thuật toán)
    double matroid_error = 0.0;

    // tổng vi phạm (có thể thay đổi nếu bạn muốn gộp thêm chỉ số khác)
    double total_error = 0.0; // fair_error + matroid_error

    // ====== Accounting ======
    std::size_t queries = 0;         // số queries oracle hàm f(·) (theo định nghĩa trong từng thuật toán)
    std::size_t matroid_checks = 0;  // số lần gọi oracle kiểm tra độc lập (independence) của matroid

    // ====== Runtime / Memory ======
    double time_sec = 0.0; // thời gian chạy (s)
    double mem_mb = 0.0;   // bộ nhớ dùng thêm (MB) do main set

    // ====== Solution ======
    ksub::Assignment x; // nghiệm (nhãn 0..K)
};

} // namespace algs

#endif // ALGS_RESULT_H
