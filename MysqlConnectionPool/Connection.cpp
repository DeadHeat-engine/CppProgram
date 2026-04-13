#include "Connection.h"
#include "public.h"

Connection::Connection()
{
    _conn = mysql_init(nullptr);
}

Connection::~Connection()
{
    if (_conn)
    {
        mysql_close(_conn);
        _conn = nullptr;
    }
}

//数据库连接函数的实现
bool Connection::Connect(const string& host, int port, const string& database, const string& user, const string& password)
{
    //判断空间是否分配成功
    if (!_conn)
    {
        LOG("Failed to init mysql connection object");
        return false;
    }

    //连接数据库，失败返回false
    if (!mysql_real_connect(_conn, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0))
    {
        LOG("Failed to connect database: " + string(mysql_error(_conn)));
        mysql_close(_conn);
        _conn = nullptr;
        return false;
    }

    //成功连接数据库，返回true
    return true;
}

//数据库更新操作的实现
bool Connection::Update(const string& sql)
{
    if (!_conn)
    {
        LOG("Database connection is not initialized");
        return false;
    }
    if (mysql_query(_conn, sql.c_str()) != 0)
    {
        LOG("Failed to execute update query: " + string(mysql_error(_conn)));
        return false;
    }
    return true;
}

//数据库查询操作的实现
MYSQL_RES* Connection::Query(const string& sql)
{
    if (!_conn)
    {
        LOG("Database connection is not initialized");
        return nullptr;
    }
    if (mysql_query(_conn, sql.c_str()) != 0)
    {
        LOG("Failed to execute query: " + string(mysql_error(_conn)));
        return nullptr;
    }
    return mysql_store_result(_conn);
}