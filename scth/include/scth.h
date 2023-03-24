
#ifndef _SCTH_
#define _SCTH_

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
