default:threads.o

autograder_main.o: autograder_main.c
	g++ -c -o autograder_main.o autograder_main.c

p3_grader: autograder_main.o threads.o
	g++ -o p3_grader autograder_main.o threads.o

threads.o: pthread.cpp
	g++ -c pthread.cpp -o threads.o

clean:
	rm -f p3_grader *.o
