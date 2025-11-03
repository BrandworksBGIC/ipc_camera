.PHONY:	all certtool fip

all: certtool fip
ifeq ($(wildcard ram_init/generate.sh),)
	@echo "ram_init/generate.sh does not exist, skipping execution."
else
	./ram_init/generate.sh
endif

certtool:
	@echo "Built cert_create ..."
	@echo "cert_create is already built"

fip:
	@echo "Built fip ..."
