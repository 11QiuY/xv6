//
// network tests
// to be used with nettest.py (run outside of qemu)
//

#include "kernel/types.h"
#include "kernel/spinlock.h"
#include "kernel/net.h"
#include "kernel/stat.h"
#include "user/user.h"

//
// send a single UDP packet (but don't recv() the reply).
// python3 nettest.py txone can be used to wait for
// this packet, and you can also see what
// happened with tcpdump -XXnr packets.pcap
//
void txone() {
  printf("txone: sending one packet\n");
  uint32 dst = 0x0A000202;  // 10.0.2.2
  int    dport = NET_TESTS_PORT;
  char   buf[5];
  buf[0] = 't';
  buf[1] = 'x';
  buf[2] = 'o';
  buf[3] = 'n';
  buf[4] = 'e';
  if (send(2003, dst, dport, buf, 5) < 0) {
    printf("txone: send() failed\n");
  }
}

//
// test just receive.
// outside of qemu, run
//   ./nettest.py rx
//
int rx(char *name) {
  bind(2000);

  int lastseq = -1;
  int ok = 0;

  while (ok < 4) {
    char   ibuf[128];
    uint32 src;
    uint16 sport;
    int    cc = recv(2000, &src, &sport, ibuf, sizeof(ibuf) - 1);
    if (cc < 0) {
      fprintf(2, "nettest %s: recv() failed\n", name);
      return 0;
    }

    if (src != 0x0A000202) {  // 10.0.2.2
      printf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("packet 1")) {
      printf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("packet xxxxxx")) {
      printf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "packet ", strlen("packet ")) != 0) {
      printf("packet doesn't start with packet\n");
      return 0;
    }

    ibuf[cc] = '\0';
#define isdigit(x) ((x) >= '0' && (x) <= '9')
    if (!isdigit(ibuf[7])) {
      printf("packet doesn't contain a number\n");
      return 0;
    }
    for (int i = 7; i < cc; i++) {
      if (!isdigit(ibuf[i])) {
        printf("packet contains non-digits in the number\n");
        return 0;
      }
    }
    int seq = ibuf[7] - '0';
    if (isdigit(ibuf[8])) {
      seq *= 10;
      seq += ibuf[8] - '0';
      if (isdigit(ibuf[9])) {
        seq *= 10;
        seq += ibuf[9] - '0';
      }
    }

    if (lastseq != -1) {
      if (seq != lastseq + 1) {
        printf("got seq %d, expecting %d\n", seq, lastseq + 1);
        return 0;
      }
    }

    lastseq = seq;

    ok += 1;
  }

  printf("%s: OK\n", name);

  return 1;
}

//
// test receive on two different ports, interleaved.
// outside of qemu, run
//   ./nettest.py rx2
//
int rx2() {
  bind(2000);
  bind(2001);

  for (int i = 0; i < 3; i++) {
    char   ibuf[128];
    uint32 src;
    uint16 sport;
    int    cc = recv(2000, &src, &sport, ibuf, sizeof(ibuf) - 1);
    if (cc < 0) {
      fprintf(2, "nettest rx2: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) {  // 10.0.2.2
      printf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("one 1")) {
      printf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("one xxxxxx")) {
      printf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "one ", strlen("one ")) != 0) {
      printf("packet doesn't start with one\n");
      return 0;
    }
  }

  for (int i = 0; i < 3; i++) {
    char   ibuf[128];
    uint32 src;
    uint16 sport;
    int    cc = recv(2001, &src, &sport, ibuf, sizeof(ibuf) - 1);
    if (cc < 0) {
      fprintf(2, "nettest rx2: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) {  // 10.0.2.2
      printf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("one 1")) {
      printf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("one xxxxxx")) {
      printf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "two ", strlen("two ")) != 0) {
      printf("packet doesn't start with two\n");
      return 0;
    }
  }

  for (int i = 0; i < 3; i++) {
    char   ibuf[128];
    uint32 src;
    uint16 sport;
    int    cc = recv(2000, &src, &sport, ibuf, sizeof(ibuf) - 1);
    if (cc < 0) {
      fprintf(2, "nettest rx2: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) {  // 10.0.2.2
      printf("wrong ip src %x\n", src);
      return 0;
    }

    if (cc < strlen("one 1")) {
      printf("len %d too short\n", cc);
      return 0;
    }

    if (cc > strlen("one xxxxxx")) {
      printf("len %d too long\n", cc);
      return 0;
    }

    if (memcmp(ibuf, "one ", strlen("one ")) != 0) {
      printf("packet doesn't start with one\n");
      return 0;
    }
  }

  printf("rx2: OK\n");

  return 1;
}

//
// send some UDP packets to nettest.py tx.
//
int tx() {
  for (int ii = 0; ii < 5; ii++) {
    uint32 dst = 0x0A000202;  // 10.0.2.2
    int    dport = NET_TESTS_PORT;
    char   buf[3];
    buf[0] = 't';
    buf[1] = ' ';
    buf[2] = '0' + ii;
    if (send(2000, dst, dport, buf, 3) < 0) {
      printf("send() failed\n");
      return 0;
    }
    sleep(10);
  }

  // can't actually tell if the packets arrived.
  return 1;
}

//
// send just one UDP packets to nettest.py ping,
// expect a reply.
// nettest.py ping must be started first.
//
int ping0() {
  printf("ping0: starting\n");

  bind(2004);

  uint32 dst = 0x0A000202;  // 10.0.2.2
  int    dport = NET_TESTS_PORT;
  char   buf[5];
  memcpy(buf, "ping0", sizeof(buf));
  if (send(2004, dst, dport, buf, sizeof(buf)) < 0) {
    printf("ping0: send() failed\n");
    return 0;
  }

  char   ibuf[128];
  uint32 src = 0;
  uint16 sport = 0;
  memset(ibuf, 0, sizeof(ibuf));
  int cc = recv(2004, &src, &sport, ibuf, sizeof(ibuf) - 1);
  if (cc < 0) {
    fprintf(2, "ping0: recv() failed\n");
    return 0;
  }

  if (src != 0x0A000202) {  // 10.0.2.2
    printf("ping0: wrong ip src %x, expecting %x\n", src, 0x0A000202);
    return 0;
  }

  if (sport != NET_TESTS_PORT) {
    printf("ping0: wrong sport %d, expecting %d\n", sport, NET_TESTS_PORT);
    return 0;
  }

  if (memcmp(buf, ibuf, sizeof(buf)) != 0) {
    printf("ping0: wrong content\n");
    return 0;
  }

  if (cc != sizeof(buf)) {
    printf("ping0: wrong length %d, expecting %ld\n", cc, sizeof(buf));
    return 0;
  }

  printf("ping0: OK\n");

  return 1;
}

//
// send many UDP packets to nettest.py ping,
// expect a reply to each.
// nettest.py ping must be started first.
//
int ping1() {
  printf("ping1: starting\n");

  bind(2005);

  for (int ii = 0; ii < 20; ii++) {
    uint32 dst = 0x0A000202;  // 10.0.2.2
    int    dport = NET_TESTS_PORT;
    char   buf[3];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = '0' + ii;
    if (send(2005, dst, dport, buf, 3) < 0) {
      printf("ping1: send() failed\n");
      return 0;
    }

    char   ibuf[128];
    uint32 src = 0;
    uint16 sport = 0;
    memset(ibuf, 0, sizeof(ibuf));
    int cc = recv(2005, &src, &sport, ibuf, sizeof(ibuf) - 1);
    if (cc < 0) {
      fprintf(2, "ping1: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) {  // 10.0.2.2
      printf("ping1: wrong ip src %x, expecting %x\n", src, 0x0A000202);
      return 0;
    }

    if (sport != NET_TESTS_PORT) {
      printf("ping1: wrong sport %d, expecting %d\n", sport, NET_TESTS_PORT);
      return 0;
    }

    if (memcmp(buf, ibuf, 3) != 0) {
      printf("ping1: wrong content %s, expecting %s\n", ibuf, buf);
      return 0;
    }

    if (cc != 3) {
      printf("ping1: wrong length %d, expecting 3\n", cc);
      return 0;
    }
  }

  printf("ping1: OK\n");

  return 1;
}

//
// send UDP packets from two different ports to nettest.py ping,
// expect a reply to each to appear on the correct port.
// nettest.py ping must be started first.
//
int ping2() {
  printf("ping2: starting\n");

  bind(2006);
  bind(2007);

  for (int ii = 0; ii < 5; ii++) {
    for (int port = 2006; port <= 2007; port++) {
      uint32 dst = 0x0A000202;  // 10.0.2.2
      int    dport = NET_TESTS_PORT;
      char   buf[4];
      buf[0] = 'p';
      buf[1] = ' ';
      buf[2] = (port == 2006 ? 'a' : 'A') + ii;
      buf[3] = '!';
      if (send(port, dst, dport, buf, 4) < 0) {
        printf("ping2: send() failed\n");
        return 0;
      }
    }
  }

  for (int port = 2006; port <= 2007; port++) {
    for (int ii = 0; ii < 5; ii++) {
      char   ibuf[128];
      uint32 src = 0;
      uint16 sport = 0;
      memset(ibuf, 0, sizeof(ibuf));
      int cc = recv(port, &src, &sport, ibuf, sizeof(ibuf) - 1);
      if (cc < 0) {
        fprintf(2, "ping2: recv() failed\n");
        return 0;
      }

      if (src != 0x0A000202) {  // 10.0.2.2
        printf("ping2: wrong ip src %x\n", src);
        return 0;
      }

      if (sport != NET_TESTS_PORT) {
        printf("ping2: wrong sport %d\n", sport);
        return 0;
      }

      if (cc != 4) {
        printf("ping2: wrong length %d\n", cc);
        return 0;
      }

      // printf("port=%d ii=%d: %c%c%c\n", port, ii, ibuf[0], ibuf[1], ibuf[2]);

      char buf[4];
      buf[0] = 'p';
      buf[1] = ' ';
      buf[2] = (port == 2006 ? 'a' : 'A') + ii;
      buf[3] = '!';

      if (memcmp(buf, ibuf, 3) != 0) {
        // possibly recv() sees packets out of order.
        printf("ping2: wrong content\n");
        return 0;
      }
    }
  }

  printf("ping2: OK\n");

  return 1;
}

//
// send a big burst of packets from ports 2008 and 2010,
// causing drops,
// bracketed by two packets from port 2009.
// check that the two packets can be recv()'d on port 2009.
// check that port 2008 had a finite queue length (dropped some).
// nettest.py ping must be started first.
//
int ping3() {
  printf("ping3: starting\n");

  bind(2008);
  bind(2009);

  //
  // send one packet on 2009.
  //
  {
    uint32 dst = 0x0A000202;  // 10.0.2.2
    int    dport = NET_TESTS_PORT;
    char   buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'A';
    buf[3] = '!';
    if (send(2009, dst, dport, buf, 4) < 0) {
      printf("ping3: send() failed\n");
      return 0;
    }
  }
  sleep(1);

  //
  // send so many packets from 2008 and 2010 that some of the
  // replies must be dropped due to the requirement
  // for finite maximum queueing.
  //
  for (int ii = 0; ii < 257; ii++) {
    uint32 dst = 0x0A000202;  // 10.0.2.2
    int    dport = NET_TESTS_PORT;
    char   buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'a' + ii;
    buf[3] = '!';
    int port = 2008 + (ii % 2) * 2;
    if (send(port, dst, dport, buf, 4) < 0) {
      printf("ping3: send() failed\n");
      return 0;
    }
  }
  sleep(1);

  //
  // send another packet from 2009.
  //
  {
    uint32 dst = 0x0A000202;  // 10.0.2.2
    int    dport = NET_TESTS_PORT;
    char   buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'B';
    buf[3] = '!';
    if (send(2009, dst, dport, buf, 4) < 0) {
      printf("ping3: send() failed\n");
      return 0;
    }
  }

  //
  // did both reply packets for 2009 arrive?
  //
  for (int ii = 0; ii < 2; ii++) {
    char   ibuf[128];
    uint32 src = 0;
    uint16 sport = 0;
    memset(ibuf, 0, sizeof(ibuf));
    int cc = recv(2009, &src, &sport, ibuf, sizeof(ibuf) - 1);
    if (cc < 0) {
      fprintf(2, "ping3: recv() failed\n");
      return 0;
    }

    if (src != 0x0A000202) {  // 10.0.2.2
      printf("ping3: wrong ip src %x\n", src);
      return 0;
    }

    if (sport != NET_TESTS_PORT) {
      printf("ping3: wrong sport %d\n", sport);
      return 0;
    }

    if (cc != 4) {
      printf("ping3: wrong length %d\n", cc);
      return 0;
    }

    // printf("port=%d ii=%d: %c%c%c\n", port, ii, ibuf[0], ibuf[1], ibuf[2]);

    char buf[4];
    buf[0] = 'p';
    buf[1] = ' ';
    buf[2] = 'A' + ii;
    buf[3] = '!';

    if (memcmp(buf, ibuf, 3) != 0) {
      // possibly recv() sees packets out of order.
      // possibly the burst on 2008 caused 2009's
      // packets to be dropped.
      printf("ping3: wrong content\n");
      return 0;
    }
  }

  //
  // now count how many replies were queued for 2008.
  //
  int fds[2];
  pipe(fds);
  int pid = fork();
  if (pid == 0) {
    close(fds[0]);
    write(fds[1], ":", 1);  // ensure parent's read() doesn't block
    while (1) {
      char   ibuf[128];
      uint32 src = 0;
      uint16 sport = 0;
      memset(ibuf, 0, sizeof(ibuf));
      int cc = recv(2008, &src, &sport, ibuf, sizeof(ibuf) - 1);
      if (cc < 0) {
        printf("ping3: recv failed\n");
        break;
      }
      write(fds[1], "x", 1);
    }
    exit(0);
  }
  close(fds[1]);

  sleep(5);
  static char nbuf[512];
  int         n = read(fds[0], nbuf, sizeof(nbuf));
  close(fds[0]);
  kill(pid);

  n -= 1;  // the ":"
  if (n > 16) {
    printf("ping3: too many packets (%d) were queued on a UDP port\n", n);
    return 0;
  }

  printf("ping3: OK\n");

  return 1;
}

// Encode a DNS name
void encode_qname(char *qn, char *host) {
  char *l = host;

  for (char *c = host; c < host + strlen(host) + 1; c++) {
    if (*c == '.') {
      *qn++ = (char)(c - l);
      for (char *d = l; d < c; d++) {
        *qn++ = *d;
      }
      l = c + 1;  // skip .
    }
  }
  *qn = '\0';
}

// Decode a DNS name
void decode_qname(char *qn, int max) {
  char *qnMax = qn + max;
  while (1) {
    if (qn >= qnMax) {
      printf("invalid DNS reply\n");
      exit(1);
    }
    int l = *qn;
    if (l == 0) break;
    for (int i = 0; i < l; i++) {
      *qn = *(qn + 1);
      qn++;
    }
    *qn++ = '.';
  }
}

// Make a DNS request
int dns_req(uint8 *obuf) {
  int         len = 0;

  struct dns *hdr = (struct dns *)obuf;
  hdr->id = htons(6828);
  hdr->rd = 1;
  hdr->qdcount = htons(1);

  len += sizeof(struct dns);

  // qname part of question
  char *qname = (char *)(obuf + sizeof(struct dns));
  char *s = "pdos.csail.mit.edu.";
  encode_qname(qname, s);
  len += strlen(qname) + 1;

  // constants part of question
  struct dns_question *h = (struct dns_question *)(qname + strlen(qname) + 1);
  h->qtype = htons(0x1);
  h->qclass = htons(0x1);

  len += sizeof(struct dns_question);
  return len;
}

// Process DNS response
int dns_rep(uint8 *ibuf, int cc) {
  struct dns *hdr = (struct dns *)ibuf;
  int         len;
  char       *qname = 0;
  int         record = 0;

  if (cc < sizeof(struct dns)) {
    printf("DNS reply too short\n");
    return 0;
  }

  if (!hdr->qr) {
    printf("Not a DNS reply for %d\n", ntohs(hdr->id));
    return 0;
  }

  if (hdr->id != htons(6828)) {
    printf("DNS wrong id: %d\n", ntohs(hdr->id));
    return 0;
  }

  if (hdr->rcode != 0) {
    printf("DNS rcode error: %x\n", hdr->rcode);
    return 0;
  }

  // printf("qdcount: %x\n", ntohs(hdr->qdcount));
  // printf("ancount: %x\n", ntohs(hdr->ancount));
  // printf("nscount: %x\n", ntohs(hdr->nscount));
  // printf("arcount: %x\n", ntohs(hdr->arcount));

  len = sizeof(struct dns);

  for (int i = 0; i < ntohs(hdr->qdcount); i++) {
    char *qn = (char *)(ibuf + len);
    qname = qn;
    decode_qname(qn, cc - len);
    len += strlen(qn) + 1;
    len += sizeof(struct dns_question);
  }

  for (int i = 0; i < ntohs(hdr->ancount); i++) {
    if (len >= cc) {
      printf("dns: invalid DNS reply\n");
      return 0;
    }

    char *qn = (char *)(ibuf + len);

    if ((int)qn[0] > 63) {  // compression?
      qn = (char *)(ibuf + qn[1]);
      len += 2;
    } else {
      decode_qname(qn, cc - len);
      len += strlen(qn) + 1;
    }

    struct dns_data *d = (struct dns_data *)(ibuf + len);
    len += sizeof(struct dns_data);
    // printf("type %d ttl %d len %d\n", ntohs(d->type), ntohl(d->ttl),
    // ntohs(d->len));
    if (ntohs(d->type) == ARECORD && ntohs(d->len) == 4) {
      record = 1;
      printf("DNS arecord for %s is ", qname ? qname : "");
      uint8 *ip = (ibuf + len);
      printf("%d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
      if (ip[0] != 128 || ip[1] != 52 || ip[2] != 129 || ip[3] != 126) {
        printf("dns: wrong ip address");
        return 0;
      }
      len += 4;
    }
  }
  printf("len = %d, cc = %d\n", len, cc);

  // // needed for DNS servers with EDNS support
  // for (int i = 0; i < ntohs(hdr->arcount); i++) {
  //   char *qn = (char *)(ibuf + len);
  //   if (*qn != 0) {
  //     printf("dns: invalid name for EDNS\n");
  //     return 0;
  //   }
  //   len += 1;

  //   struct dns_data *d = (struct dns_data *)(ibuf + len);
  //   len += sizeof(struct dns_data);
  //   if (ntohs(d->type) != 41) {
  //     printf("dns: invalid type for EDNS\n");
  //     return 0;
  //   }
  //   len += ntohs(d->len);
  // }

  // if (len != cc) {
  //   printf("dns: processed %d data bytes but received %d\n", len, cc);
  //   return 0;
  // }
  if (!record) {
    printf("dns: didn't receive an arecord\n");
    return 0;
  }

  return 1;
}

int dns() {
#define N 1000
  uint8  obuf[N];
  uint8  ibuf[N];
  uint32 dst;
  int    len;

  printf("dns: starting\n");

  memset(obuf, 0, N);
  memset(ibuf, 0, N);

  // 8.8.8.8: google's name server
  dst = (8 << 24) | (8 << 16) | (8 << 8) | (8 << 0);

  len = dns_req(obuf);

  bind(10000);

  if (send(10000, dst, 53, (char *)obuf, len) < 0) {
    fprintf(2, "dns: send() failed\n");
    return 0;
  }

  uint32 src;
  uint16 sport;
  int    cc = recv(10000, &src, &sport, (char *)ibuf, sizeof(ibuf));
  if (cc < 0) {
    fprintf(2, "dns: recv() failed\n");
    return 0;
  }
  printf("dns: received %d bytes\n", cc);
  printf("dns: received src : %x\n", src);
  printf("dns: received sport : %d\n", sport);
  printf("dns: received ibuf: %s\n", ibuf);

  if (dns_rep(ibuf, cc)) {
    printf("dns: OK\n");
    return 1;
  } else {
    return 0;
  }
}

void usage() {
  printf("Usage: nettest txone\n");
  printf("       nettest tx\n");
  printf("       nettest rx\n");
  printf("       nettest rx2\n");
  printf("       nettest rxburst\n");
  printf("       nettest ping1\n");
  printf("       nettest ping2\n");
  printf("       nettest ping3\n");
  printf("       nettest dns\n");
  printf("       nettest grade\n");
  exit(1);
}

//
// use sbrk() to count how many free physical memory pages there are.
// touches the pages to force allocation.
// because out of memory with lazy allocation results in the process
// taking a fault and being killed, fork and report back.
//
int countfree() {
  int fds[2];

  if (pipe(fds) < 0) {
    printf("pipe() failed in countfree()\n");
    exit(1);
  }

  int pid = fork();

  if (pid < 0) {
    printf("fork failed in countfree()\n");
    exit(1);
  }

  if (pid == 0) {
    close(fds[0]);

    while (1) {
      uint64 a = (uint64)sbrk(4096);
      if (a == 0xffffffffffffffff) {
        break;
      }

      // modify the memory to make sure it's really allocated.
      *(char *)(a + 4096 - 1) = 1;

      // report back one more page.
      if (write(fds[1], "x", 1) != 1) {
        printf("write() failed in countfree()\n");
        exit(1);
      }
    }

    exit(0);
  }

  close(fds[1]);

  int n = 0;
  while (1) {
    char c;
    int  cc = read(fds[0], &c, 1);
    if (cc < 0) {
      printf("read() failed in countfree()\n");
      exit(1);
    }
    if (cc == 0) break;
    n += 1;
  }

  close(fds[0]);
  wait((int *)0);

  return n;
}

int main(int argc, char *argv[]) {
  if (argc != 2) usage();

  if (strcmp(argv[1], "txone") == 0) {
    txone();
  } else if (strcmp(argv[1], "rx") == 0 || strcmp(argv[1], "rxburst") == 0) {
    rx(argv[1]);
  } else if (strcmp(argv[1], "rx2") == 0) {
    rx2();
  } else if (strcmp(argv[1], "tx") == 0) {
    tx();
  } else if (strcmp(argv[1], "ping0") == 0) {
    ping0();
  } else if (strcmp(argv[1], "ping1") == 0) {
    ping1();
  } else if (strcmp(argv[1], "ping2") == 0) {
    ping2();
  } else if (strcmp(argv[1], "ping3") == 0) {
    ping3();
  } else if (strcmp(argv[1], "grade") == 0) {
    //
    // "python3 nettest.py grade" must already be running...
    //
    int free0 = countfree();
    int free1 = 0;
    txone();
    sleep(2);
    ping0();
    sleep(2);
    ping1();
    sleep(2);
    ping2();
    sleep(2);
    ping3();
    sleep(2);
    dns();
    sleep(2);
    if ((free1 = countfree()) + 32 < free0) {
      printf("free: FAILED -- lost too many free pages %d (out of %d)\n", free1,
             free0);
    } else {
      printf("free: OK\n");
    }
  } else if (strcmp(argv[1], "dns") == 0) {
    dns();
  } else {
    usage();
  }

  exit(0);
}
