#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8  local_mac[ETHADDR_LEN] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};

static struct spinlock netlock;
static struct spinlock portlock;  // protect the ports array

void                   netinit(void) {
  initlock(&netlock, "netlock");
  initlock(&portlock, "portlock");
}

struct port ports[128];
int         port_end = 0;

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64 sys_bind(void) {
  //
  // Your code here.
  //
  int port;
  int h = -1;
  argint(0, &port);

  // printf("bind: port %d\n", port);

  if (port < 0 || port > 65535) return -1;

  acquire(&portlock);

  for (int i = 0; i < port_end; i++) {
    if (!ports[i].valid && h == -1) h = i;
    if (ports[i].valid && ports[i].port == port) {
      release(&portlock);
      return -1;
    }
  }
  if (h == -1) {
    h = port_end;
    port_end++;
  }
  ports[h].port = port;
  ports[h].valid = 1;
  ports[h].head = 0;
  ports[h].tail = 0;
  initlock(&ports[h].lock, "port");
  // printf("bind: port %d\n", port);

  release(&portlock);

  return 0;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64 sys_unbind(void) {
  //
  // Optional: Your code here.
  //
  int port;
  argint(0, &port);

  if (port < 0 || port > 65535) return -1;

  acquire(&portlock);
  for (int i = 0; i < port_end; i++) {
    if (ports[i].valid && ports[i].port == port) {
      ports[i].valid = 0;
      for (int j = ports[i].head; j != ports[i].tail;
           j = (j + 1) % PORT_MAX_BUF) {
        kfree(ports[i].buf[j]);
      }
      ports[i].head = 0;
      ports[i].tail = 0;
      release(&portlock);
      return 0;
    }
  }
  release(&portlock);
  return 0;
}

struct port *get_port(int port) {
  acquire(&portlock);
  for (int i = 0; i < port_end; i++) {
    if (ports[i].valid && ports[i].port == port) {
      release(&portlock);
      return &ports[i];
    }
  }
  release(&portlock);
  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64 sys_recv(void) {
  //
  // Your code here.
  //
  int    dport;
  uint64 src;
  uint64 sport;
  uint64 bufaddr;
  int    maxlen;

  argint(0, &dport);
  argaddr(1, &src);
  argaddr(2, &sport);
  argaddr(3, &bufaddr);
  argint(4, &maxlen);

  // printf("recv: dport %d\n", dport);

  if (dport < 0 || dport > 65535) return -1;
  struct port *p = get_port(dport);
  if (p == 0) {
    panic("recv: get_port failed");
    return -1;
  }
  acquire(&p->lock);

  int head = p->head;
  int tail = p->tail;

  while (head == tail) {  // 如果环形队列为空，等待port收到数据包
    sleep(p, &p->lock);
    head = p->head;
    tail = p->tail;
  }
  // printf("recv: head %d tail %d\n", head, tail);
  static int recv_times = 0;
  recv_times++;
  printf("recv: recv_times %d\n", recv_times);

  struct eth *eth = (struct eth *)p->buf[head];
  struct ip  *ip = (struct ip *)(eth + 1);

  if (ip->ip_p != IPPROTO_UDP) {
    panic("recv: ip->ip_p != IPPROTO_UDP");
    goto error;
  }
  // copy src
  uint32       ip_src = htonl(ip->ip_src);
  struct proc *proc = myproc();
  // printf("recv: ip_src %x\n", ip_src);

  if (copyout(proc->pagetable, src, (char *)&ip_src, sizeof(ip_src)) < 0) {
    panic("recv: copyout failed");
    goto error;
  }
  // copy sport
  struct udp *udp = (struct udp *)(ip + 1);
  uint16      sport_ = ntohs(udp->sport);
  uint16      dport_ = ntohs(udp->dport);

  if (dport_ != dport) {
    panic("recv: dport != dport_");
    goto error;
  }

  // printf("recv: sport %d\n", sport_);
  if (copyout(proc->pagetable, sport, (char *)&sport_, sizeof(sport_)) < 0) {
    panic("recv: copyout failed");
    goto error;
  }

  // copy buf
  int len = ntohs(udp->ulen) - sizeof(struct udp);
  len = len > maxlen ? maxlen : len;

  char *payload = (char *)(udp + 1);
  // if (sport_ == 53) {
  //   printf("recv: len %d\n", len);
  //   printf("recv: payload %s\n", payload);
  // }
  if (copyout(proc->pagetable, bufaddr, payload, len) < 0) {
    panic("recv: copyout failed");
    goto error;
  }

  static int free_times = 0;
  free_times++;
  printf("recv: free_times %d\n", free_times);
  kfree(p->buf[head]);
  p->head = (head + 1) % PORT_MAX_BUF;
  release(&p->lock);

  return len;
error:
  kfree(p->buf[head]);
  if (holding(&p->lock)) release(&p->lock);
  return -1;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short in_cksum(const unsigned char *addr, int len) {
  int                   nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int          sum = 0;
  unsigned short        answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64 sys_send(void) {
  struct proc *p = myproc();
  int          sport;
  int          dst;
  int          dport;
  uint64       bufaddr;
  int          len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  // printf("send: sport %d dst %x dport %d len %d\n", sport, dst, dport, len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if (total > PGSIZE) return -1;

  char *buf = kalloc();
  if (buf == 0) {
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *)buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45;  // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if (copyin(p->pagetable, payload, bufaddr, len) < 0) {
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void ip_rx(char *inbuf, int len) {
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if (seen_ip == 0) printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //
  // char       *buf = kalloc();
  memset(inbuf + len, 0, PGSIZE - len);
  struct eth *eth = (struct eth *)inbuf;
  struct ip  *ip = (struct ip *)(eth + 1);
  struct udp *udp = (struct udp *)(ip + 1);
  // memmove(buf, ip, len - sizeof(struct eth));

  int dport = ntohs(udp->dport);

  // printf("ip_rx: dport %d buf len %ld\n", dport,
  //        (len - sizeof(struct eth) - sizeof(struct ip) - sizeof(struct
  //        udp)));
  // printf("ip_rx: payload %s\n", (char *)(udp + 1));

  struct port *p = get_port(dport);
  if (p == 0) {
    kfree(inbuf);
    // kfree(buf);
    return;
  }
  acquire(&p->lock);
  int head = p->head;
  int tail = p->tail;
  if ((tail + 1) % PORT_MAX_BUF == head) {  // 如果环形队列满，丢弃数据包
    // printf("ip_rx: port %d full\n", dport);
    kfree(inbuf);
    // kfree(buf);
    // wakeup(p);
    release(&p->lock);
    return;
  }
  p->buf[tail] = inbuf;
  p->tail = (tail + 1) % PORT_MAX_BUF;
  wakeup(p);
  release(&p->lock);
  // kfree(inbuf);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void arp_rx(char *inbuf) {
  static int seen_arp = 0;

  if (seen_arp) {
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *)inbuf;
  struct arp *inarp = (struct arp *)(ineth + 1);

  char       *buf = kalloc();
  if (buf == 0) panic("send_arp_reply");

  struct eth *eth = (struct eth *)buf;
  memmove(eth->dhost, ineth->shost,
          ETHADDR_LEN);  // ethernet destination = query source
  memmove(eth->shost, local_mac,
          ETHADDR_LEN);  // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void net_rx(char *buf, int len) {
  struct eth *eth = (struct eth *)buf;

  if (len >= sizeof(struct eth) + sizeof(struct arp) &&
      ntohs(eth->type) == ETHTYPE_ARP) {
    arp_rx(buf);
  } else if (len >= sizeof(struct eth) + sizeof(struct ip) &&
             ntohs(eth->type) == ETHTYPE_IP) {
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
