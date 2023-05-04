#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>

int PID = 0;

module_param(PID, int, 0);
MODULE_PARM_DESC(PID, "pid");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Efe Ertem | Arda Aykan Poyraz");
MODULE_DESCRIPTION("A module for the project");

void createTree(struct task_struct *t);

int simple_init(void) {
    struct pid *pid_struct;
    struct task_struct *task;
    
    printk(KERN_INFO "Loading Module\n");

    pid_struct = find_get_pid(PID);
    task = pid_task(pid_struct, PIDTYPE_PID);

    if (task != NULL) createTree(task);

    return 0;
}

void simple_exit(void) {
	printk(KERN_INFO "Removing Module \n");
}

void createTree(struct task_struct *task) {
    
    struct task_struct *child;
    struct list_head *list;

    if(!list_empty(&task->children)){
    	list_for_each(list, &task->children) {
	        child = list_entry(list, struct task_struct, sibling);
	        printk(KERN_INFO "%d %d\n", (child->parent)->pid, child->pid);
	        createTree(child);
	    }
    }
}


module_init(simple_init);
module_exit(simple_exit);