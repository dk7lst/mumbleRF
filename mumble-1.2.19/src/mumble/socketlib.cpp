#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "socketlib.h"

IPAddr::IPAddr() {
 a.sin_family=AF_INET;
 clear();
}

IPAddr::IPAddr(unsigned long n_addr, unsigned short n_port) { // Parameter in network byte order!
 a.sin_family=AF_INET;
 a.sin_addr.s_addr=n_addr;
 a.sin_port=n_port;
}

 bool IPAddr::operator==(const IPAddr &other) const {
  return a.sin_family == other.a.sin_family && a.sin_addr.s_addr == other.a.sin_addr.s_addr && a.sin_port == other.a.sin_port;
 }

 bool IPAddr::operator!=(const IPAddr &other) const {
  return !operator==(other);
 }

void IPAddr::clear() {
 a.sin_addr.s_addr=a.sin_port=0;
}

char *IPAddr::tostring(char *buf) const { // Puffer muss gross genug sein!
 sprintf(buf,"%s:%d",inet_ntoa(a.sin_addr),ntohs(a.sin_port));
 return buf;
}

int IPAddr::setbydotstring(const char *dotstr) {
 return inet_aton(dotstr,&a.sin_addr);
}

int IPAddr::setbyhostname(const char *hostname) {
 struct hostent *he;
 if((he=gethostbyname(hostname))==NULL) return -1;
 a.sin_addr.s_addr=*(unsigned long *)*he->h_addr_list; // wir verwenden immer die erste gefundene Adresse
 return 0;
}

int IPAddr::setbyipport(const char *hostport) {
 char *ip,*port;
 if((ip=strdup(hostport))==NULL) return -1;
 if((port=strchr(ip,':'))==NULL)
  if((port=strchr(ip,'/'))==NULL) {
   free(ip);
   return -2;
  }
 *port++=0;
 if(setbyhostname(ip)) {
  free(ip);
  return -3;
 }
 setport(atoi(port));
 free(ip);
 return 0;
}

uint16_t IPAddr::getport() const {
 return ntohs(a.sin_port);
}

void IPAddr::setport(uint16_t port) {
 a.sin_port=htons(port);
}

int IPAddr::isset() const {
 return a.sin_addr.s_addr || a.sin_port;
}

int IPAddr::ismulticast() const {
 return (*(unsigned char *)&a.sin_addr.s_addr & 0xF0)==0xE0;
}

int IPAddr::islocalif() const {
 int result=-1,s;
 if((s=socket(AF_INET,SOCK_DGRAM,0))>=0) {
  struct ifconf ifc;
  struct ifreq *ifr;
  char buf[1024];
  ifc.ifc_buf=buf;
  ifc.ifc_len=sizeof(buf);
  if(ioctl(s,SIOCGIFCONF,&ifc)>=0) {
   ifr=ifc.ifc_req;
   result=0;
   for (unsigned i=0;i<ifc.ifc_len/sizeof(struct ifreq);++i)
    if(((sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr==a.sin_addr.s_addr) {
     result=1;
     break;
    }
  }
  close(s);
 }
 return result;
}

AbstractSocket::AbstractSocket() {
 sockfd = -1;
 socketmode = -1;
 reuseaddrflag = 0;
}

AbstractSocket::~AbstractSocket() {
 close();
}

int AbstractSocket::open() {
 close();
 sockfd = socket(PF_INET, socketmode, 0);
 return sockfd;
}

int AbstractSocket::close() {
 if(sockfd != -1) {
  ::close(sockfd);
  sockfd = -1;
 }
 return 0;
}

int AbstractSocket::bind() {
 return bind((uint16_t)0);
}

int AbstractSocket::bind(uint16_t port) {
 IPAddr interface;
 interface.a.sin_addr.s_addr=INADDR_ANY;
 interface.a.sin_port=htons(port);
 return bind(&interface);
}

int AbstractSocket::bind(IPAddr *interface) {
 if(open() == -1) return -1;
 if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&reuseaddrflag,sizeof(reuseaddrflag))) return -2;
 return ::bind(sockfd,(const sockaddr *)&interface->a,sizeof(interface->a));
}

int AbstractSocket::bind(IPAddr *bindaddr, uint16_t portoverride) {
 IPAddr interface;
 interface.a.sin_addr.s_addr=bindaddr->a.sin_addr.s_addr;
 interface.a.sin_port=htons(portoverride);
 return bind(&interface);
}

int AbstractSocket::getsockname(IPAddr *bindaddr, unsigned *bindaddrlen) const {
 if(bindaddrlen) *bindaddrlen=sizeof(bindaddr->a);
 return ::getsockname(sockfd,(sockaddr *)&bindaddr->a,bindaddrlen);
}

void AbstractSocket::enablereuseaddr(int onoff) {
 reuseaddrflag=onoff;
}

int AbstractSocket::enablerttlrecording(int onoff) const {
 return setsockopt(sockfd,SOL_IP,IP_RECVTTL,&onoff,sizeof(onoff));
}

int AbstractSocket::getttl(void) const {
  unsigned char ttl;
  socklen_t size=sizeof(ttl);
  if(getsockopt(sockfd, SOL_IP, IP_TTL, &ttl, &size)!=0) return -1;
  return ttl;
}

int AbstractSocket::setttl(int ttl) const {
 if(ttl<0 || ttl>255) return -1;
 uint8_t ttlbyte=ttl;
 return setsockopt(sockfd,IPPROTO_IP,IP_TTL,&ttlbyte,sizeof(ttlbyte));
}

bool AbstractSocket::waitfordata(int timeout_us) const {
 fd_set rxset;
 FD_ZERO(&rxset);
 FD_SET(sockfd, &rxset);
 timeval timeout;
 timeout.tv_sec = 0;
 timeout.tv_usec = timeout_us;
 return select(sockfd + 1, &rxset, NULL, NULL, &timeout) > 0;
}

UDPSocket::UDPSocket() : AbstractSocket() {
 socketmode = SOCK_DGRAM;
}

int UDPSocket::sendto(const IPAddr *destination, const void *buf, int bytes) const {
 return ::sendto(sockfd,buf,bytes,0,(const sockaddr *)&destination->a,sizeof(destination->a));
}

int UDPSocket::recvfrom(void *buf, int maxbytes, IPAddr *fromaddr, unsigned *fromaddrlen) const {
 if(fromaddrlen) *fromaddrlen=sizeof(fromaddr->a);
 return ::recvfrom(sockfd,buf,maxbytes,0,(sockaddr *)&fromaddr->a,fromaddrlen);
}

int UDPSocket::recvfrom(void *buf, int maxbytes, IPAddr *fromaddr, unsigned *fromaddrlen, int timeout_us) const {
  if(waitfordata(timeout_us)) return recvfrom(buf, maxbytes, fromaddr, fromaddrlen);
  if(fromaddrlen) *fromaddrlen = 0;
  return 0;
}

int UDPSocket::recvfrom(void *buf, int maxbytes, IPAddr *fromaddr, unsigned *fromaddrlen, int *ttl) const {
 if(!ttl) return recvfrom(buf,maxbytes,fromaddr,fromaddrlen);
 const int CMSGBUFSIZE=256;
 struct msghdr msg;
 struct iovec iov;
 int result;
 char cmsgbuf[CMSGBUFSIZE];
 iov.iov_base=buf;
 iov.iov_len=maxbytes;
 msg.msg_name=&fromaddr->a;
 msg.msg_namelen=(fromaddrlen)?sizeof(fromaddr->a):0;
 msg.msg_iov=&iov;
 msg.msg_iovlen=1;
 msg.msg_control=cmsgbuf;
 msg.msg_controllen=CMSGBUFSIZE;
 msg.msg_flags=0;
 *ttl=-1;
 result=recvmsg(sockfd,&msg,0);
 if(fromaddrlen) *fromaddrlen=msg.msg_namelen;
 if(result>=0) {
  struct cmsghdr *cmsg;
  for(cmsg=CMSG_FIRSTHDR(&msg);cmsg;cmsg=CMSG_NXTHDR(&msg,cmsg))
   if(cmsg->cmsg_level==SOL_IP && cmsg->cmsg_type==IP_TTL) {
    *ttl=*(int *)CMSG_DATA(cmsg);
    break;
   }
 }
 return result;
}

MulticastUDPSocket::MulticastUDPSocket() : UDPSocket() {
 reuseaddrflag=1;
}

int MulticastUDPSocket::join(const IPAddr *mcgroup) const {
 struct ip_mreqn m;
 m.imr_multiaddr=mcgroup->a.sin_addr;
 m.imr_address.s_addr=INADDR_ANY;
 m.imr_ifindex=0;
 return setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&m,sizeof(m));
}

int MulticastUDPSocket::join(const IPAddr *mcgroup, const IPAddr *interface) const {
 struct ip_mreqn m;
 m.imr_multiaddr=mcgroup->a.sin_addr;
 m.imr_address.s_addr=interface->a.sin_addr.s_addr;
 m.imr_ifindex=0;
 return setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&m,sizeof(m));
}

int MulticastUDPSocket::leave(const IPAddr *mcgroup) const {
 struct ip_mreqn m;
 m.imr_multiaddr=mcgroup->a.sin_addr;
 m.imr_address.s_addr=INADDR_ANY;
 m.imr_ifindex=0;
 return setsockopt(sockfd,IPPROTO_IP,IP_DROP_MEMBERSHIP,&m,sizeof(m));
}

int MulticastUDPSocket::getttl(void) const {
 int ttl;
 socklen_t size=sizeof(ttl);
 if(getsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,&size) != 0) return -1;
 return ttl;
}

int MulticastUDPSocket::setttl(int ttl) const {
 return setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl));
}

int MulticastUDPSocket::enableloopmode(int flag) const {
 return setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_LOOP,&flag,sizeof(flag));
}

int MulticastUDPSocket::setoutgoinginterface(const IPAddr *interface) const {
 return setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_IF,&interface->a.sin_addr.s_addr,sizeof(interface->a.sin_addr.s_addr));
}

TCPSocket::TCPSocket() : AbstractSocket() {
 socketmode = SOCK_STREAM;
}

int TCPSocket::connect(const IPAddr *destination) {
 if(open() == -1) return -1;
 return ::connect(sockfd, (const sockaddr *)&destination->a, sizeof(destination->a));
}

int TCPSocket::send(const void *buf, int bytes) const {
 return ::send(sockfd, buf, bytes, 0);
}

int TCPSocket::sendline(const char *str) const {
 return send(str, strlen(str));
}

int TCPSocket::recv(void *buf, int maxbytes) const {
 return ::recv(sockfd, buf, maxbytes, 0);
}

int TCPSocket::recvline(void *buf, int maxbytes) const {
 char *ptr = (char *)buf, byte;
 while(maxbytes > 1) {
  if(recv(&byte, 1) <= 0) break;
  if(byte == 13) break; // Zeilenende
  if(byte == 10) {
    if(ptr > buf) break; // Offenbar sendet Gegenstelle nur \r, daher als Zeilenende interpretieren
    continue; // \r gehört vermutlich zum letzten \n, daher ignorieren
  }
  *ptr++ = byte;
  --maxbytes;
 }
 if(maxbytes > 0) *ptr = 0;
 return ptr - (char *)buf;
}
