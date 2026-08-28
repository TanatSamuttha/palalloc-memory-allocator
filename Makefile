testc:
	gcc test/test.c -o test -O0 -Isource
	./test

testcpp:
	g++ test/test.cpp -o test -O0 -Isource
	./test

