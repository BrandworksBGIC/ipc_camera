PARAM_FILE := ./Makefile.param
include $(PARAM_FILE)

exclude_dirs = font/
dirs := $(shell ls -d */)
dirs := $(filter-out $(exclude_dirs), $(dirs))

SUBDIRS := $(dirs)

.PHONY: all

all: subdirs

subdirs: $(SUBDIRS)
	for dir in $(SUBDIRS);\
		do $(MAKE) -C $$dir || exit 1;\
	done

clean:
	for dir in $(SUBDIRS);\
		do $(MAKE) -C $$dir clean || exit 1;\
	done


