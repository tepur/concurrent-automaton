#ifndef STATS_H
#define STATS_H

#include <stdio.h>

typedef struct {
    int snt, rcd, acc;
} Stats;

extern void init_stats(Stats* stats);

#endif //STATS_H
