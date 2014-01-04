# Record&Replay Project 

##Commandlines:
###Record:
* `../../../pin -t obj-intel64/MyPinTool.so -o cond -mode record -- ./test2`
###Replay:
* `../../../pin -t obj-intel64/MyPinTool.so -o cond -mode replay -count 6 -- ./test2`
###Tests:
* `gcc -o test testApp.c -lpthread` 
* `gcc -o test2 testCond.c -lpthread` 
