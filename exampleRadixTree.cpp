#include <string>
#include <iostream>
#include <cstdlib>

#include "radix_tree.hpp"

radix_tree<std::string, int> tree;
radix_tree<std::vector<uint8_t>,int> tree1;


void insert() {
    tree["apache"]    = 0;
    tree["afford"]    = 1;
    tree["available"] = 2;
    tree["affair"]    = 3;
    tree["avenger"]   = 4;
    tree["binary"]    = 5;
    tree["bind"]      = 6;
    tree["brother"]   = 7;
    tree["brace"]     = 8;
    tree["blind"]     = 9;
    tree["bro"]       = 10;

    tree1[{0,0,0,0}]      = 1;
    tree1[{1,1,1,1}]      = 2;
    tree1[{2,2,2,2}]      = 3;
    tree1[{3,3,3,3}]      = 4;
    tree1[{4,4,4,4}]      = 5;
    tree1[{5,5,5,5}]      = 6;
}

void longest_match(std::string key)
{
    radix_tree<std::string, int>::iterator it;

    it = tree.longest_match(key);

    std::cout << "longest_match(\"" << key << "\"):" << std::endl;

    if (it != tree.end()) {
        std::cout << "    " << it->first << ", " << it->second << std::endl;
    } else {
        std::cout << "    failed" << std::endl;
    }
}

void prefix_match(std::string key)
{
    std::vector<radix_tree<std::string, int>::iterator> vec;
    std::vector<radix_tree<std::string, int>::iterator>::iterator it;

    tree.prefix_match(key, vec);

    std::cout << "prefix_match(\"" << key << "\"):" << std::endl;

    for (it = vec.begin(); it != vec.end(); ++it) {
        std::cout << "    " << (*it)->first << ", " << (*it)->second << std::endl;
    }
}

void greedy_match(std::string key)
{
    std::vector<radix_tree<std::string, int>::iterator> vec;
    std::vector<radix_tree<std::string, int>::iterator>::iterator it;

    tree.greedy_match(key, vec);

    std::cout << "greedy_match(\"" << key << "\"):" << std::endl;

    for (it = vec.begin(); it != vec.end(); ++it) {
        std::cout << "    " << (*it)->first << ", " << (*it)->second << std::endl;
    }
}

void traverse() {
    radix_tree<std::string, int>::iterator it;

    std::cout << "traverse:" << std::endl;
    for (it = tree.begin(); it != tree.end(); ++it) {
        std::cout << "    " << it->first << ", " << it->second << std::endl;
    }
    radix_tree<std::vector<uint8_t>,int>::iterator it1;
    for(it1 = tree1.begin(); it1 != tree1.end(); ++it1)
    {
        for(uint8_t elem:it1->first) 
            std::cout<<static_cast<int>(elem)<<' ';
        std::cout<<"\t\t\t\t\t\t, "<<it1->second<<std::endl;
    }
}

int main()
{
    insert();

    longest_match("binder");
    longest_match("bracelet");
    longest_match("apple");

    prefix_match("aff");
    prefix_match("bi");
    prefix_match("a");

    greedy_match("avoid");
    greedy_match("bring");
    greedy_match("attack");

    traverse();

    //tree.erase("bro");
    prefix_match("bro");


    return EXIT_SUCCESS;
}
