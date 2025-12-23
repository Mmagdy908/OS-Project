build:
	gcc process_generator.c -o process_generator.out
	gcc clk.c -o clk.out
	gcc -c Memory/mmu.c -o Memory/mmu.o -I.
	gcc scheduler.c Memory/mmu.o -o scheduler.out -lm
	gcc process.c -o process.out
	gcc test_generator.c -o test_generator.out

clean:
	rm -f *.out *.txt Memory/*.o *.o *.log

all: clean build

run:
	./process_generator.out