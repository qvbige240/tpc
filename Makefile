.SUFFIXES : .x .o .c .s

include env.conf

ROOT=$(shell /bin/pwd)

DIR_PJSIP=$(ROOT)/..

INSTALL_PJ=$(DIR_PJSIP)/final_x86
LIB_PJ = $(INSTALL_PJ)/lib
LIB_ZLOG = $(ROOT)/lib/x86
LIB_SYS = /usr/lib
INC_PJ = $(INSTALL_PJ)/include
INC1 = $(ROOT)
INC2 = $(ROOT)/inc
INCSYS = /usr/include

CC=gcc -g -O0 -DUSE_ZLOG -I$(INC_PJ) -I$(INC1) -I$(INC2) -I$(INCSYS)
WEC_LDFLAGS=-L$(LIB_PJ) -L$(LIB_ZLOG) -L$(LIB_SYS)
STRIP=strip

TARGET = libtpc.so
SRCS := tpc_endpoints.c  tpc_epoll.c  tpc_event.c  tpc_event_map.c  tpc_event_msg.c  tpc_event_thread.c  tpc_util.c ice_client.c

STATIC_LIB = $(LIB_PJ)/libpjsua-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsip-ua-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjsip-simple-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsip-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjmedia-codec-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjmedia-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjmedia-videodev-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjmedia-audiodev-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjmedia-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjnath-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjlib-util-x86_64-unknown-linux-gnu.a  \
$(LIB_PJ)/libsrtp-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libresample-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libgsmcodec-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libspeex-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libilbccodec-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libwebrtc-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpj-x86_64-unknown-linux-gnu.a
STATIC_LIB += $(LIB_ZLOG)/libzlog.a

LIBS= -lcrypto -lpthread -ldl -lm

all: 
	$(CC) -shared -fPIC $(WEC_LDFLAGS) $(SRCS) -o $(TARGET) $(STATIC_LIB) $(LIBS)
#	$(STRIP) $(TARGET) 

clean:
	rm -f *.o 
	rm -f *.x 
	rm -f *.flat
	rm -f *.map
	rm -f temp
	rm -f *.img
	rm -f $(TARGET)	
	rm -f *.gdb
	rm -f *.bak
