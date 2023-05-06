#include "kalloc.h"
#include "defs.h"
#include "riscv.h"

extern char ekernel[];
/*=========================begin===========================*/
uint64 frame_pa_start;
uint64 frame_pa_end;

struct page *pages;
int get_page(uint64 pa){
	if(!PGALIGNED(pa)){
		return -1;
	}
	// can not equal to frame_pa_end
	if(pa < frame_pa_start || pa >= frame_pa_end){
		return -1;
	}
	int page_index = (pa - frame_pa_start)/PGSIZE;
	pages[page_index].ref++;
	return 0;
}
int put_page(uint64 pa){
	if(!PGALIGNED(pa)){
		return -1;
	}
	if(pa < frame_pa_start || pa >= frame_pa_end){
		return -1;
	}
	int page_index = (pa - frame_pa_start)/PGSIZE;
	if(pages[page_index].ref <= 0){
		errorf("put page: no ref\n");
		return -1;
	}
	pages[page_index].ref--;
	return pages[page_index].ref;
}
int get_page_ref(uint64 pa){
	if(!PGALIGNED(pa)){
		return -1;
	}
	if(pa < frame_pa_start || pa >= frame_pa_end){
		return -1;
	}
	int page_index = (pa - frame_pa_start)/PGSIZE;
	return pages[page_index].ref;
}
/*=========================================================*/

struct linklist {
	struct linklist *next;
};

struct {
	struct linklist *freelist;
} kmem;

void freerange(void *pa_start, void *pa_end)
{
	char *p;
	p = (char *)PGROUNDUP((uint64)pa_start);
	for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
		kfree(p);
}

void kinit()
{
/*=========================begin===========================*/
	// aligned address is comforting
	uint64 free_memory_start = PGROUNDUP((uint64)ekernel);

    int num_pages = PGROUNDUP(PHYSTOP - free_memory_start) / PGSIZE;
	frame_pa_start = PGROUNDUP(free_memory_start + sizeof(struct page) * num_pages);
	frame_pa_end = frame_pa_start + num_pages * PAGE_SIZE;

	pages = (struct page *)free_memory_start;

	// page ref initialized to 1;
	for(int i=0;i<num_pages;i++){
		pages[i].ref = 1;
	}
/*=========================================================*/
	freerange((void *)frame_pa_start, (void *)PHYSTOP);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
/*=========================begin===========================*/
void kfree(void *pa)
{
	struct linklist *l;
	if (((uint64)pa % PGSIZE) != 0 || (char *)pa < ekernel ||
	    (uint64)pa >= PHYSTOP)
		panic("kfree");
	// Fill with junk to catch dangling refs.
	int ret = put_page((uint64)pa);
	if(ret < 0){
		panic("put page failed in kfree\n");
	}else if (ret == 0){
		memset(pa, 1, PGSIZE);
	    l = (struct linklist *)pa;
	    l->next = kmem.freelist;
	    kmem.freelist = l;
	}
}
/*=========================================================*/

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/*=========================begin===========================*/
void *kalloc()
{
	struct linklist *l;
	l = kmem.freelist;
	if (l) {
		kmem.freelist = l->next;
		memset((char *)l, 5, PGSIZE); // fill with junk
		if(get_page((uint64) l) != 0){
		    panic("get_page failed in kalloc\n");
	    }
/*=========================================================*/
	}
	return (void *)l;
}