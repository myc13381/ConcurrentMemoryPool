#include "Common.h"
#include "ConcurrentAlloc.h"

// #define SHOWTIME

#define SHOWMESSAGE do {\
	printf("%lu threads concurrent execute %lu rounds, concurrent alloc %lu times per round: cost:%lu ms\n", nworks, rounds, ntimes, malloc_costtime);\
	printf("%lu threads %lu rounds, concurrent dealloc %lu times per round : cost: %lu ms\n",nworks, rounds, ntimes, free_costtime);\
	printf("%lu threads concurrent alloc&dealloc %lu times, cost total: %lu ms\n",nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);\
} while(0)

static std::pair<size_t, size_t> BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds,size_t mem_size)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				
				for (size_t i = 0; i < ntimes; i++)
				{
					size_t begin1 = clock();
					void *ptr=malloc(mem_size);
					size_t end1 = clock();
					malloc_costtime+=end1-begin1;
					v.push_back(ptr);
				}
				
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				free_costtime += end2 - begin2;
			}
		});
	}
	for (size_t k = 0; k < nworks; ++k) vthread[k].join();

#ifdef SHOWTIME
	SHOWMESSAGE;
#endif // SHOWTIME

	return std::make_pair(malloc_costtime, free_costtime);
}

// 单轮次申请释放次数 线程数 轮次
static std::pair<size_t,size_t> BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds, size_t mem_size)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	
	void *ptr=ConcurrentAlloc(512*1024);
	ConcurrentFree(ptr);
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				for (size_t i = 0; i < ntimes; i++)
				{
					size_t begin1 = clock();
					void *ptr=ConcurrentAlloc(mem_size);
					size_t end1 = clock();
					malloc_costtime += end1 - begin1;
					v.push_back(ptr);
				}
				
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				free_costtime += end2 - begin2;
			}
		});
	}

	for (size_t k = 0; k < nworks; ++k) vthread[k].join();

#ifdef SHOWTIME
	SHOWMESSAGE;
#endif // SHOWTIME

	return std::make_pair(malloc_costtime, free_costtime);
}


static void test(int ntimes,int nthreads,int rounds,int mem_size)
{
	//cout << "==========================================================" << endl;
	cout << "memory size " << mem_size << "Byte" << endl;
	std::pair<size_t, size_t> pc = BenchmarkConcurrentMalloc(ntimes, nthreads, rounds, mem_size);
	std::pair<size_t, size_t> pm = BenchmarkMalloc(ntimes, nthreads, rounds, mem_size);
	cout << "bench rate concurrentalloc/malloc=" << (double)(pm.first +  0.000000000000000001) / (double)(pc.first +  0.000000000000000001) << endl;//加1防止出现除以0
	cout << "bench rate concurrentdealloc/free=" << (double)(pm.second + 0.000000000000000001) / (double)(pc.second + 0.000000000000000001) << endl;
	cout << "total bench rate =" << (double)(pm.first + pm.second + 0.000000000000000001) / (double)(pc.first + pc.second + 0.000000000000000001) << endl;
	//cout << "==========================================================" << endl;
	cout << endl;
}

int main()
{
#ifdef _WIN32
	system("cls");
#else
	system("clear");
#endif
	int ntimes = 10;//每个线程申请的次数
	int nthreads = 10;//线程的个数
	int rounds = 5;//执行的轮次
	int mem_size = 16 * 1024;
	size_t arr[] = {1024*1024,1024*512,1024*128,1024*64,1024*32,1024,512,128,64,16,4};
	//test(ntimes, nthreads, rounds, 1024*1024);
	for (size_t val : arr)
	{
		test(ntimes, nthreads, rounds, val);
	}
	return 0;
}