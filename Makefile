.SUFFIXES : .x .o .c .s

include env.conf

ROOT=$(shell /bin/pwd)

#CC=gcc
#STRIP=strip

ifeq ($(DIR_PJSIP), )
DIR_PJSIP=$(ROOT)/..
endif

#test:
	@echo "======$(DIR_PJSIP)"
	@echo "======WORKDIR: $(WORKDIR) "
	@echo "======WORKDIR: ${WORKDIR} "
	@echo "======DIR_PREMAKE: $(DIR_PREMAKE)"
	@echo "======DIR_PREMAKE: ${DIR_PREMAKE}"
	@echo "========BUILD_PATH: $(BUILD_PATH)"
	@echo "========BUILD_PATH: ${BUILD_PATH}"
	@echo "========FINAL_PATH: $(FINAL_PATH)"
	@echo "========GBASE_LIB: $(GBASE_LIB)"
	@echo "========GBASE_INCLUDE: $(GBASE_INCLUDE)"
	@echo "========GOLBAL_CFLAGS: $(GOLBAL_CFLAGS)"
	@echo "========GOLBAL_CPPFLAGS: $(GOLBAL_CPPFLAGS)"
	@echo "========GOLBAL_LDFLAGS: $(GOLBAL_LDFLAGS)"
	@echo "========PLATFORM: $(PLATFORM)"
	@echo "========CC: $(CC)"

ifeq ($(PLATFORM), x86)
INSTALL_PJ=$(DIR_PJSIP)/final_x86
else ifeq ($(PLATFORM), nt966x)
INSTALL_PJ=$(DIR_PJSIP)/final_nt966x
else ifeq ($(PLATFORM), nt966x_d048)
INSTALL_PJ=$(DIR_PJSIP)/final_nt966x
endif

#INSTALL_PJ=$(DIR_PJSIP)/final_$(PLATFORM)

LIB_PJ = $(INSTALL_PJ)/lib
INC_PJ = $(INSTALL_PJ)/include
INC = $(ROOT)
INC_TPC = $(ROOT)/inc
#INC_SYS = /usr/include
#LIB_SYS = /usr/lib

#TPC_CFLAGS=-g -O0 -DUSE_ZLOG -I$(INC_PJ) -I$(INC1) -I$(INC_TPC) -I$(INC_SYS)
#WEC_LDFLAGS=-L$(GBASE_LIB) -L$(LIB_PJ)  
TPC_CFLAGS  = -g -O0 -DUSE_ZLOG -I$(INC_PJ) -I$(INC) -I$(INC_TPC) -DTPC_CONFIG_ANDROID=0 $(GOLBAL_CFLAGS) -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1
WEC_LDFLAGS = $(GOLBAL_LDFLAGS) -L$(LIB_PJ)

TARGET = libtpc.so
SRCS := tpc_endpoints.c  tpc_epoll.c  tpc_event.c  tpc_event_map.c  tpc_event_msg.c  tpc_event_thread.c  tpc_util.c ice_client.c

DEPEND_LIB_X86 = $(LIB_PJ)/libpjsua-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsip-ua-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjsip-simple-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjsip-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjmedia-codec-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjmedia-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjmedia-videodev-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjmedia-audiodev-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjmedia-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpjnath-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libpjlib-util-x86_64-unknown-linux-gnu.a  \
$(LIB_PJ)/libsrtp-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libresample-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libgsmcodec-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libspeex-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libilbccodec-x86_64-unknown-linux-gnu.a $(LIB_PJ)/libwebrtc-x86_64-unknown-linux-gnu.a \
$(LIB_PJ)/libpj-x86_64-unknown-linux-gnu.a

DEPEND_LIB_NT966X = $(LIB_PJ)/libpjsua-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjsip-ua-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libpjsip-simple-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjsip-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libpjmedia-codec-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjmedia-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libpjmedia-videodev-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjmedia-audiodev-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libpjmedia-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libpjnath-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libpjlib-util-mipsel-24kec-linux-uclibc.a  \
$(LIB_PJ)/libsrtp-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libresample-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libgsmcodec-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libspeex-mipsel-24kec-linux-uclibc.a \
$(LIB_PJ)/libilbccodec-mipsel-24kec-linux-uclibc.a $(LIB_PJ)/libpj-mipsel-24kec-linux-uclibc.a

ifeq ($(PLATFORM), x86)
DEPEND_LIB = $(DEPEND_LIB_X86)
else ifeq ($(PLATFORM), nt966x)
DEPEND_LIB = $(DEPEND_LIB_NT966X)
else ifeq ($(PLATFORM), nt966x_d048)
DEPEND_LIB = $(DEPEND_LIB_NT966X)
endif

LIB_ZLOG   = $(FINAL_PATH)/lib/libzlog.a
LIB_CRYPTO = $(FINAL_PATH)/lib/libcrypto.a
#STATIC_LIB += $(LIB_ZLOG) 
#STATIC_LIB += $(LIB_CRYPTO)

#LIBS= -lcrypto -lpthread -ldl -lm
LIBS= -lpthread -ldl -lm

all: 
	$(CC) -shared -fPIC $(TPC_CFLAGS) $(SRCS) -o $(TARGET) $(DEPEND_LIB)
#	$(CC) -shared -fPIC $(TPC_CFLAGS) $(WEC_LDFLAGS) $(SRCS) -o $(TARGET) $(STATIC_LIB) $(LIBS)
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

install:
	mkdir -p $(FINAL_PATH)/include/tpc/
	cp -RLf $(INC_TPC)/* $(FINAL_PATH)/include/tpc/
	cp -af $(TARGET)  $(FINAL_PATH)/lib
#	install -d $(FINAL_PATH)

