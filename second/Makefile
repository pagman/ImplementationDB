hp:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/hp_main.c ./src/record.c ./src/hp_file.c -lbf -o ./build/hp_main -O2

bf:
	@echo " Compile bf_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/bf_main.c ./src/record.c -lbf -o ./build/bf_main -O2;

ht:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/ht_main.c ./src/record.c ./src/ht_table.c -lbf -o ./build/ht_main -O2

sht:
	@echo " Compile hp_main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/sht_main.c ./src/record.c ./src/sht_table.c ./src/ht_table.c -lbf -o ./build/sht_main -O2

	
test_1:
	@echo " Compile test 1 main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main_1.c ./src/record.c ./src/hp_file.c ./src/ht_table.c -lbf -o ./build/main_1 -O2;	

test_2:
	@echo " Compile test 2 main ...";
	gcc -I ./include/ -L ./lib/ -Wl,-rpath,./lib/ ./examples/main_2.c ./src/record.c ./src/hp_file.c ./src/ht_table.c ./src/sht_table.c -lbf -o ./build/main_2 -O2;	
	
run_bf: bf
	./build/bf_main
	
run_hp: hp
	./build/hp_main	
	
run_ht: ht
	./build/ht_main	
		
run_sht: sht
	./build/sht_main		
	
run_test_1: test_1
	./build/main_1
	
run_test_2: test_2
	./build/main_2		

clean:
	rm -rf *.db
