ifeq ($(PARAM_FILE), )
	PARAM_FILE := ../Makefile.param
	include $(PARAM_FILE)
endif

CFLAGS += $(addprefix -I,$(INC_DIR))
DEP_LIBS += $(addprefix -L,$(LIB_DIR)) -Wl,-Bstatic -lrtstream -lrtsisp -lh1encoder\
	    -lrtsjpeg -lrtsosd -lrtsamixer -lrtsv4l2 -lrtscamkit\
	    -lrtsacodec -lrtsaec -lrtsmp3 -lopencore-amrnb -lrtsaac -lsbc -lopus\
	    -Wl,-Bdynamic -lasound -lpthread -lm
LDLIBS += $(DEP_LIBS)

SRCS:=$(wildcard *.c)
OBJS:=$(basename $(SRCS))

.PHONY: all

all:$(OBJS)

$(OBJS):%:%.c
	@echo "compile" $@
	@$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)


clean:
	@rm -f *.o $(OBJS)
