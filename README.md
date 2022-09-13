# Operating Systems Design
These are the files associated with my Operating Systems Design class. We implemented a number of projects by adding to the starter code given to us by the professor.
The program is a shell with various commands and key presses that do particular things. Commands "p1" through "p6" run the various projects to completion. 
This is an explanation of each project:

P1: Here, I implemented the shell that would run all the other projects. It deals with handling tasks, which can run in either the foreground or background. It takes
commands from the command line and performs the appropiate operations. It also handles signals sent by certain key presses. There's also functionality to edit the
command line and recall previously executed commands. Commands in quotations are treated as a single string. I dealt with c strings extensively here, as well as dynamically allocating memory to the heap. The mapping for particular commands/shortcuts and their associated functions can be found in p1.c

P2: For this project, I replaced the simplistic 2-state scheduler with a 5-state, preemptive, prioritized, round-robin scheduler using ready and blocked task queues.
Individual tasks may be terminated and resources recovered. We delved into counting semaphores to demonstrate multitasking in the operating system. 

P3: This project dealt heavily with semaphores and mutexes to implement concurrent programming. Running p3 starts a simulation of Jurrasic Park, where the park has a limited number of tickets, workers, cars to tour the park, and space within shops. Numerous tourists come through the park accessing each attraction in order. The binary semaphores work on these objects to signal needs and allow the objects to coordinate. The counting semaphores offer resource management such as handing out a limited number of tickets. The mutexes (which are also implemented using binary semaphores) handle access control so that different tasks don't alter resources simultaneously, which would cause race conditions and/or deadlock, almost certainly breaking the program. This is how an operating system allows multiple programs to run at once, accessing the same resources, waiting for resources to become available, waiting for other tasks to finish executing, etc., all without breaking the program. Very cool. 

P4: I implemented the extremely complicated virtual memory system, which stores and swaps out data in page tables. The code I implemented is in the mmu.c file. I used a double clock algorithm to efficiently swap appropriate pages out of memory and into swap space (secondary memory), or from swap space back into usable memory. The double paging system (root page tables and user page tables) allows more physical memory to map to virtual memory addresses. This project deals heavily with inspecting individual bits to know how to handle everything. The program also tracks the amount of accesses, hits, faults, page reads, page writes, and swap pages. This was the most difficult project of the class, and explaining each detail that went into it would be difficult in a short time. 

P5: This project is an implementation of a second scheduler, which can be toggled on and off. The new scheduler has fair-share functionality, which allocates computations evenly amongst users or groups. Computations are also evenly divided amongst users within groups. 

P6: I implemented a FAT-12 file system here, which allows us to mount a disk and then open, close, define, delete, read, write, and seek files on the mounted disk. This was the second hardest project of the class and involved a lot of error checking and tedious data handling. 
