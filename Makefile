hp:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/hp_main.c ./src/record.c ./src/hp_file.c -lbf -o ./build/hp_main -O2

bf:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c ./src/record.c -lbf -o ./build/bf_main -O2;

ht:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/ht_main.c ./src/record.c ./src/ht_table.c -lbf -o ./build/ht_main -O2
	
test:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main.c ./src/record.c ./src/hp_file.c ./src/ht_table.c -lbf -o ./build/main -O2;	

clean:
	rm -rf *.db

run_bf: bf
	./build/bf_main
	
run_hp: hp
	./build/hp_main	
	
run_ht: ht
	./build/ht_main		
	
run_test: test
	./build/main			
