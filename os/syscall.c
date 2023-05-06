#include "syscall.h"
#include "console.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
/*===========================start=================================*/
#include "file.h"
/*=================================================================*/
uint64 console_write(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	tracef("write size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return len;
}

uint64 console_read(uint64 va, uint64 len)
{
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	tracef("read size = %d", len);
	for (int i = 0; i < len; ++i) {
		int c = consgetc();
		str[i] = c;
	}
	copyout(p->pagetable, va, str, len);
	return len;
}

uint64 sys_write(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_write(va, len);
	case FD_INODE:
		return inodewrite(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
}

uint64 sys_read(int fd, uint64 va, uint64 len)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d\n", fd);
		return -1;
	}
	switch (f->type) {
	case FD_STDIO:
		return console_read(va, len);
	case FD_INODE:
		return inoderead(f, va, len);
	default:
		panic("unknown file type %d\n", f->type);
	}
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

uint64 sys_gettimeofday(uint64 val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal t;
	t.sec = cycle / CPU_FREQ;
	t.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	copyout(p->pagetable, val, (char *)&t, sizeof(TimeVal));
	return 0;
}

uint64 sys_getpid()
{
	return curr_proc()->pid;
}

uint64 sys_getppid()
{
	struct proc *p = curr_proc();
	return p->parent == NULL ? IDLE_PID : p->parent->pid;
}

uint64 sys_clone()
{
	debugf("fork!");
	return fork();
}

static inline uint64 fetchaddr(pagetable_t pagetable, uint64 va)
{
	uint64 *addr = (uint64 *)useraddr(pagetable, va);
	return *addr;
}

uint64 sys_exec(uint64 path, uint64 uargv)
{
	struct proc *p = curr_proc();
	char name[MAX_STR_LEN];
	copyinstr(p->pagetable, name, path, MAX_STR_LEN);
	uint64 arg;
	static char strpool[MAX_ARG_NUM][MAX_STR_LEN];
	char *argv[MAX_ARG_NUM];
	int i;
	for (i = 0; uargv && (arg = fetchaddr(p->pagetable, uargv));
	     uargv += sizeof(char *), i++) {
		copyinstr(p->pagetable, (char *)strpool[i], arg, MAX_STR_LEN);
		argv[i] = (char *)strpool[i];
	}
	argv[i] = NULL;
	return exec(name, (char **)argv);
}

uint64 sys_wait(int pid, uint64 va)
{
	struct proc *p = curr_proc();
	int *code = (int *)useraddr(p->pagetable, va);
	return wait(pid, code);
}

uint64 sys_openat(uint64 va, uint64 omode, uint64 _flags)
{
	struct proc *p = curr_proc();
	char path[200];
	copyinstr(p->pagetable, path, va, 200);
	return fileopen(path, omode);
}

uint64 sys_close(int fd)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d", fd);
		return -1;
	}
	fileclose(f);
	p->files[fd] = 0;
	return 0;
}

struct Stat {
   uint64 dev;     // 文件所在磁盘驱动号，该实现写死为 0 即可。
   uint64 ino;     // inode 文件所在 inode 编号
   uint32 mode;    // 文件类型
   uint32 nlink;   // 硬链接数量，初始为1
   uint64 pad[7];  // 无需考虑，为了兼容性设计
};

// 文件类型只需要考虑:
#define DIR 0x040000              // directory
#define FILE 0x100000             // ordinary regular file

int sys_fstat(int fd, uint64 stat)
{
	if (fd < 0 || fd > FD_BUFFER_SIZE)
		return -1;
	struct proc *p = curr_proc();
	struct file *f = p->files[fd];
	if (f == NULL) {
		errorf("invalid fd %d", fd);
		return -1;
	}

	struct Stat tmp_stat;
	tmp_stat.dev = 0;
	tmp_stat.ino = f->ip->inum;
	tmp_stat.mode = f->ip->type == T_FILE ? FILE : DIR;
	tmp_stat.nlink = f->ip->nlink;
	copyout(p->pagetable,(uint64)stat,(char *)&tmp_stat,sizeof(struct Stat));
// 	{
//    uint64 dev,     // 文件所在磁盘驱动号，该实现写死为 0 即可。
//    uint64 ino,     // inode 文件所在 inode 编号
//    uint32 mode,    // 文件类型
//    uint32 nlink,   // 硬链接数量，初始为1
//    uint64 pad[7],  // 无需考虑，为了兼容性设计
// }

	return 0;
}
/*===========================start=================================*/
int sys_linkat(int olddirfd, uint64 oldpath, int newdirfd, uint64 newpath,
	       uint64 flags)
{
	struct proc *p = curr_proc();
	char path1[200];char path2[200];
	copyinstr(p->pagetable, path1, oldpath, 200);
	copyinstr(p->pagetable, path2, newpath, 200);

	return create_hlink(path1,path2);
}

int sys_unlinkat(int dirfd, uint64 name, uint64 flags)
{
	struct proc *p = curr_proc();
	char path[200];;
	copyinstr(p->pagetable, path, name, 200);
	return remove_hlink(path);
}
/*=================================================================*/
uint64 sys_sbrk(int n)
{
	uint64 addr;
	struct proc *p = curr_proc();
	addr = p->program_brk;
	if (growproc(n) < 0)
		return -1;
	return addr;
}
/*=========================================================*/
uint64 sys_task_info(TaskInfo* info){
	TaskInfo k_info;
	struct proc *p = curr_proc();
	uint64 current_time_ms = get_time_now();

	k_info.status = Running;
	k_info.time = (int)(current_time_ms - p->time);
	memmove(k_info.syscall_times,p->syscall_times,sizeof(info->syscall_times));
	copyout(p->pagetable,(uint64)info,(char *)&k_info,sizeof(TaskInfo));
	return 0;
}

// this syscall handles va unmap and pa free when map failed
uint64 sys_mmap(uint64 start, uint64 len, int port, int flag, int fd){

	// fall early

	if((port & ~0x7) != 0){
		errorf("mmap:port contain dirty bits");
		return 0-1;
	}
	if((port & 0x7) == 0 ){
		errorf("mmap:request non R or W or X memory is meaningless");
		return -1;
	}
	if(!PGALIGNED(start)){
		errorf("mmap:start address is not page aligned");
		return -1;
	}
	if(len == 0){
		return 0;
	}
	if(len > 1024 * 1024 * 1024){
		errorf("mmap:too large");
		return -1;
	}

	// do the job

	int perm = PTE_U;
	if(port & 0b1u ){
		perm |= PTE_R;
	}
	if(port & 0b10u ){
		perm |= PTE_W;
	}
	if(port & 0b100u ){
		perm |= PTE_X;
	}
	perm &= (0b11110u);

    struct proc *p = curr_proc();

	int num = PGROUNDUP(len) / PAGE_SIZE;
	int mapped_pages = 0;

	uint64 pa = 0;
	uint64 va = start;

	while(mapped_pages < num){
		pa = (uint64) kalloc();
		if(pa == 0){
			// no p memory,unmap and free mapped page ,then fail;
			uvmunmap(p->pagetable,start,mapped_pages,1);
			errorf("mmap:no enough physical mem");
			return -1;
		}
		// map phical memory page to user virtual memory space
		if(mappages(p->pagetable, va, PAGE_SIZE, pa, perm) == -1){
		    //unmap,free and fail;
		    uvmunmap(p->pagetable,start,mapped_pages,1);

			// this pa is successfully alloced but not mapped,thus not freed by uvmunmap. 
			kfree((void *)pa);
			errorf("mmap:map va to pa failed,possibly already mapped");
			return -1;
	    }
		mapped_pages++;
		va += PAGE_SIZE;
	}

	return 0;
}

// this syscall "handles" va remap and pa realloc when unmap failed
uint64 sys_munmap(uint64 start, uint64 len){

	// fall early
	if(!PGALIGNED(start)){
		errorf("munmap:start address is not page aligned");
		return -1;
	}
	if(len == 0){
		return 0;
	}
	if(!PGALIGNED(len)){
		// not specified, choose to up align len
	}

	struct proc *p = curr_proc();

	int num = PGROUNDUP(len) / PAGE_SIZE;
	uint64 va = start;
	
	while(num > 0){
		// useraddr return 0 when no map, but what if the pa been mapped is 0...
		if(useraddr(p->pagetable,va) == 0){
		    errorf("munmap:va range contains page that not mapped to pm");
			return -1;
	    }
		va += PAGE_SIZE;
		num--;
	}
	// unmap and free at end, so as to "handle" va unmap and pa free when map failed
	uvmunmap(p->pagetable,start,PGROUNDUP(len) / PAGE_SIZE,1);
	return 0;
}


uint64 sys_spawn(uint64 va)
{
	struct proc *p = curr_proc();
	struct proc *np = curr_proc();

	// Allocate process.
	if ((np = allocproc()) == 0) {
		return -1;
	}

	// now:
	// np->context.ra is (uint64)usertrapret;
	// np->context.sp is p->kstack + KSTACK_SIZE;

	char path[200];
	copyinstr(p->pagetable, path, va, 200);

	struct inode *ip;
	if ((ip = namei(path)) == 0) {
		errorf("invalid file name %s\n", path);
		return -1;
	}
	bin_loader(ip, np);
	iput(ip);

	// now:
	// np->ustack is va_end + PAGE_SIZE;
	// np->trapframe->sp is np->ustack + USTACK_SIZE;
	// np->trapframe->epc is va_start;
	// np->max_page is PGROUNDUP(np->ustack + USTACK_SIZE - 1) / PAGE_SIZE;
	// np->program_brk is np->ustack + USTACK_SIZE;
    // np->heap_bottom is np->ustack + USTACK_SIZE;
	// np->state is RUNNABLE;

	np->parent = p;
	// new process return 0
	np->trapframe->a0 = 0;

	// let new process have same stride as parent
	np->stride = p->stride;
	// new process can be scheduled
	add_task(np);

	return np->pid;
}

uint64 sys_set_priority(long long prio){
    if(prio < 2){
		errorf("prio need to in [2, isize_max]");
		return -1;
	}
	struct proc *p = curr_proc();
	p->priority = prio;
	return prio;
}
/*=========================================================*/
extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);
/*=========================================================*/
	curr_proc()->syscall_times[id]++;
/*=========================================================*/
	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_read:
		ret = sys_read(args[0], args[1], args[2]);
		break;
	case SYS_openat:
		ret = sys_openat(args[0], args[1], args[2]);
		break;
	case SYS_close:
		ret = sys_close(args[0]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
		// __builtin_unreachable();
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday(args[0], args[1]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	case SYS_getppid:
		ret = sys_getppid();
		break;
	case SYS_clone: // SYS_fork
		ret = sys_clone();
		break;
	case SYS_execve:
		ret = sys_exec(args[0], args[1]);
		break;
	case SYS_wait4:
		ret = sys_wait(args[0], args[1]);
		break;
	case SYS_fstat:
		ret = sys_fstat(args[0], args[1]);
		break;
	case SYS_linkat:
		ret = sys_linkat(args[0], args[1], args[2], args[3], args[4]);
		break;
	case SYS_unlinkat:
		ret = sys_unlinkat(args[0], args[1], args[2]);
		break;
	case SYS_spawn:
		ret = sys_spawn(args[0]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
/*=========================================================*/
	case SYS_task_info:
	    ret = sys_task_info((TaskInfo*)args[0]);
		break;
	case SYS_mmap:
	    ret = sys_mmap(args[0],args[1],args[2],args[3],args[4]);
		break;
	case SYS_munmap:
	    ret = sys_munmap(args[0],args[1]);
		break;
	case SYS_setpriority:
	    ret = sys_set_priority(args[0]);
		break;
/*=========================================================*/
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}
