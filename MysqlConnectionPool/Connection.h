#pragma once
/*
	定义数据库连接类，封装连接数据库、执行SQL语句等功能
*/

#include<string>
#include <mysql.h>
#include<ctime>
using namespace std;

class Connection
{
public:
	Connection();
	~Connection();

	//定义连接数据库的函数
	bool Connect(const string& host, int port, const string& database, const string& user, const string& password);
	
	//定义更新操作
	bool Update(const string& sql);
	//定义查询操作，返回查询结果集
	MYSQL_RES* Query(const string& sql);

	void resetAliveTime() { _aliveTime = clock(); }
	clock_t getAliveTime() const { return clock() - _aliveTime; }
	
private:
	//表示和数据库连接的成员变量
	MYSQL* _conn;
	//记录进入空闲状态的起始时间
	int _aliveTime;
};
