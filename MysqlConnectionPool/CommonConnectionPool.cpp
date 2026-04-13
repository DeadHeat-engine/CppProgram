#include "CommonConnectionPool.h"
#include "public.h"

//线程安全的懒汉式单例模式实现获取连接池对象的静态函数
ConnectionPool* ConnectionPool::GetConnectionPool()
{
	static ConnectionPool pool;
	return &pool;
}

//连接池构造函数的实现
ConnectionPool::ConnectionPool()
	: _currentSize(0), _initSize(0), _maxSize(0), _maxIdleTime(0), _connectionTimeout(0)
{
	//从配置文件中加载数据库连接池的相关参数,如果加载失败则输出错误日志
	if (!LoadConfigFile())
	{
		LOG("Failed to load configuration file");
		return;
	}

	//根据加载的配置参数创建初始数量的数据库连接对象，并将它们放入连接池的队列中
	for (int i = 0; i < m_minConnections; i++)
	{
		std::unique_ptr<Connection> conn = std::make_unique<Connection>();
		if (conn->Connect(m_host, m_port, m_database, m_user, m_password))
		{
			_connectionQueue.push(std::move(conn));
			_connectionQueue.back()->resetAliveTime();

			_currentSize++;
		}
		else
		{
			LOG("Failed to create initial database connection");
			// conn会自动释放
		}
	}

	//启动独立的线程，作为生产者生产新的连接 
	std::thread producerThread(&ConnectionPool::ProduceConnection, this);
	producerThread.detach();

	//启动一个新的线程，扫描超过maxIdleTime的连接并将其销毁
	std::thread cleanerThread([this]() {
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(60)); // 每60秒检查一次
			std::unique_lock<std::mutex> lock(_queueMutex);
			size_t initialSize = _connectionQueue.size();
			for (size_t i = 0; i < initialSize; ++i)
			{
				std::unique_ptr<Connection> conn = std::move(_connectionQueue.front());
				_connectionQueue.pop();
				if (conn->getAliveTime() > _maxIdleTime * 1000 && _currentSize > m_minConnections) // 超过最大空闲时间且连接数大于初始值
				{
					LOG("销毁一个超过最大空闲时间的连接");
					_currentSize--;
					// conn会自动释放
				}
				else
				{
					_connectionQueue.push(std::move(conn)); // 连接仍然有效或需要保留，放回队列
				}
			}
		}
		});
	cleanerThread.detach();
}


//从配置文件中加载数据库连接池的相关参数的实现
bool ConnectionPool::LoadConfigFile()
{
	FILE* configFile = fopen("mysql.ini", "r");
	if (configFile == nullptr)
	{
		LOG("无法打开配置文件 mysql.ini");
		return false;
	}

	char line[256];
	bool hasRequiredFields = false;

	while (fgets(line, sizeof(line), configFile) != nullptr)
	{
		// 移除行尾的换行符
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
		{
			line[len - 1] = '\0';
		}

		std::string lineStr(line);

		// 跳过空行和注释行
		if (lineStr.empty() || lineStr[0] == ';' || lineStr[0] == '#')
			continue;

		// 解析键值对 (格式: key=value)
		size_t delimiterPos = lineStr.find('=');
		if (delimiterPos == std::string::npos)
			continue;

		std::string key = lineStr.substr(0, delimiterPos);
		std::string value = lineStr.substr(delimiterPos + 1);

		// 去除前后空格
		key.erase(0, key.find_first_not_of(" \t"));
		key.erase(key.find_last_not_of(" \t") + 1);
		value.erase(0, value.find_first_not_of(" \t"));
		value.erase(value.find_last_not_of(" \t") + 1);

		// 根据键名设置配置参数
		if (key == "ip")
		{
			m_host = value;
			LOG("host = " + value);
			hasRequiredFields = true;
		}
		else if (key == "port")
		{
			m_port = std::stoi(value);
			LOG("port = " + value);
		}
		else if (key == "username")
		{
			m_user = value;
			LOG("user = " + value);
		}
		else if (key == "password")
		{
			m_password = value;
			LOG("password 已设置");
		}
		else if (key == "database")
		{
			m_database = value;
			LOG("database = " + value);
		}
		else if (key == "initialSize")
		{
			m_minConnections = std::stoi(value);
			LOG("minConnections = " + value);
		}
		else if (key == "maxActive")
		{
			m_maxConnections = std::stoi(value);
			LOG("maxConnections = " + value);
		}
		else if (key == "maxIdleTime")
		{
			_maxIdleTime = std::stoi(value);
			LOG("maxIdleTime = " + value);
		}
		else if (key == "connectionTimeout")
		{
			_connectionTimeout = std::stoi(value);
			LOG("connectionTimeout = " + value);
		}
		
	}

	fclose(configFile);

	// 验证必要的配置参数是否已设置
	if (!hasRequiredFields || m_minConnections <= 0 || m_maxConnections <= 0 || m_minConnections > m_maxConnections)
	{
		LOG("配置文件参数不完整或无效");
		return false;
	}

	LOG("配置文件加载完成");
	return true;
}


//生产者，运行在独立的线程中
void ConnectionPool::ProduceConnection()
{
	for (;;)
	{
		std::unique_lock<std::mutex> lock(_queueMutex);
		
		// 等待直到需要生产：队列为空 且 连接数未达到最大值
		_condition.wait(lock, [this]() {
			return _connectionQueue.empty() && _currentSize < m_maxConnections;
		});
		
		// 连接池中连接数量未达到最大值，创建新的连接对象并放入队列
		std::unique_ptr<Connection> conn = std::make_unique<Connection>();
		if (conn->Connect(m_host, m_port, m_database, m_user, m_password))
		{
			_connectionQueue.push(std::move(conn));
			_connectionQueue.back()->resetAliveTime();
			_currentSize++;
			LOG("生产者线程创建了一个新的数据库连接，当前连接数: " + std::to_string(_currentSize));
			_condition.notify_all(); // 通知消费者有新连接可用
		}
		else
		{
			LOG("生产者线程创建数据库连接失败");
			lock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 失败后等待后重试
			lock.lock();
		}
	}
}


//消费者，获取连接池中一个可用的连接对象，返回连接对象的智能指针的实现
std::shared_ptr<Connection> ConnectionPool::GetConnection()
{
	std::unique_lock<std::mutex> lock(_queueMutex);
	while (_connectionQueue.empty())
	{
		// 连接池中没有可用连接，消费者线程等待
		LOG("连接池中没有可用连接，消费者线程等待");
		if (std::cv_status::timeout == _condition.wait_for(lock, std::chrono::milliseconds(_connectionTimeout)))
		{
			LOG("获取连接超时");
			return nullptr;
		}
	}
	
	// 从连接池队列中获取一个可用的连接对象
	std::unique_ptr<Connection> conn = std::move(_connectionQueue.front());
	_connectionQueue.pop();
	LOG("消费者线程获取了一个数据库连接，当前可用连接数: " + std::to_string(_connectionQueue.size()));
	
	// 将获取到的连接对象封装成智能指针返回给消费者线程
	// 自定义删除器：连接使用完毕后自动归还到连接池
	return std::shared_ptr<Connection>(conn.release(), [this](Connection* connPtr) {
		std::unique_lock<std::mutex> lock(_queueMutex);
		if (connPtr != nullptr)
		{
			connPtr->resetAliveTime();
			_connectionQueue.push(std::unique_ptr<Connection>(connPtr));
			LOG("连接已归还到连接池，当前可用连接数: " + std::to_string(_connectionQueue.size()));
			_condition.notify_all();
		}
	});
}