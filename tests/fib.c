#include <stdio.h>
#include <stdlib.h>

// computes a number of fibbonnaci sequence
int fib(int n)
{
    int result = 1, prev = 0;

    while (n > 0)
    {
        int current = result;

        result += prev;
        prev = current;

        n -= 1;
    }

    return result;
}

int main(int argc, char *argv[])
{
    int n = 0, val = 0;

    if (argc < 2)
    {
        return -1;
    }

    n = atoi(argv[1]);
    val = fib(n);

    printf("%d number in Fibonacci sequence is %d\n", n, val);

    return 0;
}

