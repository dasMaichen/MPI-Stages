#ifndef __EXAMPI_DAEMON_H
#define __EXAMPI_DAEMON_H

#include <string>
#include <netinet/in.h>

namespace exampi
{

class Daemon
{
public:
	static Daemon& get_instance();

	Daemon(Daemon&)					= delete;
	void operator=(Daemon const&)	= delete;

	int send_barrier_ready();
	int recv_barrier_release();
	int send_clean_up();

private:
	int sock;
	sockaddr_in daemon;
	sockaddr_in local;

	Daemon();
	
	~Daemon();

	int send(std::string);
	std::string recv();
};

} // ::exampi

#endif // guard
