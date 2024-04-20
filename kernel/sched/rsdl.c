#define RSDL_LIST_ACTIVE(rq, prio) \
  &((rq->rsdl).active[prio])

#define RSDL_LIST_EXPIRED(rq, prio) \
  &((rq->rsdl).expired[prio])

static void enqueue_task_rsdl(struct rq *rq, struct task_struct *p, int flags) {
  struct rsdl_rq *rq_rsdl;
  struct sched_rsdl_entity *se_rsdl;
  struct rsdl_list *active_list;
  int oldprio, newprio;
  
  rq_rsdl = &rq->rsdl;
  se_rsdl = &p->rsdl;

  oldprio = se_rsdl->priority;
  newprio = PRIO_TO_NICE(p->static_prio) + 20;

  if (oldprio != newprio) {
    se_rsdl->priority = newprio;
  }
    
  if (!task_is_running(p)) {
    return;
  }

  if (se_rsdl->on_rq) {
    return;
  }

  se_rsdl->task = p;
  se_rsdl->quota = 5;
  se_rsdl->on_rq = true;
  rq_rsdl->nr_running++;
  
  if (flags & ENQUEUE_WAKEUP) {
  }
  
  if (flags & ENQUEUE_RESTORE) {
  }
  
  if (se_rsdl->priority <= rq_rsdl->current_list) {
    active_list = RSDL_LIST_ACTIVE(rq, rq_rsdl->current_list);
  } else {
    active_list = RSDL_LIST_ACTIVE(rq, se_rsdl->priority);
  }

  list_add_tail(&se_rsdl->list, &active_list->list);
}

static void dequeue_task_rsdl(struct rq *rq, struct task_struct *p, int flags) {
  struct rsdl_rq *rq_rsdl;
  struct sched_rsdl_entity *se_rsdl;

  rq_rsdl = &rq->rsdl;
  se_rsdl = &p->rsdl;

  if (flags & DEQUEUE_SLEEP) {
    se_rsdl->on_rq = false;
  }
}


static void __rsdl_restart_epoch(struct rq *rq) {
  struct rsdl_rq *rq_rsdl;
  struct rsdl_list *current_list, *expired_list, *temp_list;

  rq_rsdl = &rq->rsdl;
  
  for (int i = 0; i < NICE_WIDTH; i++) {
    current_list = RSDL_LIST_ACTIVE(rq, i);
    current_list->quota = 0;

    expired_list = RSDL_LIST_EXPIRED(rq, i);
    expired_list->quota = 20;
  }

  rq_rsdl->current_list = 0;

  temp_list = rq->rsdl.active;
  rq->rsdl.active = rq->rsdl.expired;
  rq->rsdl.expired = temp_list;
}

static struct task_struct *pick_next_task_rsdl(struct rq *rq) {
  struct rsdl_rq *rq_rsdl;
  struct sched_rsdl_entity *se_rsdl;
  struct task_struct *task;
  struct rsdl_list *active_list, *expired_list, *next_list;
  int i, task_state;

  rq_rsdl = &rq->rsdl;
    
  if (rq_rsdl->nr_running == 0) {
    __rsdl_restart_epoch(rq);
    return NULL;
  }

  i = rq_rsdl->current_list;
  while (i < NICE_WIDTH) {
        active_list = RSDL_LIST_ACTIVE(rq, i);
        
        if (list_empty(&active_list->list)) {
              if (i == NICE_WIDTH - 1) {
        	__rsdl_restart_epoch(rq);
        	i = rq_rsdl->current_list;
              } else {
        	active_list->quota = 0;
        	i++;
              }
              continue;
        }
    
        se_rsdl = list_first_entry(&active_list->list, struct sched_rsdl_entity, list);
        task_state = se_rsdl->task->__state;
        
        if (!task_is_running(se_rsdl->task)) {
              list_del(&se_rsdl->list);
              se_rsdl->on_rq = false;
              rq_rsdl->nr_running--;
              continue;
        }
    
        if (active_list->quota == 0 || (!(task_state & TASK_DEAD) && !task_is_running(se_rsdl->task))) {
    
              if (i == NICE_WIDTH - 1) {
            	while (!list_empty(&active_list->list)) {
                	  se_rsdl = list_first_entry(&active_list->list, struct sched_rsdl_entity, list);
                	  se_rsdl->quota = 5;
                	  expired_list = RSDL_LIST_EXPIRED(rq, se_rsdl->priority);
                	  list_move_tail(&se_rsdl->list, &expired_list->list);
            	}
            	__rsdl_restart_epoch(rq);
            	i = rq_rsdl->current_list;
              } else {
            	list_for_each_entry(se_rsdl, &active_list->list, list) {
            	  se_rsdl->quota += 5;
            	}
            	next_list = RSDL_LIST_ACTIVE(rq, i+1);
            	list_splice_tail_init(&active_list->list, &next_list->list);
            	i++;
              }
        
              continue;
          
        }
    
        if (se_rsdl->quota == 0) {
           if (i == NICE_WIDTH - 1) {
            	expired_list = RSDL_LIST_EXPIRED(rq, se_rsdl->priority);
            	se_rsdl->quota = 5;
            	list_move_tail(&se_rsdl->list, &expired_list->list);
                  } else {
            	next_list = RSDL_LIST_ACTIVE(rq, i + 1);
            	se_rsdl->quota = 5;
            	list_move_tail(&se_rsdl->list, &next_list->list);
           }
    
          continue;
        }
    
        rq_rsdl->current_list = i;
        list_del(&se_rsdl->list);
        task = se_rsdl->task;
        se_rsdl->on_rq = false;
        
        break;
  }

#ifdef RSDL_LOG
  if (rq->cpu == 0)
    printk("RSDL:             => picked task (pid: %d, priority: %d, quota: %d, running: %d, on_rq: %d)\n", se_rsdl->task->pid, se_rsdl->priority, se_rsdl->quota, task_is_running(se_rsdl->task), se_rsdl->on_rq);
#endif

  WARN_ON(!task_is_running(task));  
  return task;
}

static void put_prev_task_rsdl(struct rq *rq, struct task_struct *p) {
    struct rsdl_rq *rq_rsdl;
    struct sched_rsdl_entity *se_rsdl;
    struct rsdl_list *current_list, *expired_list, *next_list;
    int task_state, list_index, oldprio, newprio;
    
    rq_rsdl = &rq->rsdl;
    se_rsdl = &p->rsdl;

    oldprio = se_rsdl->priority;
    newprio = PRIO_TO_NICE(p->static_prio) + 20;

    if (oldprio != newprio) {
    se_rsdl->priority = newprio;
    }
    
    task_state = se_rsdl->task->__state;
    
    if (task_state & TASK_DEAD) {
    rq_rsdl->nr_running--;
    return;
    }

    if (task_state & DEQUEUE_SLEEP) {
    rq_rsdl->nr_running--;
    return;
    }

    se_rsdl->on_rq = true;

    if (se_rsdl->priority > rq_rsdl->current_list) {
    list_index = se_rsdl->priority;
    } else {
    list_index = rq_rsdl->current_list;
    }

    current_list = RSDL_LIST_ACTIVE(rq, list_index);
    
    if (list_index == NICE_WIDTH - 1) {
    if (se_rsdl->quota == 0) {
        se_rsdl->quota = 5;
        expired_list = RSDL_LIST_EXPIRED(rq, se_rsdl->priority);
        list_add_tail(&se_rsdl->list, &expired_list->list);
    } else {
        list_add_tail(&se_rsdl->list, &current_list->list);  
    }
    } else {
    if (se_rsdl->quota == 0) {
        se_rsdl->quota = 5;
        next_list = RSDL_LIST_ACTIVE(rq, list_index + 1);
        list_add_tail(&se_rsdl->list, &next_list->list);
    } else {
        list_add_tail(&se_rsdl->list, &current_list->list);
    }
    }

}

static void set_next_task_rsdl(struct rq *rq, struct task_struct *p, bool first) {
    struct sched_rsdl_entity *se_rsdl;
    se_rsdl = &p->rsdl;
    
    rq->curr = pick_next_task_rsdl(rq);
}

static void task_tick_rsdl(struct rq *rq, struct task_struct *curr, int queued) {
    struct rsdl_rq *rq_rsdl;
    struct sched_rsdl_entity *se_rsdl;
    struct rsdl_list *current_list;

    rq_rsdl = &rq->rsdl;
    se_rsdl = &curr->rsdl;
    current_list = RSDL_LIST_ACTIVE(rq, rq_rsdl->current_list);
     
    if (!current_list->quota || !se_rsdl->quota) {
    resched_curr(rq);
    } else {
    --current_list->quota;
    --se_rsdl->quota;
    }
}

static void check_preempt_curr_rsdl(struct rq *rq, struct task_struct *p, int flags) {
}


static void yield_task_rsdl(struct rq *rq) {
}

static bool yield_to_task_rsdl(struct rq *rq, struct task_struct *p) {
  return false;
}

#ifdef CONFIG_SMP
static int balance_rsdl(struct rq *rq, struct task_struct *prev, struct rq_flags *rf) {
  return rq->cpu;
}

static	int  select_task_rq_rsdl(struct task_struct *p, int task_cpu, int flags) {
  return task_cpu;
}

static struct task_struct *pick_task_rsdl(struct rq *rq) {
  return pick_next_task_rsdl(rq);
}

static void migrate_task_rq_rsdl(struct task_struct *p, int new_cpu) {
}

static void task_woken_rsdl(struct rq *this_rq, struct task_struct *task) {
  struct rsdl_rq *rq_rsdl;
  struct sched_rsdl_entity *se_rsdl;

  rq_rsdl = &this_rq->rsdl;
  se_rsdl = &task->rsdl;
  
  if (!se_rsdl->on_rq) {
    enqueue_task_rsdl(this_rq, task, 0);
  }
}

static void set_cpus_allowed_rsdl(struct task_struct *p, const struct cpumask *newmask, u32 flags) {
}

static void rq_online_rsdl(struct rq *rq) {
}

static void rq_offline_rsdl(struct rq *rq) {
}

struct rq *find_lock_rq_rsdl(struct task_struct *p, struct rq *rq) {
  return rq;
}
#endif

static void task_dead_rsdl(struct task_struct *p) {
  struct sched_rsdl_entity *se_rsdl = &p->rsdl;
  se_rsdl->quota = 0;
  se_rsdl->priority = 20;
  se_rsdl->on_rq = false;
}

static void task_fork_rsdl(struct task_struct *p) {
}

static void prio_changed_rsdl(struct rq *this_rq, struct task_struct *task, int oldprio) {
}

static void switched_from_rsdl(struct rq *this_rq, struct task_struct *task) {
}

static void switched_to_rsdl(struct rq *this_rq, struct task_struct *task) {
}

static unsigned int get_rr_interval_rsdl(struct rq *rq, struct task_struct *task) {
  return 5;
}

static void update_curr_rsdl(struct rq *rq) {
}

DEFINE_SCHED_CLASS(rsdl) = {
	.enqueue_task		= enqueue_task_rsdl,
	.dequeue_task		= dequeue_task_rsdl,
	.yield_task		= yield_task_rsdl,
	.yield_to_task		= yield_to_task_rsdl,

	.check_preempt_curr	= check_preempt_curr_rsdl,

	.pick_next_task		= pick_next_task_rsdl,
	.put_prev_task		= put_prev_task_rsdl,
	.set_next_task          = set_next_task_rsdl,
	
#ifdef CONFIG_SMP
	.balance		= balance_rsdl,
	.pick_task		= pick_task_rsdl,
	.select_task_rq		= select_task_rq_rsdl,
	.migrate_task_rq	= migrate_task_rq_rsdl,
	.task_woken             = task_woken_rsdl,
	.rq_online		= rq_online_rsdl,
	.rq_offline		= rq_offline_rsdl,

	.task_dead		= task_dead_rsdl,
	.set_cpus_allowed	= set_cpus_allowed_rsdl,
#endif
	
	.task_tick		= task_tick_rsdl,
		
	.task_fork		= task_fork_rsdl,

	.prio_changed		= prio_changed_rsdl,
	.switched_from		= switched_from_rsdl,
	.switched_to		= switched_to_rsdl,

	.get_rr_interval	= get_rr_interval_rsdl,

	.update_curr		= update_curr_rsdl,
	
#ifdef CONFIG_UCLAMP_TASK
	.uclamp_enabled		= 0,
#endif
};