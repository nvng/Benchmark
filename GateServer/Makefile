ifeq ($(JOBS),)
	JOBS := $(shell grep -c ^processor /proc/cpuinfo 2>/dev/null)
	ifeq ($(JOBS),)
		JOBS := 1
	endif
endif

ifeq ($(release), y)
	CC = g++
	CC_FLAG = -DNDEBUG -std=c++2a -Wall -Werror -O2 -fno-omit-frame-pointer -Wno-unused-function -rdynamic
else
	CC = ccache g++
	CC_FLAG = -g -std=c++2a -Wall -Werror -O0 -Wno-unused-function -rdynamic -fno-omit-frame-pointer -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector
endif

# {{{ project info
INC_DIR	= . .. ./src \
            ../Deps/libnvngLib/include/ActorFramework/Impl/Share \
            ../Deps/libnvngLib/include/ActorFramework/Impl/Gate

SRC_DIR   = ./src \
            ../Deps/libnvngLib/include/ActorFramework/Impl/Share \
            ../Deps/libnvngLib/include/ActorFramework/Impl/Gate

TARGET	= GateServer.out

SRC_EXTS = .c .cc
HDR_EXTS = .h .hpp

SRC	= $(foreach d, $(SRC_DIR), $(wildcard $(addprefix $(d)/*, $(SRC_EXTS))))
# HEADERS = $(foreach d, $(SRC_DIR), $(wildcard $(addprefix $(d)/*, $(HDR_EXTS))))

INC	+= $(addprefix -I, $(INC_DIR)) # 等价于下面一行
# INC	= $(foreach d, $(INC_DIR), $(addsuffix $(d), -I))

# OBJS    += $(addprefix $(basename $(SRC)), .o) # 只给最后一个加，前面的都没加 why???
OBJS	+= $(foreach s, $(SRC), $(addprefix $(basename $s), .o))
DEPS	+= $(foreach s, $(SRC), $(addprefix $(basename $(s)), .d))

RM	+= $(OBJS) $(TARGET)
RM_ALL  += $(DEPS)
# }}}

# {{{ dependency includes and libs
EXT_DIR = ../Deps/libnvngLib \
          ../Deps/libboost

EXT_LIB = nvngLib fmt protobuf pthread \
          boost_program_options boost_filesystem boost_stacktrace_backtrace \
          event event_core event_extra event_openssl event_pthreads \
          ssl crypto libgo dl jemalloc mysqlcppconn hiredis atomic z lz4 zstd

LIB_DIR = $(foreach d, $(EXT_DIR), $(addprefix -L, $(d)/lib))
INC	+= $(foreach d, $(EXT_DIR), $(addprefix -I, $(d)/include))
LIB	+= $(addprefix -l, $(EXT_LIB))
# }}}

#{{{ protobuf
PROTO_DIR = ../Proto
ifneq ($(PROTO_DIR),)
	PROTO_PROTO = $(foreach d, $(PROTO_DIR), $(wildcard $(addprefix $(d)/*, .proto)))
	PROTO_CC = $(PROTO_PROTO:.proto=.pb.cc)
	PROTO_H  = $(PROTO_PROTO:.proto=.pb.h)
	OBJS	+= $(PROTO_PROTO:.proto=.pb.o)
	INC	+= $(addprefix -I, $(PROTO_DIR))

	RM_ALL += $(PROTO_CC) $(PROTO_H)
	
	ADDED_DEPS += $(PROTO_CC)
endif
#}}}

#{{{ precompiled headers
PCH	  = ./src/PreCompile.h
ifneq ($(PCH),)
	INC    += -include $(PCH)
	PCH_OBJ = $(foreach f, $(PCH), $(addsuffix .gch, $(basename $(f))))

	# 这里可以认为是生成.d时，需要*.gch，所以就会生成相应的文件
	# DEPS    += $(PCH_OBJ:.gch=.d)
	
	RM_ALL += $(PCH_OBJ)
endif
#}}}

#{{{ compile rules
COMPILE_CMD = $(CC) $(CC_FLAG) $(INC)

.PHONY	: all show c cl release db j

# 虽然生成依赖的时候就生成了 PROTO_CC，而如果这里不写，删除之前生成的。
# 然后根据 TARGET 的依赖 OBJS 生成 PROTO_CC
all : $(PROTO_CC) $(PCH_OBJ) $(TARGET)

# $(COMPILE_CMD) -o $(TARGET) $(OBJS) $(LIB_DIR) -static $(LIB)
$(TARGET) : $(OBJS)
	$(COMPILE_CMD) -o $(TARGET) $(OBJS) $(LIB_DIR) $(LIB)

MAKE_OBJ_CMD = $(COMPILE_CMD) -o $@ -c $<
MAKE_DEP_CMD = @echo create dep $@; \
	       set -e; \
	       $(COMPILE_CMD) -MM $< | sed 's,\($(notdir $*)\)\.o[ :]*,$*.o $*.d : ,g' > $@

%.o : %.cpp $(ADDED_DEPS) ; $(MAKE_OBJ_CMD)
%.o : %.cc  $(ADDED_DEPS) ; $(MAKE_OBJ_CMD)
%.o : %.c   $(ADDED_DEPS) ; $(MAKE_OBJ_CMD)

%.d : %.cpp $(ADDED_DEPS) ; $(MAKE_DEP_CMD)
%.d : %.cc  $(ADDED_DEPS) ; $(MAKE_DEP_CMD)
%.d : %.c   $(ADDED_DEPS) ; $(MAKE_DEP_CMD)

# for protobuf
%.pb.cc : %.proto
	protoc -I=$(dir $<) --cpp_out=$(dir $<) $<

%.pb.d : %.pb.cc
	$(MAKE_DEP_CMD)

# for precompiled header
%.gch : %.h $(ADDED_DEPS)
	$(COMPILE_CMD) -x c++-header -o $@ $<

%.d : %.gch
	@echo create dependency $@; \
	set -e; \
	$(COMPILE_CMD) -MM $*.h | sed 's,\($(notdir $*)\)\.o[ :]*,$*.gch $*.d : ,g' > $@

# include ./src/proto/Makefile # 在最后执行
sinclude $(DEPS) # 在最先执行

j :
	make -j$(JOBS)

db :
	@make cl; \
        compiledb -n make;

release :
	make -j$(JOBS) release=y;

show :
	@echo "TARGET	:" $(TARGET)
	@echo "SRC_DIR	:" $(SRC_DIR)
	@echo "EXT_DIR	:" $(EXT_DIR)
	@echo "PROTO_DIR :" $(PROTO_DIR)
	@echo "INC :" $(INC)
	@echo "EXT_LIB :" $(EXT_LIB)
	@echo "LIB_DIR :" $(LIB_DIR)
	@echo "JOBS :" $(JOBS)

cl : c
	-rm -f $(RM_ALL) 1 .clang_complete

c :
	-rm -f $(RM)

#}}}

#{{{ help note

# .PHONY 定义伪目录
#
# -rm	表示也许某些文件出现问题，继续做后面的事
#
# =	是最基本的赋值
# :=	是覆盖之前的值
# ?=	是如果没有被赋值过就赋予等号后面的值
# +=	是添加等号后面的值
#
# $?	比目标的修改时间更晚的那些依赖模块表
# $@	当前目标的全路径名。可用于用户定义的目标名的相关行中
# $^	所有的依赖文件
# $<	比给定的目标文件时间标记更新的依赖文件名
# $*	去掉后缀的当前目标名。例如，若当前目标是pro.o，则$*表示pro
# }}}

# vim: fenc=utf8:expandtab:ts=8:
