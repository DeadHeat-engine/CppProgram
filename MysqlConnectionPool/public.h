#pragma once
#include<iostream>

#define LOG(str)  \
	do { \
		cout << "[" << __FILE__ << ":" << __LINE__ << ":" << __TIMESTAMP__  "] " << str << endl; \
	} while (0)