#ifndef SKIPLIST_H
#define SKIPLIST_H

// TODO: Add the adequate functions to replace MySql in this server
// TODO: RAII in the resource
// TODO: Synchronization mode in Reader-Writer model
// TODO: Raft
// TODO: Template different key and value
// TODO: Printf replace cout function

// *!* this is just a toy storage engine and still has a long way to the really database. 

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <mutex>

#include "../locker/locker.h"

#define STORE_FILE "../store/dumpFile"

std::string delimiter = ":";

locker m_skipListLocker;      // locker for critical section

// class template to implement node
template<typename K,typename V>
class Node
{
public:
    Node() {}

    Node(K k,V v,int);

    ~Node();

    K get_key() const;
    V get_value() const;

    void set_Value(V);

    Node<K,V> **forward; // Linear array to hold pointers to next node of different level

    int node_level;

private:
    
    // Key-value node
    K key;
    V value;   

};

template<typename K,typename V>
Node<K,V>::Node(const K k,const V v,int level)
{
    this->key = k;
    this->value = v;
    this->node_level = level;

    // level + 1, because array index is from 0 to level
    this->forward = new Node<K,V>* [level+1]; 

    // SkipList = List + binary search List
    memset(this->forward, 0, sizeof(Node<K,V>*) * (level + 1) ); // Full forward array with 0
};

template<typename K,typename V>
Node<K,V>::~Node()
{
    delete []forward;
};

template<typename K,typename V>
void Node<K,V>::set_Value(V value)
{
    this->value = value;
};

template<typename K,typename V>
K Node<K,V>::get_key() const
{
    return key;
};

template<typename K,typename V>
V Node<K,V>::get_value() const
{
    return value;
};

// The data structure of the skip list is shown in the ../TinyWebServer.drawio
// Class template for Skip list
template<typename K,typename V>
class SkipList
{
public:
    SkipList(int);
    ~SkipList();

    int get_random_level();   

    Node<K,V>* create_node(K, V, int); 

    int  insert_element(K ,V);
    bool search_element(K);
    void delete_element(K);

    void display_list();

    void dump_file();
    void load_file();

    int size();


private:
    void get_key_value_from_string(const std::string& str,std::string* key,std::string* value);
    bool is_valid_string(const std::string& str);

private:
    
    // The max level of this skip list
    int _max_level;

    // The current level of this skip list
    int _skip_list_level;

    // the pointer to the head node of this skip list
    Node<K,V> *_header;

    // file operator
    std::ofstream _file_writer;    // writer
    std::ifstream _file_reader;    // reader   

    int _element_count;            // the current element count of this SkipList

};

// *!* Public function

/*
Take this following SkipList as an example

level  3          1                                                  10


level 2           1                     5                            10


level 1           1          3          5                 8          10        

    
level 0          1     2     3    4     5           7     8    9     10               

The fast table formed by adding a multi row index to  a common linked list\
is the key data structure of Redis SortedSet and LevelDB MemTable. 

Compared with the common linked list, the time complexity of its\
insert/delete/search operations  can be O(log(n))

*/

template<typename K,typename V>
SkipList<K,V>::SkipList(int max_level)
{
    this->_max_level       = max_level;
    this->_skip_list_level = 0;
    this->_element_count   = 0;

    // Create header node and initialize key and value of a null node
    K key;
    V value;
    this->_header = new Node<K,V>(key, value, _max_level);

}

template<typename K,typename V>
SkipList<K,V>::~SkipList()
{
    if( _file_writer.is_open() )
    {
        _file_writer.close();
    }

    if( _file_reader.is_open() )
    {
        _file_reader.close();
    }

    delete _header;

}

template<typename K,typename V>
int SkipList<K,V>::get_random_level()
{
    int k = 1;
    while( rand() % 2 )
    {
        k++;
    }

    k = (k < _max_level) ? k : _max_level;

    return k;
}

// Insert element in this skip list
// return 1 means element exiting
// return 0 means insert the new element successful

/*
Insert the element 6 in the Skip List


level  3  +----->  1                                                  10
                   |
                   |
level 2           1 ------------------>  5                           10
                                         |
                                         |   insert
level 1           1          3          5 -> | 6 |        8          10        
                                             |   |
level 0          1     2     3    4     5    | 6 |  7     8    9     10               


*/

// Create new Node
template<typename K,typename V>
Node<K,V>* SkipList<K,V>::create_node(const K key,const V value,int level)
{
    Node<K,V> *newNode = new Node<K,V>(key, value, level);
    return newNode;
}

template<typename K,typename V>
int SkipList<K,V>::insert_element(const K key,const V value)
{
    m_skipListLocker.lock();

    Node<K, V> *current = this->_header;

    // Create an update array and Initialize it
    // Update the array when we put this node so that node->forward[i] could be operated later

    Node<K,V> *update[_max_level + 1];
    memset(update, 0, sizeof(Node<K,V>*) * (_max_level + 1));

    // Begin at the highest level of this skip list
    for( int i = _skip_list_level; i >= 0; i-- )
    {
        while( current->forward[i] != NULL &&  current->forward[i]->get_key() < key)
        {
            current = current->forward[i];
        }
        update[i] = current;
    }

    // Inserting the new node needs to begin at the level 0
    current = current->forward[0];

    // If this node is exit, we should get its key
    if( current != NULL && current->get_key() == key )
    {
        std::cout << "Key: " << key << ", exists" << std::endl;
        m_skipListLocker.unlock();
        return 1;   // Tips: Return 1 = this element exists
    }

    // If this node is not exit, we should insert the new node
    // If this key of this node isn't equal the key of the insert node\
    // initializing update value with the header pointer
    if( current == NULL || current->get_key() != key )
    {
        // Generating a random level of this new node
        int random_level = get_random_level();

        // If random_level is more than the current skip_list_level, initializing update \
        // value with the header pointer
        if( random_level > _skip_list_level )
        {
            for( int i = _skip_list_level + 1; i < random_level + 1;i++ )
            {
                update[i] = _header;
            }

            _skip_list_level = random_level;
        }

        // Creating new node with the generated random level
        Node<K,V>* inserted_node = create_node(key,value,random_level);

        // Inserting the new node
        for(int i = 0; i <= random_level;i++)
        {
            inserted_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = inserted_node;
        }

        std::cout << "Inserting key: " << key << " - value: " << value << std::endl;
        _element_count++;

    }

    m_skipListLocker.unlock();
    return 0;

}

template<typename K,typename V>
void SkipList<K,V>::delete_element(K key)
{
    m_skipListLocker.lock();
    Node<K,V>* current = this->_header;
    Node<K,V> *update[_max_level + 1];

    memset(update, 0, sizeof( Node<K,V>* ) * (_max_level + 1));

    // Begin at the highest level of this skip list
    for( int i = _skip_list_level; i >= 0; i-- )
    {
        while( current->forward[i]!= NULL &&  current->forward[i]->get_key() < key )
        {
            current = current->forward[i];
        }

        update[i] = current;
    }

    current = current->forward[0];   // The first Node in the skip list
    
    if( current != nullptr && current->get_key() == key )
    {
        // Starting at the lowest level of the skip list and Deleting this node in each level
        for( int i = 0; i <= _skip_list_level; i++ )
        {
            // If this node not exits in this level, breaking it
            if( update[i]->forward[i] != current )
            {
                break;
            }

            update[i]->forward[i] = current->forward[i]; // Move the node to the next level
        }

        // Removing the level which not exits any elements
        while( _skip_list_level > 0 && _header->forward[_skip_list_level] == 0 )
        {
            _skip_list_level--;
        }

        std::cout << "Successfully deleting the key : " << key << std::endl;
        _element_count--;

    }

    m_skipListLocker.unlock();
    return ;

}

template<typename K,typename V>
bool SkipList<K,V>::search_element(K key)
{
    std::cout << "Begin to search_element " << std::endl;
    // The search operation not need to mutex lock
    // m_skipListLocker.lock();

    Node<K,V>* current = _header;

    // Begin at the highest level of this skip list
    for( int i = _skip_list_level; i >= 0; i-- )
    {
        while( current->forward[i]!= NULL &&  current->forward[i]->get_key() < key )
        {
            current = current->forward[i];
        }

    }

    current = current->forward[0];   // The first Node in the skip list

    // Current_node exits
    if( current != nullptr && current->get_key() == key )
    {
        std::cout << "Found the key : " << key << " - value:  "<< current->get_value() << std::endl;
        return true;
    }

    // m_skipListLocker.unlock();

    std::cout << "Failed to find the key : " << key << std::endl;
    return false;
    
}

template<typename K,typename V>
void SkipList<K,V>::display_list()
{
    std::cout << "\n ---------------- Skip List --------------- " << "\n";
    for( int i = 0; i <= _skip_list_level; i++ )
    {
        std::cout << "--------------------------------------------" << std::endl;

        Node<K,V>* current = this->_header->forward[i];
        std::cout << "Level : " << i << std::endl;

        while( current!= nullptr )
        {   
            std::cout << "Key : " << current->get_key() << " , Value: " << current->get_value() << std::endl;
            current = current->forward[i];
        }

        std::cout << std::endl;

        std::cout << "--------------------------------------------" << std::endl;
    }

}

template<typename K,typename V>
void SkipList<K,V>::dump_file()
{
    std::cout << "\n ------------- Dump File --------------- " << "\n";

    _file_writer.open(STORE_FILE);   // ../store/dumpFile

    Node<K,V> *current = this->_header->forward[0];

    while( current != nullptr )
    {
        _file_writer << current->get_key() << " : " << current->get_value() << "\n";
        std::cout    << current->get_key() << " : " << current->get_value() << "\n";
        current = current->forward[0];
    }

    _file_writer.flush();
    _file_writer.close();

    return ;
    
}

template<typename K,typename V>
void SkipList<K,V>::load_file()
{
    _file_reader.open(STORE_FILE);   // ../store/dumpFile

    std::cout << "\n ------------- Load File --------------- " << "\n";

    std::string line;
    std::string* key   = new std::string();
    std::string* value = new std::string();

    while( getline(_file_reader, line) )
    {
        get_key_value_from_string(line, key, value);

        if( key->empty() || value->empty() )
        {
            continue;
        }

        insert_element(*key,*value);

      std::cout << "Key : " << *key << " , Value: " << *value << std::endl;

    }

    _file_reader.close();

}

template<typename K,typename V>
int SkipList<K,V>::size()
{
    return _element_count;
}

// *!* Private Function

template<typename K,typename V>
void SkipList<K,V>::get_key_value_from_string(const std::string& str,std::string* key,std::string* value)
{
    if( !is_valid_string(str) )
    {
        return;
    }

    // delimiter = ":"
    *key = str.substr(0, str.find(delimiter));    
    *value = str.substr(str.find(delimiter) + 1, str.length());

}

template<typename K,typename V>
bool SkipList<K,V>::is_valid_string(const std::string& str)
{
    if( str.empty() )
    {
        return false;
    }

    if( str.find(delimiter) == std::string::npos )
    {
        return false;
    }

    return true;

}


#endif /* SKIPLIST_H */