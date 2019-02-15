#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>

#include "transports/udptransport.h"
#include "universe.h"
#include "config.h"

namespace exampi
{

UDPTransport::UDPTransport() : header_pool(32)
{
	// create
	socket_recv = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_recv == 0)
	{
		debug("ERROR: socket creation failed: " << socket_recv);

		throw UDPTransportCreationException();
	}

	// setsockopt to reuse address
	//int opt = 1;
	//if(setsockopt(server_recv, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))

	// bind
	Universe &universe = Universe::get_root_universe();

	// garuntees no local collision
	int port = std::stoi(std::string(std::getenv("EXAMPI_UDP_TRANSPORT_BASE"))) +
	           universe.rank;
	debug("udp transport port: " << port);

	struct sockaddr_in address_local;
	address_local.sin_family = AF_INET;
	address_local.sin_addr.s_addr = INADDR_ANY;
	address_local.sin_port = htons(port);

	if(bind(socket_recv, (sockaddr *)&address_local, sizeof(address_local)) < 0)
	{
		debug("ERROR: socket binding failed");

		throw UDPTransportBindingException();
	}

	// prepare msg header
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;

	hdr.msg_control = NULL;
	hdr.msg_controllen = 0;
	hdr.msg_flags = 0;

	// cache remote addresses
	Config &config = Config::get_instance();

	for(long int rank = 0; rank < universe.world_size; ++rank)
	{
		std::string descriptor = config[std::to_string(rank)];
		size_t delimiter = descriptor.find_first_of(":");
		std::string ip = descriptor.substr(0, delimiter);
		int port = std::stoi(descriptor.substr(delimiter+1));

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ip.c_str());
		addr.sin_port = htons(port);

		cache.insert({rank, addr});
	}

	// setting available protocols for UDPTransport
	// todo still subtract sizeof(protocol+envelope)
	//      65507 maximum udp packet size
	//available_protocols[Protocol::EAGER] = 65507;
	//available_protocols[Protocol::EAGER_ACK] = 65507;

	available_protocols[Protocol::EAGER] = 128;
	available_protocols[Protocol::EAGER_ACK] = 128;
}

UDPTransport::~UDPTransport()
{
	close(socket_recv);
}

Header *UDPTransport::ordered_recv()
{
	// early exit test
	char test;
	ssize_t size = recv(socket_recv, &test, sizeof(test), MSG_PEEK | MSG_DONTWAIT);
	if(size <= 0)
	{
		return nullptr;
	}

	// commit to do receive operation
	std::lock_guard<std::recursive_mutex> lock(guard);

	// check again, that the data has not been taken by another thread
	debug("double checking work availability");

	size = recv(socket_recv, &test, sizeof(test), MSG_PEEK | MSG_DONTWAIT);
	if(size <= 0)
	{
		debug("receive was done before the thread got here");
		return nullptr;
	}

	debug("receive from underlying socket available");
	Header *header = header_pool.allocate();

	// TODO obviously don't do this, this is temporary
	void *data = malloc(sizeof(int));

	iovec iovs[2];
	iovs[0].iov_base = header;
	iovs[0].iov_len = sizeof(Header);

	// todo this is where we would need to either have a fixed size message
	//      or read it in after reading the header

	iovs[1].iov_base = data;
	iovs[1].iov_len = sizeof(int);

	hdr.msg_iov = iovs;
	hdr.msg_iovlen = 2;
	hdr.msg_name = NULL;
	hdr.msg_namelen = 0;

	debug("receiving msg from socket");
	int err = recvmsg(socket_recv, &hdr, 0);
	if(err <= 0)
	{
		header_pool.deallocate(header);
		free(data);
		
		debug("failed receive");
	
		return nullptr;
	}
	else
	{
		debug("header: e " << header->envelope.epoch << " c " << header->envelope.context << " s " << header->envelope.source << " d " << header->envelope.destination << " t " << header->envelope.tag);

		data_buffer[(const Header*)header] = data;
		debug("data received " << *((int*)data));

		return header;
	}
}

int UDPTransport::fill(const Header *header, Request *request)
{
	// look up payload with respect to header
	//return -1;

	void *data = data_buffer[header];
	//size int

	std::memcpy((void*)request->payload.buffer, data, sizeof(int));

	// TODO replace with throw
	return MPI_SUCCESS;
}

//int UDPTransport::reliable_send(ProtocolMessage_uptr message)
int UDPTransport::reliable_send(const Protocol protocol, const Request *request)
{
	std::lock_guard<std::recursive_mutex> lock(guard);

	// todo depends on datatype
	//      we currently only support vector/block
	iovec iovs[3];

	// protocol
	iovs[0].iov_base = (void*)&protocol;
	iovs[0].iov_len = sizeof(Protocol);

	// request->envelope
	iovs[1].iov_base = (void*)&request->envelope;
	iovs[1].iov_len = sizeof(Envelope);

	debug("envelope to send: e " << request->envelope.epoch << " c " << request->envelope.context << " s " << request->envelope.source << " d " << request->envelope.destination << " t " << request->envelope.tag);

	// request->buffer;
	// todo assume no packing due to datatype, single int
	iovs[2].iov_base = (void*)request->payload.buffer;
	iovs[2].iov_len = sizeof(int) * request->payload.count;

	// rank -> root commmunicator -> address
	sockaddr_in &addr = cache[request->envelope.destination];

	hdr.msg_iov = iovs;
	hdr.msg_iovlen = 3;
	hdr.msg_name = &addr;
	hdr.msg_namelen = sizeof(addr);

	int err = sendmsg(socket_recv, &hdr, 0);
	if(err <= 0)
	{
		// TODO convert to throw
		return MPI_ERR_RELIABLE_SEND_FAILED;
	}
	else
	{
		// TODO convert to throw
		debug("sent " << err << " bytes");
		return MPI_SUCCESS;
	}
}

const std::map<Protocol, size_t> &UDPTransport::provided_protocols() const
{
	return available_protocols;
}

int UDPTransport::save(std::ostream &r)
{
	return MPI_SUCCESS;
}

int UDPTransport::load(std::istream &r)
{
	return MPI_SUCCESS;
}

int UDPTransport::halt()
{
	// TODO no idea what this does, from original udp transport
//	//std::cout << debug() << "basic::Transport::receive(...)" << std::endl;
//	char buffer[2];
//	struct sockaddr_storage src_addr;
//
//	struct iovec iov[1];
//	iov[0].iov_base=buffer;
//	iov[0].iov_len=sizeof(buffer);
//
//	struct msghdr message;
//	message.msg_name=&src_addr;
//	message.msg_namelen=sizeof(src_addr);
//	message.msg_iov=iov;
//	message.msg_iovlen=1;
//	message.msg_control=0;
//	message.msg_controllen=0;
//
//
//	//std::cout << debug() <<
//	//          "basic::Transport::receive, constructed msg, calling msg.receive" << std::endl;
//
//	//std::cout << debug() << "basic::Transport::udp::recv\n";
//
//	recvmsg(recvSocket.getFd(), &message, MSG_WAITALL);
//	//std::cout << debug() << "basic::Transport::udp::recv exiting\n";
//	//std::cout << debug() << "basic::Transport::receive returning" << std::endl;
	return MPI_SUCCESS;
}

}
