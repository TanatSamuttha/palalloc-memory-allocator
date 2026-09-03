testc:
	gcc Test/test.c -o test -O0 -Isource
	./test

testcpp:
	g++ Test/test.cpp -o test -O0 -Isource
	./test

buildbenchmark:
	g++ Benchmark/benchmark.cpp -o benchmark -O3 -Isource
	./benchmark