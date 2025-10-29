#include "MicroHash.h"

uint8_t Userdata[128] = {0};

MicroHash_Handle_t HashTable = NULL;

int main(void)
{
    for(int i = 0; i < 128; i++)
    {
        Userdata[i] = i * 12; // 模拟有效数据
    }

    MicroHash_Conf_t conf = {
        .buckSize = 10,
    };
    MicroHash_Init(&HashTable,&conf);

    // 插入
    MicroHash_Insert(&HashTable,1,(void*)&Userdata[1]);

    // 查找
    uint8_t *data = (uint8_t*)MicroHash_Find(HashTable,1);


    // 移除
    MicroHash_Remove(HashTable,1);

    // 删除哈希表
    MicroHash_Delete(&HashTable);
    
    return 0;
}