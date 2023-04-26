1.refator sys_taskinfo and getsystime functions,the key of code is copyin and copyout,because the Timeval and TaskInfo struct ptr is passed from user program.it represent virtual address.



2.mmap and munmap function,the  key of implementation is length judgement and comprehension of vm.c functions.




