PROG = tarc tarx
SRC_DIR = src

LIBFDR = /home/cosc360/libfdr
GS_DIR = /home/cosc360/lab4/gs

CC = gcc
INCLUDES = -I$(LIBFDR)/include/
CFLAGS = -g -Werror -Wall -MD -std=gnu99 $(INCLUDES)

SRC = $(wildcard $(SRC_DIR)/*.c)
PROG_OBJ = $(patsubst %, $(SRC_DIR)/%.o, $(PROG))
OBJ = $(filter-out $(PROG_OBJ), $(SRC:.c=.o))
LIBS = $(LIBFDR)/lib/libfdr.a

define PROG_RULE
$(1): $(OBJ) $(patsubst %, $(SRC_DIR)/%.o, $(1))
	$(CC) $(CFLAGS) -o $(1) $(OBJ) $(patsubst %, $(SRC_DIR)/%.o, $(1)) $(LIBS)
endef

all: $(PROG)

$(foreach _prog, $(PROG), $(eval $(call PROG_RULE, $(_prog))))

#$(PROG): $(OBJ) | $(PROG_OBJ)
#	$(CC) $(CFLAGS) -o $@ $^ $(SRC_DIR)/$@.o $(LIBS)

.PHONY: gs-a ga-a gs-b ga-b clean

gs-a: $(PROG)
	$(GS_DIR)/gradescript-5a $(test)

ga-a: $(PROG)
	$(GS_DIR)/gradeall-5a

gs-b: $(PROG)
	$(GS_DIR)/gradescript-5b $(test)

ga-b: $(PROG)
	$(GS_DIR)/gradeall-5b

clean:
	rm -rf $(PROG) $(OBJ) $(PROG_OBJ) $(OBJ:.o=.d) $(PROG_OBJ:.o=.d) tmp-* 0*

-include $(OBJ:.o=.d)
