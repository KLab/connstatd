#include<stdio.h>
#include<time.h>
#include<signal.h>
#include<sys/socket.h>
#include<net/tcp_states.h>
#include<linux/netlink.h>
#include<linux/inet_diag.h>

int                       sigflag;

static void SignalHandler(int sig)
{
  switch(sig){
    case SIGALRM:
      sigflag = 1;
      break;
  }
}

int scount(int n)
{
  int r=0;
  int len;
  static int  seq=1;
  static char rbuf[65535];
  struct {
    struct nlmsghdr      nlh;
    struct inet_diag_req req;
  } wbuf;
  struct nlmsghdr       *nlh;
  struct inet_diag_msg  *msg;

  wbuf.nlh.nlmsg_len          = NLMSG_ALIGN(NLMSG_LENGTH(sizeof(wbuf.req)));
  wbuf.nlh.nlmsg_type         = TCPDIAG_GETSOCK;
  wbuf.nlh.nlmsg_flags        = NLM_F_REQUEST | NLM_F_DUMP;
  wbuf.nlh.nlmsg_seq          = seq++;
  wbuf.req.idiag_family       = AF_INET;
  wbuf.req.idiag_src_len      = 0;
  wbuf.req.idiag_dst_len      = 0;
  wbuf.req.idiag_ext          = 0; 
  wbuf.req.idiag_states       = TCPF_ESTABLISHED;
  wbuf.req.idiag_dbs          = 0;
  wbuf.req.id.idiag_sport     = htons(3306);
  wbuf.req.id.idiag_dport     = 0;
  wbuf.req.id.idiag_cookie[0] = INET_DIAG_NOCOOKIE;
  wbuf.req.id.idiag_cookie[1] = INET_DIAG_NOCOOKIE;
  len = write(n, &wbuf, wbuf.nlh.nlmsg_len);
  if(len < 0){
    fprintf(stderr,"netlink write error!!\n");
    return(-1);
  }
  while(1){
    len=read(n, rbuf, sizeof(rbuf));
    if(len == -1){
      fprintf(stderr,"netlink read error!!\n");
      return(-1);
    }
    for(nlh=(struct nlmsghdr *)rbuf;NLMSG_OK(nlh, len);nlh=NLMSG_NEXT(nlh, len)){
      if(nlh->nlmsg_seq != wbuf.nlh.nlmsg_seq)
        continue;
      msg=(struct inet_diag_msg *)((char *)nlh + sizeof(struct nlmsghdr));
      if(nlh->nlmsg_type == NLMSG_ERROR){
        fprintf(stderr,"netlink msg error\n");
        return(-1);
      }
      if(nlh->nlmsg_type == NLMSG_DONE){
        return(r);
      }
      printf("%02d: state=%02d sport=%05d dport=%05d\n", r, msg->idiag_state, ntohs(msg->id.idiag_sport), ntohs(msg->id.idiag_dport));
      r++;
    }
  }
  return(0);
}

int main(int argc, char *argv[])
{
  int     c;
  int     m;
  int     n;
  timer_t t;
  struct itimerspec it;
  struct sigaction  sa;

  /*----- signal -----*/
  m = 0;
  sigflag = 0;
  sa.sa_flags = 0;
  sa.sa_handler = SignalHandler;
  sigemptyset(&sa.sa_mask);
  if(sigaction(SIGALRM, &sa, NULL) == -1){
    fprintf(stderr, "sigaction error\n");
    return(1);
  }
  /*----- timer -----*/
  it.it_interval.tv_sec  = 60;
  it.it_interval.tv_nsec = 0;
  it.it_value.tv_sec     = 60;
  it.it_value.tv_nsec    = 0;
  if(timer_create(CLOCK_REALTIME,NULL,&t) == -1){
    fprintf(stderr, "timer_create error\n");
    return(1);
  }
  if(timer_settime(t,0,&it,NULL) == -1){
    fprintf(stderr, "timer_settime error\n");
    return(1);
  }
  /* main loop */
  while(1){
    c = scount(n);
    if(c == -1)
      return(1);
    if(m < c)
      m = c;
		printf("m=%d\n",m);
    sleep(1);
  }
  return(0);
}

