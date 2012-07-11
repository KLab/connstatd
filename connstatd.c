#include<stdio.h>
#include<time.h>
#include<signal.h>
#include<sys/socket.h>
#include<net/tcp_states.h>
#include<linux/netlink.h>
#include<linux/inet_diag.h>
#include "ganglia.h"

int                       sigflag;
Ganglia_pool              context;
Ganglia_metric            gmetric;
Ganglia_udp_send_channels channel;
Ganglia_gmond_config      gconfig;

static void SignalHandler(int sig)
{
  switch(sig){
    case SIGALRM:
      sigflag = 1;
      break;
  }
}

int gsend(int m)
{
  int  r;
  char v[8];
  sprintf(v,"%d",m);

  gmetric = Ganglia_metric_create(context);
  if(!gmetric){
    printf("Ganglia_metric_create error\n");
    return(1);
  }

  r=Ganglia_metric_set(gmetric, "db_connect_count", v, "uint32", "connect", 3, 60, 0);
  if (r != 0) {
    switch(r){
    case 1:
      fprintf(stderr,"gmetric parameters invalid. exiting.\n");
    case 2:
      fprintf(stderr,"one of your parameters has an invalid character '\"'. exiting.\n");
    case 3:
      fprintf(stderr,"the type parameter is not a valid type. exiting.\n");
    case 4:
      fprintf(stderr,"the value parameter does not represent a number. exiting.\n");
    }
    Ganglia_metric_destroy(gmetric);
    return(0);
  }

  r=Ganglia_metric_send(gmetric, channel);
  Ganglia_metric_destroy(gmetric);
  if(r){
    fprintf(stderr,"There was an error sending to %d of the send channels.\n", r);
    return(0);
  }
  return(1);
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
      /*printf("%02d: state=%02d sport=%05d dport=%05d\n", r, msg->idiag_state, ntohs(msg->id.idiag_sport), ntohs(msg->id.idiag_dport));*/
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

  context = Ganglia_pool_create(NULL);
  if(!context){
    printf("Ganglia_pool_create error\n");
    return(1);
  }
  gconfig = Ganglia_gmond_config_create("/etc/ganglia/gmond.conf",0);
  if(!gconfig){
    printf("Ganglia_gmond_config_create\n");
    return(1);
  }
  channel = Ganglia_udp_send_channels_create(context, gconfig);
  if(!channel){
    printf("Ganglia_udp_send_channels_create error\n");
    return(1);
  }
  n = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
  if(n == -1){
    printf("Can't Open NetLink!!\n");
    return(1);
  }

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
    sleep(1);
    if(sigflag){
      if(!gsend(m))
        return(1);
      sigflag = 0;
      m = 0;
    }
  }
  return(0);
}

