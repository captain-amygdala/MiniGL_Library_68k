#include <stdio.h>
#include <math.h>

static inline double my_tan(double x) {
    return sin(x) / cos(x);
}

int main(void) {
    double angle = 22.5 * 3.14159265358979323846 / 180.0;
    double t = my_tan(angle);
    float tf = (float)t;
    printf("angle: 0x%08lx\n", *(unsigned long*)&angle);
    printf("tan d: 0x%08lx\n", *(unsigned long*)&t);
    printf("tan f: 0x%08lx (expected ~0.4142, hex ~3ed413cd)\n", *(unsigned long*)&tf);
    return 0;
}
