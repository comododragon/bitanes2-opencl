GENERALFLAGS=-fPIC -DCOMMON_COLOURED_PRINTS -Iinclude/common -Iinclude
AOCLFLAGS=`aocl compile-config` `aocl link-config`

ifeq ($(USEDOUBLE),yes)
    FPFLAG=-DFP_T=double
else
    FPFLAG=-DFP_T=float
endif

cpu/execute: include/common/common.h include/brandes.h src/brandes.cl src/host.c obj/graph.o
	mkdir -p cpu
	cd cpu; ln -sf ../src/brandes.cl brandes.cl
	cd cpu; ln -sf ../include/brandes.h brandes.h
	$(CC) src/host.c obj/graph.o -o cpu/execute -DTARGET_CPU $(FPFLAG) $(GENERALFLAGS) -lOpenCL

fpga/emu/emulate: include/common/common.h include/brandes.h src/host.c obj/graph.o fpga/emu/program.aocx
	$(CC) src/host.c obj/graph.o -g -o fpga/emu/emulate $(FPFLAG) $(GENERALFLAGS) $(AOCLFLAGS)

fpga/emu/program.aocx: src/brandes.cl include/brandes.h
	mkdir -p fpga/emu
	aoc -v -march=emulator -g $(FPFLAG) --board s5phq_a7 -Iinclude src/brandes.cl -o fpga/emu/program.aocx

fpga/bin/execute: include/common/common.h include/brandes.h src/host.c obj/graph.o fpga/bin/program.aocx
	$(CC) src/host.c obj/graph.o -o fpga/bin/execute $(FPFLAG) $(GENERALFLAGS) $(AOCLFLAGS)

fpga/bin/program.aocx: src/brandes.cl include/brandes.h
	mkdir -p fpga/bin
	aoc -v $(FPFLAG) --board s5phq_a7 -Iinclude src/brandes.cl -o fpga/bin/program.aocx

obj/graph.o: src/graph.c include/graph.h
	mkdir -p obj
	$(CC) -c src/graph.c -o obj/graph.o $(GENERALFLAGS) -lm

clean:
	rm -rf fpga cpu obj
