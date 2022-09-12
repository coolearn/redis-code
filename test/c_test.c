#include <stdio.h>

int main(void){

    // a存储可以是一个数字也可以是一个地址
    int a = 10; //变量a是一个存储单元，a代表俩个值：单元地址 和 存储单元中的数据
    int *b = &a;

    printf("%d\n", a); // 单元数据
    printf("%d\n", &a); // 单元地址

    printf("%d\n", b);  // 
    printf("%d\n", *b);

    // 其实之所以会产生&a和*b的值应该是一样的误解，在于对int *b = &a；这句话的理解不到位。
    // 这句是做了两件事，定义和赋值（int* b; b=&a;）。
    // 第一步：定义，int *b和int* b两种写法都是可以的，意思是定义了一个指针变量b；
    // 第二步：赋值，b=&a；这里其实是取了a的地址(&是取地址的操作)，赋值给b，那么b的值就是a的地址。
    // printf("%d\n",*b); 这个里面的*b是将b的内容作为地址，通过这个地址再去取对应的值。跟int *b是不同的，不能混淆。


    printf("%d\n",sizeof(char)); // 1
    printf("%d\n",sizeof(short)); // 2
    printf("%d\n",sizeof(int)); // 4
    printf("%d\n",sizeof(long)); // 8 
    printf("%d\n",sizeof(long long )); // 8
    printf("%d\n",sizeof(float)); // 4 
    printf("%d\n",sizeof(double)); // 8 


    return 0;
}