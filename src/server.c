#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <netdb.h>

#include <errno.h>

#ifdef MSVER
typedef SOCKET sock_t;
#else
typedef int sock_t;
#define INVALID_SOCKET -1
#endif


void sock_init()
{
}

void sock_uninit()
{
}

//int sock_create(int domain, int type)
//{
//	int s = socket(domain, type, 0);
//	return s;
//}
//
//// macro?
//int sock_create_tcp()
//{
//	// IPPROTO_TCP
//	return sock_create(AF_INET, SOCK_STREAM);
//}
//
//int sock_create_udp()
//{
//	return sock_create(AF_INET, SOCK_DGRAM);
//}
//
//int sock_create_tcp_ipv6()
//{
//	return sock_create(AF_INET6, SOCK_STREAM);
//}

int sock_close(sock_t s)
{
	return close(s);
}

int sock_nblock(sock_t s)
{
	int fl = fcntl(s, F_GETFL);
	if (fl & O_NONBLOCK) return 0;

	return fcntl(s, F_SETFL, fl | O_NONBLOCK);
}

int sock_block(sock_t s)
{
	int fl = fcntl(s, F_GETFL);
	if (!(fl & O_NONBLOCK)) return 0;

	return fcntl(s, F_SETFL, fl & ~O_NONBLOCK);
}

//int sock_connect(int s, const char* addr, int port, int timeout_sec)
//{
//	return 0;
//}

// https://stackoverflow.com/questions/52885074/how-to-add-a-default-port-number-defined-in-program-to-getaddrinfo
static void gai_seterrno(int e)
{
	if (e) switch (e) {
#ifdef _GNU_SOURCE
		case EAI_ADDRFAMILY: 
#endif
		case EAI_FAMILY:
		    errno = EAFNOSUPPORT;
		    break;
		case EAI_AGAIN:
		    errno  = EAGAIN;
		    break;
#ifdef _GNU_SOURCE
		case EAI_NODATA:
#endif
		case EAI_FAIL:
		case EAI_NONAME:
		case EAI_SERVICE:
		    errno = EADDRNOTAVAIL;
		    break;
		case EAI_MEMORY:
		    errno = ENOMEM;
		    break;
		case EAI_SOCKTYPE:
		    errno = ESOCKTNOSUPPORT;
		    break;
		case EAI_SYSTEM:
		    /* errno already set */
		default:
		    errno = EINVAL;
	}
}

// tcp server
sock_t sock_server(const char* host, int port)
{
	if (port <= 0 || 65535 < port) {
		// error
		// port 0 is not allowed for tcp...
		return -EINVAL;
	}

	struct addrinfo hints = {0};
	// bindable
	hints.ai_flags = AI_PASSIVE;
	// don't care IPV4, IPV6
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char service[8] = {0};
	snprintf(service, sizeof(service), "%d", port);

	struct addrinfo* res;
	int rc = getaddrinfo(host, service, &hints, &res);
	// https://stackoverflow.com/questions/52885074/how-to-add-a-default-port-number-defined-in-program-to-getaddrinfo
	// https://stackoverflow.com/questions/15693945/how-to-use-getaddrinfo
	if (rc) {
		gai_seterrno(rc);
		return -errno;
	}

	// https://en.wikipedia.org/wiki/Getaddrinfo
	// #include <netinet/in.h>
	// NI_MAXHOST 1025
	sock_t s = -1;
	for (struct addrinfo* cur=res; cur; cur=cur->ai_next) {
		s = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		//if (s == INVALID_SOCKET) {
		if (s < 0) {
			continue;
		}

		const int optval = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

		int rc = bind(s, cur->ai_addr, cur->ai_addrlen);
		if (!rc) break;

		sock_close(s);
		s = -1;
	}

	freeaddrinfo(res);

	return s;
}

// tcp client
sock_t sock_connect(const char* host, int port, int socktype, int protocol, int timeout_sec)
{
	if (!socktype) {
		// TCP by default
		socktype = SOCK_STREAM;
	}


	if (!host) {
		// error
		return -EINVAL;
	}
	if (port < 0 || 65535 < port) {
		// error
		// port 0 is not allowed for tcp...
		return -EINVAL;
	}

	struct addrinfo hints = {0};
	//hints.ai_flags = 0;
	// don't care IPV4, IPV6
	hints.ai_family = AF_INET6;
	hints.ai_socktype = socktype;
	hints.ai_protocol = protocol;

	char service[8] = {0};
	snprintf(service, sizeof(service), "%d", port);

	struct addrinfo* res;
	int rc = getaddrinfo(host, service, &hints, &res);
	// https://stackoverflow.com/questions/52885074/how-to-add-a-default-port-number-defined-in-program-to-getaddrinfo
	// https://stackoverflow.com/questions/15693945/how-to-use-getaddrinfo
	if (rc) {
		gai_seterrno(rc);
		return -errno;
	}

	sock_t s = -1;

	for (struct addrinfo* cur=res; cur; cur=cur->ai_next) {
		// getnameinfo()
		// WSAAddressToString()
		s = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
		//if (s == INVALID_SOCKET) {
		if (s < 0) {
			continue;
		}

		sock_nblock(s);

		// TODO: EINTR
		int rc = connect(s, cur->ai_addr, cur->ai_addrlen);
		// errno can be EINTR -> connect again
		if (!rc) {
			sock_block(s);
			break;
		}
		if (errno != EINPROGRESS) {
			sock_close(s);
			s = -1;
			continue;
		}

		struct timeval tv = {0};
		tv.tv_sec = timeout_sec;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(s, &fds);

		// TODO: EINTR
		rc = select(s+1, 0, &fds, 0, &tv);
		if (rc<0) {
			// errno may be EINTR -> select again
			// error
			sock_close(s);
			s = -1;
			continue;
		}
		if (!rc) {
			// timeout
			sock_close(s);
			s = -1;
			continue;
		}
		//if (rc>0) {

		int error;
		socklen_t len = sizeof(error);
		if (getsockopt(s, SOL_SOCKET, SO_ERROR, &error, &len)) {
			sock_close(s);
			s = -1;
			continue;
		}

		//if (ret == ECONNREFUSED) ...
		if (error) {
			// error;
			errno = error;
			sock_close(s);
			s = -1;
			continue;
		}
		//}
		sock_block(s);
		break;

	}

	freeaddrinfo(res);

	return s;
}

//int sock_connect_ipv6(int s, const char* addr, int port, int timeout_sec)
//{
//	return 0;
//}

int sock_send(int s, const char* data, size_t sz, int timeout_sec)
{
	return 0;
}

int sock_recv(int s, char* data, size_t sz, int timeout_sec)
{
	return 0;
}



#include <stdlib.h>

//struct addrinfo hints = {0};
////hints.ai_flags = 0;
//hints.ai_family = AF_UNSPEC;
//hints.ai_socktype = SOCK_STREAM;
//hints.ai_protocol = IPPROTO_TCP;
//
//
//   struct addrinfo {
//       int              ai_flags;
//       int              ai_family;
//       int              ai_socktype;
//       int              ai_protocol;
//       socklen_t        ai_addrlen;
//       struct sockaddr *ai_addr;
//       char            *ai_canonname;
//       struct addrinfo *ai_next;
//   };
int main()
{
	sock_init();

	sock_t s = sock_server(0, 7777);
	if (s < 0) {
		// strerror
		perror("can not bind");
		exit(EXIT_FAILURE);
	}

	int rc = listen(s, 5);
	if (rc) {
		perror("can not listen");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		// loop
		sock_t c = accept(s, 0, 0);

		// onetime echo server
		char buf[1024];
		ssize_t n_read = recv(c, buf, 1024, 0);
		send(c, buf, n_read, 0);

		sock_close(c);
		// https://stackoverflow.com/questions/52787742/socket-showing-ip-and-hostname-of-the-client
	}

	sock_uninit();
	return 0;
}
