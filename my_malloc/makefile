SRC_DIR = src
GS_DIR = /home/cosc360/lab6/gs

all: ga

.PHONY: gs ga clean

gs: clean $(PROG)
	$(GS_DIR)/gradescript $(test)

ga: clean $(PROG)
	$(GS_DIR)/gradeall

clean:
	rm -rf tmp-???-test-stdout.txt tmp-???-test-stderr.txt a.out test-???.c
