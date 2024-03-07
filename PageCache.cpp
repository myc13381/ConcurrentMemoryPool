#include "PageCache.h"

constexpr int SIZEOF_PAGE_ID = sizeof(PageID);

// help function
// 将 PageID 类型按照每4位保存在一个uint8_t类型中，组成一个数组
// 例如 0x000000000000000F == 0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,0000,1111 --> {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15}
static std::vector<uint8_t> PageId2Arr(const PageID id)
{
	std::vector<uint8_t> ret(SIZEOF_PAGE_ID * 2, 0);
	PageID mask = static_cast<PageID>(0xFF) << ((SIZEOF_PAGE_ID - 1) * 8);
	for(int i=0;i<SIZEOF_PAGE_ID;++i)
	{
		uint8_t tempA = static_cast<uint8_t>((id & mask) >> (8 * (SIZEOF_PAGE_ID - i -1))); // 每次截取8位
		uint8_t tempB = (tempA & 0xF0) >> 4; // 取 tempA 前4位
		uint8_t tempC = tempA & 0x0F; // 取 tempA 后4位
		ret[i*2] = tempB;
		ret[i*2 + 1] = tempC;
		mask >>= 8;
	}
	return ret;
}

PageCache PageCache::_inst;


//大对象申请，直接从系统
Span* PageCache::AllocBigPageObj(size_t size)
{
	assert(size > MAX_BYTES);//只有申请64K以上内存时才需要调用此函数

	size = SizeClass::_Roundup(size, PAGE_SHIFT); //对齐
	size_t npage = size >> PAGE_SHIFT;
	if (npage < NPAGES)
	{
		Span* span = NewSpan(npage);
		span->_objsize = size;
		span->_usecount = 1;
		return span;
	}
	else//超过128页，向系统申请
	{
#ifdef _WIN32
		void* ptr = VirtualAlloc(0, npage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else 
		void* ptr = mmap(nullptr,npage << PAGE_SHIFT,PROT_WRITE | PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS,0,0);
#endif
		if (ptr == nullptr)
			throw std::bad_alloc();

		Span* span = new Span;
		span->_npage = npage;
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_objsize = npage << PAGE_SHIFT;
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
		_idspanmap[std::to_string(span->_pageid)] = span;
	#else
		_idspanmap[PageId2Arr(span->_pageid)] = span;
	#endif
#else
		_idspanmap[span->_pageid] = span;
#endif
		return span;
	}
}

void PageCache::FreeBigPageObj(void* ptr, Span* span)
{
	size_t npage = span->_objsize >> PAGE_SHIFT;
	if (npage < NPAGES) //相当于还是小于128页
	{
		ReleaseSpanToPageCache(span);
	}
	else
	{
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
		_idspanmap.erase(_idspanmap.find(std::to_string(npage)));
	#else
		_idspanmap.erase(_idspanmap.find(PageId2Arr(npage)));
	#endif
#else
		_idspanmap.erase(npage);
#endif
		//void* ptr = (void*)(span->_pageid << PAGE_SHIFT);//是否可以这样做然后少传递一个参数
		delete span;
#ifdef _WIN32
		VirtualFree(ptr, 0, MEM_RELEASE);
		this->_ptr_record.emplace_back(ptr);
#elif __linux__
		munmap(ptr,npage<<12);
#endif
	}
}

Span* PageCache::NewSpan(size_t n)
{
	// 加锁，防止多个线程同时到PageCache中申请span
	// 这里必须是给全局加锁，不能单独的给每个桶加锁
	// 如果对应桶没有span,是需要向系统申请的
	// 可能存在多个线程同时向系统申请内存的可能
	std::unique_lock<std::mutex> lock(_mutex);
	return _NewSpan(n);
}



Span* PageCache::_NewSpan(size_t n)
{
	assert(n < NPAGES);
	if (!_spanlist[n].Empty())
	{
		Span* span =_spanlist[n].PopFront();
		span->_usecount = 1;
		return span;
	}
		

	for (size_t i = n + 1; i < NPAGES; ++i)
	{
		if (!_spanlist[i].Empty())
		{
			//大内存对象拆分
			Span* span = _spanlist[i].PopFront();
			Span* splist = new Span;

			splist->_pageid = span->_pageid;
			splist->_npage = n;
			splist->_objsize = splist->_npage << PAGE_SHIFT;
			splist->_usecount = 1;//一次使用

			span->_pageid = span->_pageid + n;
			span->_npage = span->_npage - n;
			span->_objsize = span->_npage << PAGE_SHIFT;


			for (size_t j = 0; j < n; ++j)
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
				_idspanmap[std::to_string(splist->_pageid + j)] = splist;
	#else
				_idspanmap[PageId2Arr(splist->_pageid + j)] = splist;
	#endif
#else
				_idspanmap[splist->_pageid + j] = splist;
#endif
			_spanlist[span->_npage].PushFront(span);
			return splist;
		}
	}

	Span* span = new Span;

	// 到这里说明SpanList中没有合适的span,只能向系统申请128页的内存
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, (NPAGES - 1)*(1 << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	this->_ptr_record.push_back(ptr);
#elif __linux__
	void *ptr = sbrk((NPAGES - 1)*(1 << PAGE_SHIFT));
#endif


	span->_pageid = (PageID)ptr >> PAGE_SHIFT;
	span->_npage = NPAGES - 1;

	for (size_t i = 0; i < span->_npage; ++i)
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
		_idspanmap[std::to_string(span->_pageid + i)] = span;
	#else
		_idspanmap[PageId2Arr(span->_pageid + i)] = span;
	#endif
#else
		_idspanmap[span->_pageid + i] = span;
#endif

	_spanlist[span->_npage].PushFront(span);  //Span->_next  Span->_prev 
	return _NewSpan(n);
}

// 获取从对象到span的映射
Span* PageCache::MapObjectToSpan(void* obj)
{
	//计算页号
	PageID id = (PageID)obj >> PAGE_SHIFT;

#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
	auto it = _idspanmap.find(std::to_string(id));
	#else
	auto it = _idspanmap.find(PageId2Arr(id));
	#endif
#else
	auto it = _idspanmap.find(id);
#endif

	if (it != _idspanmap.end())
	{
		return it->second;
	}
	else
	{
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* cur)
{
	// 必须上全局锁,可能多个线程一起从ThreadCache中归还数据
	std::unique_lock<std::mutex> lock(_mutex);
	cur->_objsize = 0;
	cur->_usecount = 0;

	// 向前合并
	while (1)
	{
		PageID curid = cur->_pageid;
		PageID previd = curid - 1;

#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
		auto it = _idspanmap.find(std::to_string(previd));
	#else
		auto it = _idspanmap.find(PageId2Arr(previd));
	#endif
#else
		auto it = _idspanmap.find(previd);
#endif

		// 没有找到
		if (it == _idspanmap.end())
			break;

		// 前一个span不空闲
		if (it->second->_usecount != 0)
			break;

		Span* prev = it->second;

		//超过128页则不合并
		if (cur->_npage + prev->_npage > NPAGES - 1)
			break;

		// 先把prev从链表中移除
		_spanlist[prev->_npage].Erase(prev);

		// 合并
		prev->_npage += cur->_npage;
		//修正id->span的映射关系
		for (PageID i = 0; i < cur->_npage; ++i)
		{
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
			_idspanmap[std::to_string(cur->_pageid + 1)] = prev;
	#else
			_idspanmap[PageId2Arr(cur->_pageid + i)] = prev;
	#endif
#else
			_idspanmap[cur->_pageid + i] = prev;
#endif
		}
		delete cur;

		// 继续向前合并
		cur = prev;
	}


	//向后合并
	while (1)
	{
		////超过128页则不合并

		PageID curid = cur->_pageid;
		PageID nextid = curid + cur->_npage;

#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
		auto it = _idspanmap.find(std::to_string(nextid));
	#else
		auto it = _idspanmap.find(PageId2Arr(nextid));
	#endif
#else
		auto it = _idspanmap.find(nextid);
#endif

		if (it == _idspanmap.end())
			break;

		if (it->second->_usecount != 0)
			break;

		Span* next = it->second;

		//超过128页则不合并
		if (cur->_npage + next->_npage >= NPAGES - 1)
			break;

		_spanlist[next->_npage].Erase(next);


		cur->_npage += next->_npage;
		//修正id->Span的映射关系
		for (PageID i = 0; i < next->_npage; ++i)
		{
#ifdef USE_RADIX_TREE
	#ifdef USE_STRING
			_idspanmap[std::to_string(next->_pageid + i)] = cur;
	#else
			_idspanmap[PageId2Arr(next->_pageid + i)] = cur;
	#endif
#else
			_idspanmap[next->_pageid + i] = cur;
#endif
		}

		delete next;
	}

	// 最后将合并好的span插入到span链中
	_spanlist[cur->_npage].PushFront(cur);
}

	//析构函数
	PageCache::~PageCache()
	{
#ifdef _WIN32
		for(void *ptr:this->_ptr_record)
		{
			VirtualFree(ptr, 0, MEM_RELEASE);
		}
#elif __linux__
		brk(this->_origin_brk);
#endif
	}