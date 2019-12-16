#include "LibeventNet.h"


class MyServer
{
public:
	void Start()
	{
		m_libs = new LibeventNet * [5];
		for (int i = 0; i < 5; i++)
		{
			m_libs[i] = new LibeventNet;
			m_libs[i]->Start();
		}
	}
private:
	LibeventNet** m_libs;
};

int main()
{
	MyServer server;
	server.Start();
	while (1)
	{
		Sleep(100);
	}
	return 0;
}