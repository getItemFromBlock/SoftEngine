#include "Conversion.hpp"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#define NOMINMAX
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

namespace Utils::Serialization::Conversion
{
	void ToNetwork(uint16_t from, uint16_t& to)
	{
		to = htons(from);
	}
	void ToNetwork(uint32_t from, uint32_t& to)
	{
		to = htonl(from);
	}
	void ToNetwork(uint64_t from, uint64_t& to)
	{
#ifdef _WIN32
		to = htonll(from);
#else
		to = htobe64(from);
#endif
	}
	void ToNetwork(float from, uint32_t& to)
	{
#ifdef _WIN32
		to = htonf(from);
#else
    	union {
        	uint32_t l;
        	float d;
    	} tmp;
    	tmp.d = from;
    	ToNetwork(tmp.l, to);
#endif
	}
	void ToNetwork(double from, uint64_t& to)
	{
#ifdef _WIN32
		to = htond(from);
#else
    	union {
        	uint64_t l;
        	double d;
    	} tmp;
    	tmp.d = from;
    	ToNetwork(tmp.l, to);
#endif
	}

	void ToLocal(uint16_t from, uint16_t& to)
	{
		to = ntohs(from);
	}
	void ToLocal(uint32_t from, uint32_t& to)
	{
		to = ntohl(from);
	}
	void ToLocal(uint64_t from, uint64_t& to)
	{
#ifdef _WIN32
		to = ntohll(from);
#else
		to = be64toh(from);
#endif
	}
	void ToLocal(uint32_t from, float& to)
	{
#ifdef _WIN32
		to = ntohf(from);
#else
    	union {
        	uint32_t l;
        	float d;
    	} tmp;
    	ToLocal(from, tmp.l);
    	to = tmp.d;
#endif
	}
	void ToLocal(uint64_t from, double& to)
	{
#ifdef _WIN32
		to = ntohd(from);
#else
    	union {
        	uint64_t l;
        	double d;
    	} tmp;
    	ToLocal(from, tmp.l);
    	to = tmp.d;
#endif
	}
}