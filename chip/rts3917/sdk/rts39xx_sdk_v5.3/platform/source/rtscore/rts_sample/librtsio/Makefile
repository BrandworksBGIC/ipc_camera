ifeq ($(PARAM_FILE), )
	PARAM_FILE := ../Makefile.param
	include $(PARAM_FILE)
endif

CFLAGS += $(addprefix -I,$(INC_DIR))
DEP_LIBS += $(addprefix -L,$(LIB_DIR)) -Wl,-Bstatic -lrtsio -lrtsvideo -lrtstream \
	    -Wl,-Bdynamic -lpthread
LDLIBS += $(DEP_LIBS)

SRCS:=$(wildcard *.c)
OBJS:=$(basename $(SRCS))

.PHONY: all

all:$(OBJS)

$(OBJS):%:%.c
	@echo "compile" $@
	@$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)


clean:
	@rm -f $(OBJS) *.o
