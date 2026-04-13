#include "Connection.h"
#include "public.h"
#include "CommonConnectionPool.h"
#include <iostream>
#include <chrono>
using namespace std;

// 不使用连接池的线程任务
void testWithoutPool(int threadId, int operations)
{
	for (int i = 0; i < operations; i++)
	{
		Connection conn;
		if (conn.Connect("localhost", 3306, "test", "root", "fan1shacker."))
		{
			conn.Update("INSERT INTO user (name) VALUES ('test')");
		}
		else
		{
			LOG("Thread " + to_string(threadId) + " failed to connect to database");
		}
	}
}

// 使用连接池的线程任务
void testWithPool(int threadId, int operations)
{
	for (int i = 0; i < operations; i++)
	{
		auto conn = ConnectionPool::GetConnectionPool()->GetConnection();
		if (conn != nullptr)
		{
			conn->Update("INSERT INTO user (name) VALUES ('test')");
		}
		else
		{
			LOG("Thread " + to_string(threadId) + " failed to get connection from pool");
		}
	}
}

int main()
{
	// 测试一千次插入数据的时间，不使用连接池
	/*auto start = chrono::high_resolution_clock::now();
	
	for (int i = 0; i < 10000; i++)
	{
		Connection conn;
		if (conn.Connect("localhost", 3306, "test", "root", "fan1shacker."))
		{
			conn.Update("INSERT INTO member (name) VALUES ('test')");
		}
		else
		{
			LOG("Failed to connect to database");
		}
	}
	
	auto end = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
	
	cout << "操作耗时: " << duration.count() << " ms" << endl;*/

	//测试一千次插入数据的时间，使用连接池
	/*auto start = chrono::high_resolution_clock::now();
	for (int i = 0; i < 10000; i++)
	{
		auto conn = ConnectionPool::GetConnectionPool()->GetConnection();
		if (conn != nullptr)
		{
			conn->Update("INSERT INTO member (name) VALUES ('test')");
		}
		else
		{
			LOG("Failed to get connection from pool");
		}
	}
	
	auto end = chrono::high_resolution_clock::now();
	auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
	
	cout << "操作耗时: " << duration.count() << " ms" << endl;*/

	
	const int THREAD_COUNT = 4;
	const int OPERATIONS_PER_THREAD = 2500;  // 每个线程2500次，总共10000次

	vector<thread> threads;

	// ==================== 测试1：不使用连接池 ====================
	cout << "========== 不使用连接池（4线程并发） ==========" << endl;
	auto start1 = chrono::high_resolution_clock::now();

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(testWithoutPool, i, OPERATIONS_PER_THREAD);
	}

	for (auto& t : threads)
	{
		t.join();
	}

	auto end1 = chrono::high_resolution_clock::now();
	auto duration1 = chrono::duration_cast<chrono::milliseconds>(end1 - start1);
	cout << "不使用连接池耗时: " << duration1.count() << " ms" << endl;

	threads.clear();  // 清空线程vector以便复用

	// ==================== 测试2：使用连接池 ====================
	/*cout << "\n========== 使用连接池（4线程并发） ==========" << endl;
	auto start2 = chrono::high_resolution_clock::now();

	for (int i = 0; i < THREAD_COUNT; i++)
	{
		threads.emplace_back(testWithPool, i, OPERATIONS_PER_THREAD);
	}

	for (auto& t : threads)
	{
		t.join();
	}

	auto end2 = chrono::high_resolution_clock::now();
	auto duration2 = chrono::duration_cast<chrono::milliseconds>(end2 - start2);
	cout << "使用连接池耗时: " << duration2.count() << " ms" << endl;*/

	// 计算性能提升
	//cout << "\n========== 性能对比 ==========" << endl;
	//cout << "性能提升: " << (double)duration1.count() / duration2.count() << " 倍" << endl;

	
	return 0;
}