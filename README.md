# CS345Project
These are the files associated with my Operating Systems Design class. We implemented a number of projects by adding to the starter code given to us by the professor.
The program is a shell with various commands and key presses that do particular things. Commands "p1" through "p6" run the various projects to completion. 
This is an explanation of each project:

P1: Here, I implemented the shell that would run all the other projects. It deals with handling tasks, which can run in either the foreground and background. It takes
commands from the command line and performs the apporpiate operations. It also handles signals sent by certain key presses. There's also functionality to edit the
command line and recall previously executed commands. Commands in quotations are treated as a single string. I dealt with c strings extensively here. The mapping
for particular commands/shortcuts and their associated functions can be found in p1.c

P2: For this project, I implmeneted a priority queue for the scheduler and dispatcher that would execute tasks based on their priority. Multiple programs can run in 
the background this way

P3: This project dealt heavily with semaphores and mutexes to implement concurrent programming. Running p3 starts a simulation of Jurrasic Park, where the park has a limited number of tickets, workers,
cars to tour the park, and space within shops. Numerous tourists come through the park accessing each attraction in order. The binary semaphores work on these objects
to signal needs and allow the objects to coordinate. The counting semaphores offer resource management such as handing out a limited number of tickets. The mutexes 
(which are also implemented using binary semaphores) handle access control so that different tasks don't alter resources simultaneously, which would cause race conditions and 
almost certainly break the program. 

P4:

P5:

P6:
