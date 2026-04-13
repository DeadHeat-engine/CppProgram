#pragma once
#include<string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "Connection.h"
#include<queue>
#include<mutex>
#include<memory>
#include <condition_variable>
#include <thread>

/*
	实现连接池的类，连接池中维护多个数据库连接对象，提供获取和释放连接的接口
*/
class ConnectionPool
{
public:
	//线程安全的懒汉式单例模式，给外部提供获取连接池对象的接口
	static ConnectionPool* GetConnectionPool();
	//消费者，获取连接池中一个可用的连接对象，返回连接对象的智能指针
	std::shared_ptr<Connection> GetConnection();

private:
	//单例模式，构造函数私有化，禁止外部创建连接池对象
	ConnectionPool();

	//从配置文件中加载数据库连接池的相关参数
	bool LoadConfigFile();

	//生产者，创建新的连接对象并将其放入连接池的队列中
	void ProduceConnection();

	//定义连接池的成员变量，保存数据库连接池的相关参数
	std::string m_host;
	int m_port;
	std::string m_user;
	std::string m_password;
	std::string m_database;
	int m_minConnections;
	int m_maxConnections;

	int _initSize; //连接池初始大小
	int _maxSize; //连接池最大大小
	int _maxIdleTime; //连接池中连接的最大空闲时间
	int _connectionTimeout; //连接池获取连接的超时时间

	// 使用 unique_ptr 管理连接
	queue<std::unique_ptr<Connection>> _connectionQueue; //连接池中连接对象的队列
	mutex _queueMutex; //保护连接队列的互斥锁
	condition_variable _condition;
	int _currentSize;  //连接池中当前连接的数量
};


