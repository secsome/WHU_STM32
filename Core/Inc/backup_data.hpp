#ifndef __BACKUP_DATA_HPP
#define __BACKUP_DATA_HPP

#ifndef __cplusplus
#error "This header is only for C++"
#endif

#include "critical_data.hpp"

#define BACKUP(T, x) \
__attribute__((section(".critical"))) critical_data<T> x; \
__attribute__((section(".backup1"))) critical_data<T> x##_backup1; \
__attribute__((section(".backup2"))) critical_data<T> x##_backup2; \
__attribute__((section(".backup3"))) critical_data<T> x##_backup3

#define BACKUP_GET(x, y) \
do { \
if (x.is_valid()) x = x; \
else if (x##_backup1.is_valid()) x = x##_backup1; \
else if (x##_backup2.is_valid()) x = x##_backup2; \
else if (x##_backup3.is_valid()) x = x##_backup3; \
x##_backup1 = x; \
x##_backup2 = x; \
x##_backup3 = x; \
} while (0); \
y = x.get()

#define BACKUP_SET(x, y) \
do { \
x.set(y); \
x##_backup1 = x; \
x##_backup2 = x; \
x##_backup3 = x; \
} while (0)

#define BACKUP_IS_VALID(x) \
(x.is_valid() || x##_backup1.is_valid() || x##_backup2.is_valid() || x##_backup3.is_valid())

#endif