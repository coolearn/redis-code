#include <stdio.h>
#include "../adlist.c"
#include "../zmalloc.c"


// gcc -o adlist adlist_test.c

// 遍历List集合
void print_list(list *list){

    listIter *iter;
    listNode *node;

    printf("当前双链表的值为: ");
    iter = listGetIterator(list, AL_START_HEAD);
    while((node = listNext(iter)) != NULL) {
        // print("%d ", *(node -> value)); // value 是一个指针
        printf("%d ", *(int*)(node -> value)); // value 是一个指针
    }
    printf("\n");

    listReleaseIterator(iter);

}

int main(){

    listNode *node; //定义一个node指针
    list *list,*copy; // 定义俩个指针
    int a[10] = {1,2,3,4,5,6,7,8,9,10};

    list = listCreate();
    
    for(int i = 0; i < sizeof(list); i++){
        printf("%02x ",(unsigned int) ((char*)&list)[i]);
    }

    printf("\n");

    list = listAddNodeHead(list , &a[0]); //添加第一个元素

    print_list(list);

    list = listAddNodeHead(list , &a[2] ); //添加第一个元素
    print_list(list);

    list = listAddNodeHead(list , &a[4] ); //添加第一个元素
    print_list(list);

    list = listAddNodeTail(list , &a[9] ); //添加第一个元素
    print_list(list);

    node = listIndex(list,2); //找到元素

    printf("找到node2节点:%d \n",*(int*)(node->value));
    printf("找到node节点的next::%d \n",*(int*)(node->next->value));
    printf("找到node节点的prev:%d \n",*(int*)(node->prev->value));

    listDelNode(list,node); // 删除节点
    
    print_list(list);

    copy = listDup(list);
    
    printf("复制之后的数组\n");
    print_list(list);

    listRelease(list); //内存释放

    return 0;
}


