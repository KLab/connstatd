CC=gcc

# make GDIR=/usr/klab/src/ganglia-3.0.5
GDIR=..

all: connstatd.c
	echo $(FIXED_POOL_LEAK)
	$(CC) -g $(MY_FLAGS) -I/usr/src/linux/include -I$(GDIR)/include -I$(GDIR)/lib -I$(GDIR)/gmond -I$(GDIR)/libmetrics -o connstatd connstatd.c $(GDIR)/lib/.libs/libganglia.a $(GDIR)/lib/libgetopthelper.a /usr/lib/libapr-1.a /usr/lib/libconfuse.a -lrt

clean:
	rm -f connstatd
