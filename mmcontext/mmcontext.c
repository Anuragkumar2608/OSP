#include<linux/kernel.h>
#include<linux/syscalls.h>
#include<linux/sched.h>
#include<linux/mm.h>
#include<linux/fs.h>
#include <linux/highmem.h>

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

struct page_buff *head_buff = NULL, *tail_buff = NULL;

struct vm_pages *pg_head = NULL, *pg_tail = NULL;

struct vm_buff *vm_head = NULL, *vm_tail = NULL;

static inline void saveState(void){
    struct task_struct *task = current;
    struct page **pages;
    struct vm_area_struct *vma;
    struct vm_pages *ptr;
    struct page_buff *buff;
    struct vm_buff *vbuff;
    int ret;
    int i;
    unsigned char *kadd;
    unsigned long num_pages;
    task->saveRes = 1;
    vma = task->mm->mmap;
    for ( ; vma ; vma = vma->vm_next){
        if ((!vma->vm_file && !vma->vm_ops) && !(vma->vm_flags & VM_STACK) && (vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED) && (vma->vm_start<= task->mm->brk && vma->vm_end>= task->mm->start_brk)) {
            num_pages = vma_pages(vma);
            ptr = kmalloc(sizeof(struct vm_pages),GFP_KERNEL);
            ptr->pages = kmalloc(num_pages * sizeof(struct page *), GFP_KERNEL);
            ptr->next = NULL;

            if(!pg_tail)
            {
                pg_tail = ptr;
                pg_head = pg_tail;
            }else{
                pg_tail->next = ptr;
                pg_tail = pg_tail->next;
            }
            pages = pg_tail->pages;
            ret = get_user_pages(vma->vm_start, vma_pages(vma), FOLL_WRITE, pages, NULL);

            for (i = 0; i < vma_pages(vma); i++) {
                if (pages[i]) {
                    buff = kmalloc(sizeof(struct page_buff),GFP_KERNEL);
                    buff->buff_page = kmalloc(PAGE_SIZE,GFP_KERNEL);
                    buff->next = NULL;
                    kadd = kmap(pages[i]);
                    memcpy(buff->buff_page,kadd,PAGE_SIZE);
                    kunmap(pages[i]);
                    if(!tail_buff){
                        tail_buff = buff;
                        head_buff = tail_buff;
                    }else{
                        tail_buff->next = buff;
                        tail_buff = tail_buff->next;
                    }
                }
            }
            vbuff = kmalloc(sizeof(struct vm_buff),GFP_KERNEL);
            vbuff->page_buffs = head_buff;
            vbuff->next = NULL;
            if(!vm_tail){   
                vm_tail = vbuff;
                vm_head = vm_tail;
            }else{
                vm_tail->next = vbuff;
                vm_tail = vm_tail->next; 
            }
            head_buff = NULL;
            tail_buff = NULL;
        }
    }
    task->savedPages = pg_head;
    task->savedtoBuffer = vm_head;
}

static inline void restoreState(void){
    struct task_struct *task = current;
    struct page **pages;
    struct vm_area_struct *vma;
    struct vm_pages *ptr;
    struct page_buff *buff;
    struct vm_buff *vbuff;
    int i;
    unsigned char *kadd;
    unsigned long num_pages;
    task->saveRes = 0;
    vma = task->mm->mmap;
    pg_head = task->savedPages;
    vm_head = task->savedtoBuffer;
    pg_tail = pg_head;
    vm_tail = vm_head;
    for ( ; vma ; vma = vma->vm_next){
        if ((!vma->vm_file && !vma->vm_ops) && !(vma->vm_flags & VM_STACK) && (vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED) && (vma->vm_start<= task->mm->brk && vma->vm_end>= task->mm->start_brk)) {
            pages = pg_tail->pages;
            tail_buff = vm_tail->page_buffs;
            num_pages = vma_pages(vma);
            if (pages) {
                for (i = 0; i < vma_pages(vma); i++) {
                    if (pages[i]) {
                        kadd = kmap(pages[i]);
                        memcpy(kadd,tail_buff->buff_page,PAGE_SIZE);
                        kunmap(pages[i]);
                        buff = tail_buff;
                        tail_buff = tail_buff->next;
                        buff->next = NULL;
                        kfree(buff->buff_page);
                        kfree(buff);
                        put_page(pages[i]);
                    }
                }
            }
            vbuff = vm_tail;
            vm_tail = vm_tail->next;
            ptr = pg_tail;
            pg_tail = pg_tail->next;
            kfree(vbuff);
            kfree(ptr->pages);
            kfree(ptr);
        }
    }
    task->savedPages = NULL;
    task->savedtoBuffer = NULL;
}

asmlinkage long __x64_sys_mmcontext(int saveRes) {
    struct task_struct *task = current;
    
    struct pt_regs *regs = current_pt_regs(); // Get pointer to current pt_regs
    // long syscall_num = regs->orig_ax; // Get system call number
    long arg_1 = regs->di;

    if (task->saveRes != arg_1) {
        return -EINVAL;
    }
    if(!arg_1) {
        saveState();
    } 
    else
    {
        restoreState();
    }
    return 0;
}
