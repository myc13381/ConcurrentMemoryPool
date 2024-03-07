#ifndef RADIX_TREE_HPP
#define RADIX_TREE_HPP

#include <map>
#include <cassert>
#include <string>
#include <utility>
#include <vector>
#include <iterator>
#include <functional>

// -------------------------------------------------
// help function
// -------------------------------------------------

template<typename K>
K radix_substr(const K &key, int begin, int num);

template<>
inline std::string radix_substr<std::string>(const std::string &key, int begin, int num)
{
    return key.substr(begin, num);
}

template<>
inline std::vector<uint8_t> radix_substr<std::vector<uint8_t>>(const std::vector<uint8_t> &key, int begin, int num)
{
    std::vector<uint8_t> &v = const_cast<std::vector<uint8_t>&>(key);
    std::vector<uint8_t>::iterator start = v.begin();
    std::vector<uint8_t>::iterator end = start+num;
    return std::vector<uint8_t>(start,end);
}


template<typename K>
K radix_join(const K &key1, const K &key2);

template<>
inline std::string radix_join<std::string>(const std::string &key1, const std::string &key2)
{
    return key1 + key2;
}

template<>
inline std::vector<uint8_t> radix_join<std::vector<uint8_t>>(const std::vector<uint8_t> &key1, const std::vector<uint8_t> &key2)
{
    std::vector<uint8_t> ret=key1;
    for(uint8_t elem:key2) ret.emplace_back(elem);
    return ret;
}

template<typename K>
int radix_length(const K &key);

template<>
inline int radix_length<std::string>(const std::string &key)
{
    return static_cast<int>(key.size());
}

template<>
inline int radix_length<std::vector<uint8_t>>(const std::vector<uint8_t> &key)
{
    return static_cast<int>(key.size());
}

// -------------------------------------------------
// 基数树节点
// -------------------------------------------------

// forward declaration
template <typename K, typename T, typename Compare> class radix_tree; 
template <typename K, typename T, class Compare >class radix_tree_it;

template <typename K, typename T, typename Compare>
class radix_tree_node {
    friend class radix_tree<K, T, Compare>;
    friend class radix_tree_it<K, T, Compare>;

    typedef std::pair<const K, T> value_type;
    typedef typename std::map<K, radix_tree_node<K, T, Compare>*, Compare >::iterator it_child; // 类型定义

private:
	radix_tree_node(Compare& pred) : // 初始化列表
        m_children(std::map<K, radix_tree_node<K, T, Compare>*, Compare>(pred)), 
        m_parent(NULL), m_value(NULL), 
        m_depth(0), 
        m_is_leaf(false), m_key(), 
        m_pred(pred) {}

    radix_tree_node(const value_type &val, Compare& pred);
    radix_tree_node(const radix_tree_node&); // delete
    radix_tree_node& operator=(const radix_tree_node&); // delete

    ~radix_tree_node();

    std::map<K, radix_tree_node<K, T, Compare>*, Compare> m_children; // 每个节点维护一个map
    radix_tree_node<K, T, Compare> *m_parent; // 父节点
    value_type *m_value; // pair<cosnt K, T> 类型的指针
    int m_depth; // 深度
    bool m_is_leaf; // 是不是叶子节点
    K m_key; // K
	Compare& m_pred;
};

template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>::radix_tree_node(const value_type &val, Compare& pred) :
    m_children(std::map<K, radix_tree_node<K, T, Compare>*, Compare>(pred)),
    m_parent(NULL),
    m_value(NULL),
    m_depth(0),
    m_is_leaf(false),
    m_key(), 
	m_pred(pred)
{
    m_value = new value_type(val);
}

template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>::~radix_tree_node()
{
#ifndef __linux__
    it_child it;
    for (it = m_children.begin(); it != m_children.end(); ++it) {
            delete it->second;
    }
    delete m_value;
#endif
}



// -------------------------------------------------
// 基数树迭代器
// -------------------------------------------------

// forward declaration
template <typename K, typename T, class Compare = std::less<K> > class radix_tree;
template <typename K, typename T, class Compare = std::less<K> > class radix_tree_node;

template <typename K, typename T, class Compare = std::less<K> >
class radix_tree_it : public  std::iterator<std::forward_iterator_tag, std::pair<K, T>> {
    friend class radix_tree<K, T, Compare>;

public:
    radix_tree_it() : m_pointee(0) { }
    radix_tree_it(const radix_tree_it& r) : m_pointee(r.m_pointee) { } // 复制构造函数
    radix_tree_it& operator=(const radix_tree_it& r) 
    {
        if(this != &r)
        {
            m_pointee = r.m_pointee; 
        }
        return *this; 
    }
    ~radix_tree_it() { }

    std::pair<const K, T>& operator*  () const;
    std::pair<const K, T>* operator-> () const;
    const radix_tree_it<K, T, Compare>& operator++ ();
    radix_tree_it<K, T, Compare> operator++ (int);
    // const radix_tree_it<K, T, Compare>& operator-- ();
    bool operator!= (const radix_tree_it<K, T, Compare> &lhs) const;
    bool operator== (const radix_tree_it<K, T, Compare> &lhs) const;

private:
    radix_tree_node<K, T, Compare> *m_pointee; // 指向节点的指针，核心成员变量
    radix_tree_it(radix_tree_node<K, T, Compare> *p) : m_pointee(p) { }

    radix_tree_node<K, T, Compare>* increment(radix_tree_node<K, T, Compare>* node) const; // 移动到上一个
    radix_tree_node<K, T, Compare>* descend(radix_tree_node<K, T, Compare>* node) const; // 一直向下找到第一个叶子节点
};

// 这个函数试图找到给定节点node在基数树中的下一个节点。
// 它首先检查该节点是否有父节点，如果没有（即该节点是树的根节点），则返回NULL。
// 如果有父节点，它就在父节点的子节点集合中查找node，并尝试找到node之后的下一个子节点。
//  -->  如果找到了下一个子节点，就通过调用descend函数下降到那个子节点；
//  -->  如果没有找到（即node是其父节点的最后一个子节点），则递归地对父节点调用increment函数。

template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>* radix_tree_it<K, T, Compare>::increment(radix_tree_node<K, T, Compare>* node) const
{
	radix_tree_node<K, T, Compare>* parent = node->m_parent;

    if (parent == NULL)
        return NULL;

    typename radix_tree_node<K, T, Compare>::it_child it = parent->m_children.find(node->m_key);
    assert(it != parent->m_children.end()); // 如果找不到，直接终止，因为这个节点不存在
    ++it; // 我们的目的是找到下一个节点，所以 ++it

    if (it == parent->m_children.end())
        return increment(parent);
    else
        return descend(it->second);
}

// 从给定的节点node开始，下降到其第一个子节点（如果node不是叶子节点）。
// 如果node是叶子节点，则直接返回node。
// 否则，它获取node的第一个子节点，并递归地对那个子节点调用descend函数。
template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>* radix_tree_it<K, T, Compare>::descend(radix_tree_node<K, T, Compare>* node) const
{
    if (node->m_is_leaf)
        return node;

    typename radix_tree_node<K, T, Compare>::it_child it = node->m_children.begin();

    assert(it != node->m_children.end());

    return descend(it->second);
}

template <typename K, typename T, typename Compare>
std::pair<const K, T>& radix_tree_it<K, T, Compare>::operator* () const
{
    return *m_pointee->m_value;
}

template <typename K, typename T, typename Compare>
std::pair<const K, T>* radix_tree_it<K, T, Compare>::operator-> () const
{
    return m_pointee->m_value;
}

template <typename K, typename T, typename Compare>
bool radix_tree_it<K, T, Compare>::operator!= (const radix_tree_it<K, T, Compare> &lhs) const
{
    return m_pointee != lhs.m_pointee; // 指针不相同
}

template <typename K, typename T, typename Compare>
bool radix_tree_it<K, T, Compare>::operator== (const radix_tree_it<K, T, Compare> &lhs) const
{
    return m_pointee == lhs.m_pointee;
}

template <typename K, typename T, typename Compare>
const radix_tree_it<K, T, Compare>& radix_tree_it<K, T, Compare>::operator++ ()
{
    if (m_pointee != NULL) // it is undefined behaviour to dereference iterator that is out of bounds...
        m_pointee = increment(m_pointee);
    return *this;
}

template <typename K, typename T, typename Compare>
radix_tree_it<K, T, Compare> radix_tree_it<K, T, Compare>::operator++ (int)
{
    radix_tree_it<K, T, Compare> copy(*this);
    ++(*this);
    return copy;
}


// -------------------------------------------------
// 基数树
// -------------------------------------------------
template <typename K, typename T, typename Compare>
class radix_tree {
public:
    typedef K key_type;
    typedef T mapped_type;
    typedef std::pair<const K, T> value_type;
    typedef radix_tree_it<K, T, Compare>   iterator;
    typedef std::size_t           size_type;

	radix_tree() : m_size(0), m_root(NULL), m_predicate(Compare()) { }
	explicit radix_tree(Compare pred) : m_size(0), m_root(NULL), m_predicate(pred) { }
    ~radix_tree() {
        delete m_root;
    }

    size_type size()  const {
        return m_size;
    }
    bool empty() const {
        return m_size == 0;
    }
    void clear() {
        delete m_root;
        m_root = NULL;
        m_size = 0;
    }

    iterator find(const K &key);
    iterator begin();
    iterator end();

    std::pair<iterator, bool> insert(const value_type &val);
    bool erase(const K &key);
    void erase(iterator it);
    void prefix_match(const K &key, std::vector<iterator> &vec);
    void greedy_match(const K &key,  std::vector<iterator> &vec); // 贪婪匹配
    iterator longest_match(const K &key);

    T& operator[] (const K &lhs);

	template<class _UnaryPred> void remove_if(_UnaryPred pred)
	{
		radix_tree<K, T, Compare>::iterator backIt;
		for (radix_tree<K, T, Compare>::iterator it = begin(); it != end(); it = backIt)
		{
			backIt = it;
			backIt++;
			K toDelete = (*it).first;
			if (pred(toDelete))
			{
				erase(toDelete);
			}
		}
	}


private:
    size_type m_size; // 节点的个数
    radix_tree_node<K, T, Compare>* m_root; // 根节点

	Compare m_predicate; // 比较函数

    radix_tree_node<K, T, Compare>* begin(radix_tree_node<K, T, Compare> *node);
    radix_tree_node<K, T, Compare>* find_node(const K &key, radix_tree_node<K, T, Compare> *node, int depth);
    radix_tree_node<K, T, Compare>* append(radix_tree_node<K, T, Compare> *parent, const value_type &val);
    radix_tree_node<K, T, Compare>* prepend(radix_tree_node<K, T, Compare> *node, const value_type &val);
	void greedy_match(radix_tree_node<K, T, Compare> *node, std::vector<iterator> &vec);

    radix_tree(const radix_tree& other); // delete 复制构造函数
    radix_tree& operator =(const radix_tree other); // delete
};

template <typename K, typename T, typename Compare>
void radix_tree<K, T, Compare>::prefix_match(const K &key, std::vector<iterator> &vec)
{
    vec.clear();

    if (m_root == NULL)
        return;

    radix_tree_node<K, T, Compare> *node;
    K key_sub1, key_sub2;

    node = find_node(key, m_root, 0);

    if (node->m_is_leaf)
        node = node->m_parent;
    
    // 判断找到的这个node是不是完全匹配
    int len = radix_length(key) - node->m_depth; // 查询的 key 除去 node 节点前缀长度后剩下的部分长度
    key_sub1 = radix_substr(key, node->m_depth, len); // key 剩下要比较的部分
    key_sub2 = radix_substr(node->m_key, 0, len); // node 中实际对应的部分

    if (key_sub1 != key_sub2) // node 前缀不匹配
        return;

    greedy_match(node, vec); // 完全匹配的前缀才加入
}

template <typename K, typename T, typename Compare>
typename radix_tree<K, T, Compare>::iterator radix_tree<K, T, Compare>::longest_match(const K &key)
{
    if (m_root == NULL)
        return iterator(NULL);

    radix_tree_node<K, T, Compare> *node;
    K key_sub;

    node = find_node(key, m_root, 0);

    if (node->m_is_leaf)
        return iterator(node);

    key_sub = radix_substr(key, node->m_depth, radix_length(node->m_key));

    if (! (key_sub == node->m_key)) // node 和 key并不完全匹配
        node = node->m_parent;

    K nul = radix_substr(key, 0, 0);

    while (node != NULL) {
        typename radix_tree_node<K, T, Compare>::it_child it;
        it = node->m_children.find(nul);
        if (it != node->m_children.end() && it->second->m_is_leaf)
            return iterator(it->second);

        node = node->m_parent;
    }

    return iterator(NULL);
}


template <typename K, typename T, typename Compare>
typename radix_tree<K, T, Compare>::iterator radix_tree<K, T, Compare>::end()
{
    return iterator(NULL);
}

template <typename K, typename T, typename Compare>
typename radix_tree<K, T, Compare>::iterator radix_tree<K, T, Compare>::begin()
{
    radix_tree_node<K, T, Compare> *node;

    if (m_root == NULL || m_size == 0)
        node = NULL;
    else
        node = begin(m_root);

    return iterator(node);
}

template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>* radix_tree<K, T, Compare>::begin(radix_tree_node<K, T, Compare> *node)
{
    if (node->m_is_leaf)
        return node;


    assert(!node->m_children.empty());

    return begin(node->m_children.begin()->second);
}

template <typename K, typename T, typename Compare>
T& radix_tree<K, T, Compare>::operator[] (const K &lhs)
{
    iterator it = find(lhs);

    if (it == end()) {
        std::pair<K, T> val;
        val.first = lhs;

        std::pair<iterator, bool> ret;
        ret = insert(val);

        assert(ret.second == true);

        it = ret.first;
    }

    return it->second;
}

// 贪婪匹配，匹配前缀相同的
template <typename K, typename T, typename Compare>
void radix_tree<K, T, Compare>::greedy_match(const K &key, std::vector<iterator> &vec)
{
    radix_tree_node<K, T, Compare> *node;

    vec.clear();

    if (m_root == NULL)
        return;

    node = find_node(key, m_root, 0);

    if (node->m_is_leaf)
        node = node->m_parent;

    greedy_match(node, vec);
}

template <typename K, typename T, typename Compare>
void radix_tree<K, T, Compare>::greedy_match(radix_tree_node<K, T, Compare> *node, std::vector<iterator> &vec)
{
    if (node->m_is_leaf) {
        vec.push_back(iterator(node));
        return;
    }

	typename std::map<K, radix_tree_node<K, T, Compare>*>::iterator it;

    for (it = node->m_children.begin(); it != node->m_children.end(); ++it) {
        greedy_match(it->second, vec);
    }
}

template <typename K, typename T, typename Compare>
void radix_tree<K, T, Compare>::erase(iterator it)
{
    erase(it->first);
}

template <typename K, typename T, typename Compare>
bool radix_tree<K, T, Compare>::erase(const K &key)
{
	if (m_root == NULL)
		return 0;

	radix_tree_node<K, T, Compare> *child;
    radix_tree_node<K, T, Compare> *parent;
    radix_tree_node<K, T, Compare> *grandparent;
    K nul = radix_substr(key, 0, 0);

    child = find_node(key, m_root, 0);

    if (! child->m_is_leaf) // 不是叶子节点，无需删除
        return 0;

// 正式开始删除操作
    parent = child->m_parent;
    parent->m_children.erase(nul);

    delete child;

    m_size--;

    if (parent == m_root)
        return 1;

    if (parent->m_children.size() > 1)
        return 1;
    // 删除children 后 parent 的孩子少于等于1个，可以继续处理
    if (parent->m_children.empty()) { // parent 没有 cildren, grandparent 删除 parent
        grandparent = parent->m_parent;
        grandparent->m_children.erase(parent->m_key);
        delete parent;
    } else { // parent 有一个 chiledren
        grandparent = parent;
    }

    if (grandparent == m_root) {
        return 1;
    }

    // 祖先节点只有一个孩子，进行合并
    if (grandparent->m_children.size() == 1) {
        // merge grandparent with the uncle
        typename std::map<K, radix_tree_node<K, T, Compare>*>::iterator it;
        it = grandparent->m_children.begin();

        radix_tree_node<K, T, Compare> *uncle = it->second;

        if (uncle->m_is_leaf)
            return 1;

        uncle->m_depth = grandparent->m_depth;
        uncle->m_key   = radix_join(grandparent->m_key, uncle->m_key);
        uncle->m_parent = grandparent->m_parent;

        grandparent->m_children.erase(it);

        grandparent->m_parent->m_children.erase(grandparent->m_key);
        grandparent->m_parent->m_children[uncle->m_key] = uncle;

        delete grandparent;
    }

    return 1;
}


template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>* radix_tree<K, T, Compare>::append(radix_tree_node<K, T, Compare> *parent, const value_type &val)
{
    int depth;
    int len;
    K   nul = radix_substr(val.first, 0, 0);
    radix_tree_node<K, T, Compare> *node_c, *node_cc;

    depth = parent->m_depth + radix_length(parent->m_key);
    len   = radix_length(val.first) - depth;

    if (len == 0) {
        node_c = new radix_tree_node<K, T, Compare>(val, m_predicate);

        node_c->m_depth   = depth;
        node_c->m_parent  = parent;
        node_c->m_key     = nul;
        node_c->m_is_leaf = true;

        parent->m_children[nul] = node_c; // m_children[nul] 是一个叶子节点

        return node_c;
    } else {
        node_c = new radix_tree_node<K, T, Compare>(val, m_predicate);

        K key_sub = radix_substr(val.first, depth, len);

        parent->m_children[key_sub] = node_c;

        node_c->m_depth  = depth;
        node_c->m_parent = parent;
        node_c->m_key    = key_sub;


		node_cc = new radix_tree_node<K, T, Compare>(val, m_predicate);
        node_c->m_children[nul] = node_cc;

        node_cc->m_depth   = depth + len;
        node_cc->m_parent  = node_c;
        node_cc->m_key     = nul;
        node_cc->m_is_leaf = true;

        return node_cc;
    }
}

template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>* radix_tree<K, T, Compare>::prepend(radix_tree_node<K, T, Compare> *node, const value_type &val)
{
    int count;
    int len1, len2;

    len1 = radix_length(node->m_key);
    len2 = radix_length(val.first) - node->m_depth;

    for (count = 0; count < len1 && count < len2; count++) {
        if (! (node->m_key[count] == val.first[count + node->m_depth]) )
            break;
    }

    assert(count != 0);

    node->m_parent->m_children.erase(node->m_key); // 删除 node->m_key 键，因为要插入更小的键

    radix_tree_node<K, T, Compare> *node_a = new radix_tree_node<K, T, Compare>(m_predicate);

    // 插入更小的键 -- 公共部分
    node_a->m_parent = node->m_parent;
    node_a->m_key    = radix_substr(node->m_key, 0, count);
    node_a->m_depth  = node->m_depth;
    node_a->m_parent->m_children[node_a->m_key] = node_a;

    // 刚刚被删除的键去掉前缀后插入到前缀之后
    node->m_depth  += count;
    node->m_parent  = node_a;
    node->m_key     = radix_substr(node->m_key, count, len1 - count);
    node->m_parent->m_children[node->m_key] = node; // node 接在 node_a 之后

    // 插入 目标键值对
    K nul = radix_substr(val.first, 0, 0);
    if (count == len2) { // key查找完毕，放在 nul 节点即可
        radix_tree_node<K, T, Compare> *node_b;

        node_b = new radix_tree_node<K, T, Compare>(val, m_predicate);

        node_b->m_parent  = node_a;
        node_b->m_key     = nul;
        node_b->m_depth   = node_a->m_depth + count;
        node_b->m_is_leaf = true;
        node_b->m_parent->m_children[nul] = node_b;

        return node_b;
    } else { // count != len2，key 后面还有值
        radix_tree_node<K, T, Compare> *node_b, *node_c;

        node_b = new radix_tree_node<K, T, Compare>(m_predicate);

        node_b->m_parent = node_a;
        node_b->m_depth  = node->m_depth; // 此时，node_b 和 node 是同一层的节点
        node_b->m_key    = radix_substr(val.first, node_b->m_depth, len2 - count);
        node_b->m_parent->m_children[node_b->m_key] = node_b;

        node_c = new radix_tree_node<K, T, Compare>(val, m_predicate);

        node_c->m_parent  = node_b;
        node_c->m_depth   = radix_length(val.first);
        node_c->m_key     = nul;
        node_c->m_is_leaf = true;
        node_c->m_parent->m_children[nul] = node_c;

        return node_c;
    }
}

// 插入一个键值对
template <typename K, typename T, typename Compare>
std::pair<typename radix_tree<K, T, Compare>::iterator, bool> radix_tree<K, T, Compare>::insert(const value_type &val)
{
    if (m_root == NULL) { // 没有根节点先创建根节点
        K nul = radix_substr(val.first, 0, 0);

        m_root = new radix_tree_node<K, T, Compare>(m_predicate);
        m_root->m_key = nul;
    }


    radix_tree_node<K, T, Compare> *node = find_node(val.first, m_root, 0);

    if (node->m_is_leaf) {
        return std::pair<iterator, bool>(node, false); // 不需要/不允许插入
    } else if (node == m_root) {
        m_size++;
        return std::pair<iterator, bool>(append(m_root, val), true);
    } else { // 不完全匹配，插入
        m_size++;
        int len     = radix_length(node->m_key); // int len = min(radix_length(node->m_key), radix_lenght(val.first)-node->m_depth);
        K   key_sub = radix_substr(val.first, node->m_depth, len); // 但是 string.substr 保证了不会越界访问

        if (key_sub == node->m_key) {
            return std::pair<iterator, bool>(append(node, val), true);
        } else {
            return std::pair<iterator, bool>(prepend(node, val), true);
        }
    }
}

// 完全匹配返回迭代器，否则返回nullptr
template <typename K, typename T, typename Compare>
typename radix_tree<K, T, Compare>::iterator radix_tree<K, T, Compare>::find(const K &key)
{
    if (m_root == NULL)
        return iterator(NULL);

    radix_tree_node<K, T, Compare> *node = find_node(key, m_root, 0);

    // if the node is a internal node, return NULL
    if (! node->m_is_leaf)
        return iterator(NULL);

    return iterator(node);
}

template <typename K, typename T, typename Compare>
radix_tree_node<K, T, Compare>* radix_tree<K, T, Compare>::find_node(const K &key, radix_tree_node<K, T, Compare> *node, int depth)
{
    if (node->m_children.empty())
        return node;

    typename radix_tree_node<K, T, Compare>::it_child it;
    int len_key = radix_length(key) - depth; // 除去前面前缀的长度后剩下的长度

    for (it = node->m_children.begin(); it != node->m_children.end(); ++it) { // 从当前节点，开始寻找他的子节点，进行匹配
        if (len_key == 0) { // 前缀匹配完，还有两种情况，一种完全匹配，第二种是剩下的字符串超过了 key
            if (it->second->m_is_leaf)
                return it->second;
            else
                continue;
        }

        if (! it->second->m_is_leaf && key[depth] == it->first[0] ) { // 子节点的第一个字符匹配
            int len_node = radix_length(it->first);
            K   key_sub  = radix_substr(key, depth, len_node); // 继续判断这个子节点所有字符串是不是匹配目标对应部分

            if (key_sub == it->first) {
                return find_node(key, it->second, depth+len_node); // 匹配，继续向下寻找
            } else {
                return it->second; // 找不到了，返回最接近的节点
            }
        }
    }

    return node; 
}

/*

(root)
|
|---------------
|       |      |
abcde   bcdef  c
|   |   |      |------
|   |   $3     |  |  |
f   ge         d  e  $6
|   |          |  |
$1  $2         $4 $5

find_node():
  bcdef  -> $3
  bcdefa -> bcdef
  c      -> $6
  cf     -> c
  abch   -> abcde
  abc    -> abcde
  abcde  -> abcde
  abcdef -> $1
  abcdeh -> abcde
  de     -> (root)


(root)
|
abcd
|
$

(root)
|
$

*/

#endif // RADIX_TREE_HPP
