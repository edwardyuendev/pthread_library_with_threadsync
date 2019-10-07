# User Mode Thready Library WITH Thread Synchronization and Safety
This is a extension of the basic User Mode Thread Library that I wrote here:
https://github.com/edwardyuendev/thread_library_no_thread_safety

# The Goals of this Project were to:
1. enable multiple threads to safely coexist without causing unintentional interactions between threads
2. extend our basic user-mode thread library with synchronization primitives (lock and semaphores)
3. learn about thread safety, mutual exclusion, critical sections, and etc,.
4. improve mastery of the C++ language

In this project, I added additional functionality on top of my basic Threads library. This includes:
1. adding locking mechanisms to the library that can prevent thread switching
2. implementing pthread_join() to allow threads to wait on a specific thread
3. adding semaphore support by implementing functions from semaphore.h
   - this includes: sem_init(), sem_destroy(), sem_wait(), and sem_post()
4. significant changes to pthread_create() and pthread_exit() to work with the new added functionality
5. changes in the scheduling function

# How To Run
1. Make sure both the Makefile and pthread.cpp is in the same directory
2. Type "make" to generate the object files
3. Include pthread.h in your application and you are now able to utilize basic threads

# Restrictions: 
- The thread library was designed to work specifically on Linux (Fedora OS)
- The thread library includes pointer mangling for the purposes of running/testing the thread library on lab Linux computers
