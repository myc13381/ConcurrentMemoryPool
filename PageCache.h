#pragma once

// 是否使用基数数作为映射结构
// #define USE_RADIX_TREE
// 使用 string 作为 radix_tree 的键
#define USE_STRING

#include "Common.h"
#include <unistd.h>
#ifdef USE_RADIX_TREE
#include "radix_tree.hpp"
#endif
#ifdef __linux__
#include <sys/mman.h>
#endif

//对于Page Cache也要设置为单例，对于Central Cache获取span的时候
//每次都是从同一个page数组中获取span
//单例模式
class PageCache
{
public:
	static PageCache* GetInstence()
	{
		return &_inst;
	}

	Span* AllocBigPageObj(size_t size);
	void FreeBigPageObj(void* ptr, Span* span);

	Span* _NewSpan(size_t n);
	Span* NewSpan(size_t n);//获取的是以页为单位

	//获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	//释放空间span回到PageCache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

	//析构函数
	~PageCache();
private:
	SpanList _spanlist[NPAGES];
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
	radix_tree<std::string,Span*> _idspanmap;
	#else
	radix_tree<std::vector<uint8_t>,Span*> _idspanmap;
	#endif
#else
	std::unordered_map<PageID, Span*> _idspanmap;
#endif
	std::mutex _mutex;
#ifdef _WIN32
	vector<void*> _ptr_record;
#elif __linux__
	void *_origin_brk;
#endif
private:
	PageCache()
	{
#ifdef __linux__
	this->_origin_brk=sbrk(0);
#endif
	}
	PageCache(const PageCache&) = delete;
	static PageCache _inst;
};