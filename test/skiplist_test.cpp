#include <iostream>
#include "../skiplist/skiplist.h"
#define FILE_PATH "../store/dumpFile"

int main() {

    // 键值中的key用int型，如果用其他类型，需要自定义比较函数
    // 而且如果修改key的类型，同时需要修改skipList.load_file函数
    SkipList<int, std::string> skipList(6);
	skipList.insert_element(1, "awei"); 
	skipList.insert_element(2, "2023年"); 
	skipList.insert_element(3, "一定"); 
	skipList.insert_element(5, "会"); 
	skipList.insert_element(7, "瘦掉60斤"); 
	skipList.insert_element(9, "取得良好的考研成绩"); 
	skipList.insert_element(9, "上岸中国海洋大学"); 

    std::cout << "skipList size:" << skipList.size() << std::endl;

    skipList.dump_file();

    // skipList.load_file();

    skipList.search_element(1);
    skipList.search_element(9);


    skipList.display_list();

    skipList.delete_element(5);
    skipList.delete_element(7);

    std::cout << "skipList size:" << skipList.size() << std::endl;

    skipList.display_list();
}
