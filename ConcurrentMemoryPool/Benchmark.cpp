#include "common.h"
#include "ConcurrentAlloc.h"

//#define SHOWTIME

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
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(mem_size));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
		vthread[k].join();
	}

#ifdef SHOWTIME
	printf("%u个线程并发执行%u轮次，每轮次malloc %u次: 花费：%u ms\n", nworks, rounds, ntimes, malloc_costtime);
	printf("%u个线程并发执行%u轮次，每轮次free %u次: 花费：%u ms\n", nworks, rounds, ntimes, free_costtime);
	printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n", nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
#endif // SHOWTIME

	return std::make_pair(malloc_costtime, free_costtime);
}

// 单轮次申请释放次数 线程数 轮次
static std::pair<size_t,size_t> BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds, size_t mem_size)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(mem_size));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
		vthread[k].join();
	}
#ifdef SHOWTIME
	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n", nworks, rounds, ntimes, malloc_costtime);
	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",nworks, rounds, ntimes, free_costtime);
	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
#endif // SHOWTIME

	return std::make_pair(malloc_costtime, free_costtime);
}


static void test(int ntimes,int nthreads,int rounds,int mem_size)
{
	//cout << "==========================================================" << endl;
	cout << "每次申请内存大小：" << mem_size << "Byte" << endl;
	std::pair<size_t, size_t> pc = BenchmarkConcurrentMalloc(ntimes, nthreads, rounds, mem_size);
	std::pair<size_t, size_t> pm = BenchmarkMalloc(ntimes, nthreads, rounds, mem_size);
	cout << "性能比concurrentalloc/malloc=" << (double)(pm.first + 0.1) / (double)(pc.first + 0.1) << endl;//加1防止出现除以0
	cout << "性能比concurrentdealloc/free=" << (double)(pm.second + 0.1) / (double)(pc.second + 0.1) << endl;
	cout << "总性能比=" << (double)(pm.first + pm.second + 0.1) / (double)(pc.first + pc.second + 0.1) << endl;
	//cout << "==========================================================" << endl;
	cout << endl;
}

int main()
{
	int ntimes = 10;//每个线程申请的次数
	int nthreads = 10;//线程的个数
	int rounds = 100;//执行的轮次
	int mem_size = 16 * 1024;
	size_t arr[] = { 4,16,64,128,512,1024,1024 * 4,1024*8,1024 * 16,1024 * 64,1024 * 128,1024 * 521,1024 * 1024 };
	for (size_t val : arr)
	{
		test(ntimes, nthreads, rounds, val);
	}
	system("pause");
	return 0;
}