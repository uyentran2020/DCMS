// src/kfunctions_impl.h
#ifndef KFUNCTIONS_IMPL_H
#define KFUNCTIONS_IMPL_H

// ======================================================
// kfunctions_impl.h (dispatcher)
//
// File này chỉ chọn đúng 1 objective và include implementation tương ứng
// trong thư mục src/objectvalue/.
// ======================================================

// Bắt buộc chọn đúng 1 macro objective khi build.
#if (defined(KFUNC_MAXKCUT) + defined(KFUNC_REVENUE) + defined(KFUNC_KIC) + defined(KFUNC_SENSOR_ENTROPY_GAUSS)) != 1
#error "You must define exactly one of: KFUNC_MAXKCUT, KFUNC_REVENUE, KFUNC_KIC, KFUNC_SENSOR_ENTROPY_GAUSS"
#endif

#if defined(KFUNC_MAXKCUT)
#include "objectvalue/maxkcut.h"
#elif defined(KFUNC_REVENUE)
#include "objectvalue/revenue.h"
#elif defined(KFUNC_KIC)
#include "objectvalue/kic.h"
#elif defined(KFUNC_SENSOR_ENTROPY_GAUSS)
#include "objectvalue/sensor_entropy_gauss.h"
#endif

#endif // KFUNCTIONS_IMPL_H
