
#ifndef _SCTH_
#define _SCTH_


#define NO_MAP (-1)


extern unsigned long the_syscall_table;
//module_param(the_syscall_table, ulong, 0660);
extern unsigned long **hacked_syscall_tbl;



void syscall_table_finder(void);


void protect_memory(void);
void unprotect_memory(void);
int get_entries(int *, int, unsigned long*, unsigned long* );


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
extern long sys_put_data;
extern long sys_get_data;
extern long sys_invalidate_data;
#else

#endif
#endif
