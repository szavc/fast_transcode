STDIR   = $(prefix)/usr/bin
INSTMODE  = 0755
INSTOWNER = root
INSTGROUP = root

CFLAGS = -Wall -O3 -D__STDC_CONSTANT_MACROS -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 
#CFLAGS += -DDEBUG -DPERF_DEBUG
CFLAGS += -L${STAGING_DIR}/usr/lib -I${STAGING_DIR}/usr/include

CXXFLAGS += -std=c++11

LDFLAGS += -lpthread -ldl -lm -lz -laupera -laupcodec -lyuv -lavdevice -lavfilter -lavformat -lavcodec -lpostproc \
         -lswresample -lswscale -lavutil -lx264 -lfdk-aac

APP = transcode_test

SRCS = 		transcode_init.c \
		vfilter.c \
		afilter.c  \
		transcode_job.c \
		transcode_test.c 

PREFIX =  $(patsubst %.c,%, $(SRCS) )
OBJS =  $(patsubst %,%.o, $(PREFIX) )

%.o: %.c
	$(CXX) -c $(CFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $(CXXFLAGS)  $^ -o $@

all: $(OBJS) 
	$(CXX) $(CFLAGS) $(OBJS)  -o $(APP) $(LDFLAGS) 

	$(STRIP) $(APP)

install: 
	install -m 755 $(APP) $(STAGING_DIR)/usr/bin

clean:
	rm -f $(APP)  *.o

