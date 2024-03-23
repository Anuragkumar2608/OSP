#include<linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/cpumask.h>

asmlinkage long sys_isolate(void){
	cpumask_t mycpus;   
    struct task_struct *p;
    cpumask_setall(&mycpus);
    cpumask_clear_cpu(3,&mycpus);
    for_each_process(p){
        sched_setaffinity(p->pid,&mycpus);
    }
	return 0;
}
