CC = g++
CFLAGS := -Wall -O3 -std=c++0x 

AOCL_COMPILE_CONFIG := $(shell aocl compile-config )
OPENCL_INC_ROOT := -I. -I./AOCLUtils -I../common $(AOCL_COMPILE_CONFIG)

OBJS = zstd_opencl_mgr.o AOCLUtils/opencl.o AOCLUtils/options.o
LIB = libzstdOpenclMgr.a

OPENCL_INC = $(OPENCL_INC_ROOT)

INCLUDE_PATH = $(OPENCL_INC)

all : $(LIB)

%.o : %.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDE_PATH)

$(LIB) : $(OBJS)
	rm -f $@
	ar cr $@ $(OBJS)

#tags :
#	ctags -R *

clean:
	rm -f $(OBJS) $(TARGET) $(LIB)
