#pragma once
#include <sys/resource.h>

// Пиковый RSS процесса. На Linux ru_maxrss возвращается в килобайтах → переводим в MB.
inline double peakRSS_MB() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss / 1024.0;
}