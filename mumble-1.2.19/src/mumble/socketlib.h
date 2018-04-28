#pragma once
#include <netinet/in.h>

// Repraesentation einer IPv4 Unicast bzw. Multicast-Adresse
class IPAddr {
public:
 struct sockaddr_in a;

 IPAddr();
 IPAddr(unsigned long n_addr, unsigned short n_port);

 bool operator==(const IPAddr &other) const;
 bool operator!=(const IPAddr &other) const;

 void clear();
 char *tostring(char *buf) const;
 int setbydotstring(const char *dotstr);
 int setbyhostname(const char *hostname);
 int setbyipport(const char *hostport);
 uint16_t getport() const;
 void setport(uint16_t port);
 int isset() const;
 int ismulticast() const;
 int islocalif() const;
};

// Methoden und Attribute, die fuer alle Sockets gueltig sind
class AbstractSocket {
protected:
 int sockfd, socketmode;
 int reuseaddrflag;

public:  
 AbstractSocket();
 virtual ~AbstractSocket();
 
 virtual int open();
 virtual int close();
 virtual int bind(); // Port automatisch waehlen
 virtual int bind(uint16_t port); // Port an allen Interfaces binden
 virtual int bind(IPAddr *interface); // Port nur an einem Interface binden
 virtual int bind(IPAddr *bindaddr, uint16_t portoverride);
 virtual int getsockname(IPAddr *bindaddr, unsigned *bindaddrlen) const;
 virtual void enablereuseaddr(int onoff);
 virtual int enablerttlrecording(int onoff) const;
 //virtual int enableblockingmode(int onoff); 
 virtual int getttl() const;
 virtual int setttl(int ttl) const;
 virtual int getsocket() const {return sockfd;}
 virtual bool waitfordata(int timeout_us) const;
};

// Schnittstelle zu den Funktionen des Betriebsystems
class UDPSocket : public AbstractSocket {
public:
 UDPSocket();
 virtual int sendto(const IPAddr *destination, const void *buf, int bytes) const;
 virtual int recvfrom(void *buf, int maxbytes, IPAddr *fromaddr, unsigned *fromaddrlen) const;
 virtual int recvfrom(void *buf, int maxbytes, IPAddr *fromaddr, unsigned *fromaddrlen, int timeout_us) const;
 virtual int recvfrom(void *buf, int maxbytes, IPAddr *fromaddr, unsigned *fromaddrlen, int *ttl) const;
};

class MulticastUDPSocket : public UDPSocket {
public:
 MulticastUDPSocket();
 
 virtual int join(const IPAddr *mcgroup) const;
 virtual int join(const IPAddr *mcgroup, const IPAddr *interface) const;
 virtual int leave(const IPAddr *mcgroup) const;
 virtual int getttl() const;
 virtual int setttl(int ttl) const;
 virtual int enableloopmode(int flag) const;
 virtual int setoutgoinginterface(const IPAddr *interface) const;
};

class TCPSocket : public AbstractSocket {
public:
 TCPSocket();

 virtual int connect(const IPAddr *destination);
 virtual int send(const void *buf, int bytes) const;
 virtual int sendline(const char *str) const;
 virtual int recv(void *buf, int maxbytes) const;
 virtual int recvline(void *buf, int maxbytes) const;
};
