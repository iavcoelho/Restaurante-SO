CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O3 -g

PROJECT_ROOT = $(shell realpath ..)
BINARIES_DIR = $(PROJECT_ROOT)/bin

SUFFIX = $(shell getconf LONG_BIT)

CHEF         = semSharedMemChef
WAITER       = semSharedMemWaiter
GROUP        = semSharedMemGroup
RECEPTIONIST = semSharedMemReceptionist
MAIN         = probSemSharedMemRestaurant

OBJS = sharedMemory.o semaphore.o logging.o

.PHONY: all ct ct_ch all_bin \
	clean cleanall $(BINARIES_DIR)

all:		group         waiter      chef       receptionist     main clean
gr:		    group         waiter_bin  chef_bin   receptionist_bin main clean
wt:		    group_bin     waiter      chef_bin   receptionist_bin main clean
ch:		    group_bin     waiter_bin  chef       receptionist_bin main clean
rt:		    group_bin     waiter_bin  chef_bin   receptionist     main clean
all_bin:	group_bin     waiter_bin  chef_bin   receptionist_bin main clean

$(BINARIES_DIR):
	mkdir -p "$(BINARIES_DIR)"

chef:	$(CHEF).o $(OBJS)
	$(CC) -o "$(BINARIES_DIR)/$@" $^ -lm

waiter:		$(WAITER).o $(OBJS)
	$(CC) -o "$(BINARIES_DIR)/$@" $^

group:	$(GROUP).o $(OBJS)
	$(CC) -o "$(BINARIES_DIR)/$@" $^ -lm

receptionist:	$(RECEPTIONIST).o $(OBJS)
	$(CC) -o "$(BINARIES_DIR)/$@" $^ -lm

main:		$(MAIN).o $(OBJS)
	$(CC) -o "$(BINARIES_DIR)/$(MAIN)" $^ -lm

chef_bin: $(BINARIES_DIR)
	cp "$(BINARIES_DIR)/chef_bin_$(SUFFIX)" "$(BINARIES_DIR)/chef"

waiter_bin: $(BINARIES_DIR)
	cp "$(BINARIES_DIR)/waiter_bin_$(SUFFIX)" "$(BINARIES_DIR)/waiter"

group_bin: $(BINARIES_DIR)
	cp "$(BINARIES_DIR)/group_bin_$(SUFFIX)" "$(BINARIES_DIR)/group"

receptionist_bin: $(BINARIES_DIR)
	cp "$(BINARIES_DIR)/receptionist_bin_$(SUFFIX)" "$(BINARIES_DIR)/receptionist"

clean:
	rm -f *.o

cleanall:	$(BINARIES_DIR) clean
	rm -f "$(BINARIES_DIR)"/*

