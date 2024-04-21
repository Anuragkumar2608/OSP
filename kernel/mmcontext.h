#include<linux/kernel.h>
#include<linux/syscalls.h>
#include<linux/sched.h>
#include<linux/mm.h>
#include<linux/fs.h>
#include<linux/highmem.h>

struct page_buff{
    unsigned char *buff_page;
    struct page_buff* next;
};

struct vm_pages{
    struct page **pages;
    struct vm_pages *next;
};

struct vm_buff{
    struct page_buff *page_buffs;
    struct vm_buff *next;
};