#include "Utility.h"

#include <gtest/gtest.h>

bool sameSign(int x, int y) {
    if (x == 0)
        return y == 0;
    if (x < 0)
        return y < 0;
    return y > 0;
}

void failSameSign(int x, int y) {
    ADD_FAILURE() << "Expected same sign: " << x << " and " << y;
}

void expectSameSign(int x, int y) {
    if (x == 0) {
        if (y != 0)
            failSameSign(x, y);
        else return;
    }
    if (x < 0) {
        if (y >= 0)
            failSameSign(x, y);
        else return;
    }
    if (y <= 0)
        failSameSign(x, y);
}
