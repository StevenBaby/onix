#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/math.h>

int main(int argc, char const *argv[])
{
    double x = 3.0;

    printf("sin(%f) = %f\n", x, sin(x));
    printf("cos(%f) = %f\n", x, cos(x));
    printf("tan(%f) = %f\n", x, tan(x));
    printf("sqrt(%f) = %f\n", x, sqrt(x));
    printf("log2(%f) = %f\n", x, log2(x));
    return 0;
}
