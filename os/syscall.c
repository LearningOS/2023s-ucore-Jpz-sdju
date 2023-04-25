#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"
void dump_for_syscall_num(int syscall_index){
	struct proc *s = curr_proc();
	s->info.syscall_times[syscall_index] += 1;
}
uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

uint64 sys_gettimeofday(TimeVal *val, int _tz) // TODO: implement sys_gettimeofday in pagetable. (VA to PA)
{
	struct proc *p = curr_proc();
	TimeVal dst;

	/* The code in `ch3` will leads to memory bugs*/
	uint64 cycle = get_cycle();
	dst.sec = cycle / CPU_FREQ;
	dst.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;

	copyout(p->pagetable, (uint64)val, (char *)(&dst), sizeof(TimeVal));

	return 0;
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
        struct proc *p = curr_proc();
        addr = p->program_brk;
        if(growproc(n) < 0)
                return -1;
        return addr;	
}


int mmap(void* start, unsigned long long len, int port, int flag, int fd){
	//checking addr aligning
	if((uint64 )start % 4096 != 0){
		panic("do not align!");
	}
	if(len % 4096 != 0 ){
		panic("length do not align!");
	}
	int perm = (((port & 0x1 )<< 0) | ((port & 0x2) << 1) | ((port & 0x4) << 2)) << 1;
	if(!mappages(curr_proc()->pagetable, (uint64)start, len, (uint64)kalloc(), perm) ){
		return -1;
	}
	return 0;
}

int munmap(void* start, unsigned long long len){
	uvmunmap(curr_proc()->pagetable, (uint64)start, len%4096, 0);
	return 0;

}
// TODO: add support for mmap and munmap syscall.
// hint: read through docstrings in vm.c. Watching CH4 video may also help.
// Note the return value and PTE flags (especially U,X,W,R)
/*
* LAB1: you may need to define sys_task_info here
*/
int sys_task_info(TaskInfo *ti){
	TaskInfo dst ;
	struct proc *p = curr_proc();
	dst.status = Running;
	memmove( (void *)(dst.syscall_times), (void *)(curr_proc()->info.syscall_times), sizeof(dst.syscall_times));
	printf("%d g cycle\n",get_cycle());
	printf("%d c- infotime\n",curr_proc()->info.time);
	dst.time =  (get_cycle()- curr_proc()->info.time) * 1000/ CPU_FREQ ;
	copyout(p->pagetable, (uint64)ti, (char *)(&dst), sizeof(dst));
	return 0;
}
extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
	/*
	* LAB1: you may need to update syscall counter for task info here
	*/
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		dump_for_syscall_num(SYS_write);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		dump_for_syscall_num(SYS_exit);

		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		dump_for_syscall_num(SYS_sched_yield);
		break;
	case SYS_gettimeofday:
		dump_for_syscall_num(SYS_gettimeofday);
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		dump_for_syscall_num(SYS_sbrk);
		break;
	case SYS_task_info:
		dump_for_syscall_num(SYS_task_info);
		ret = sys_task_info((TaskInfo *)args[0]);
		break;
	case SYS_mmap:
		ret = mmap((void *)args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_munmap:
		ret = munmap((void *)args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
