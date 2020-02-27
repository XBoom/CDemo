#include <stdio.h>
#include "libtest.h"
#include "cJSON.h"


#define JSON_CON_LEN 10000
void test_macro()
{
	#ifdef TEST_A
		printf("hello\n");
	#endif

	#if TEST_B == 2
		printf("world\n");
	#endif
}

int main(void)
{
	test_macro();
	int a = 10, b = 5;
	printf("a + b = %d\n", add(a,b));
    printf("a - b = %d\n", sub(a,b));
    return 0;
}
