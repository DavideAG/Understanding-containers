#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <seccomp.h>
#include <sched.h>
#include <sys/ioctl.h>
#include "./seccomp_config.h"
void sys_filter(){

	scmp_filter_ctx ctx;

	/* Intialize filter contex.
	 *
	 * - SCMP_ACT_ALLOW : The seccomp filter will have no effect on the thread calling
         *                    the syscall if it does not match any of the configured seccomp
         *                    filter rules.       
         */
	ctx = seccomp_init(SCMP_ACT_ALLOW);

	
	
	if (ctx == NULL){
		fprintf(stderr,"=> ERR: seccomp_init.\n");
		seccomp_release(ctx);
		exit(EXIT_FAILURE);
	}

	/*
	 * - ctx : the context of the filter
	 *
	 * - SCMP_ACT_KILL_PROCESS : action field, The process will be killed by the kernel when it calls a
         *     			     syscall that matches the filter rule.
	 *
	 * - SCMP_SYS(syscall_name) : While it is possible to specify the syscall value directly using the
       	 *        		      standard __NR_syscall values, in order to ensure proper operation
         *                            across multiple architectures it is highly recommended to use the
         *                            SCMP_SYS() macro instead.
         *
	 * - 1 : arg_cont
	 *
	 * - SCMP_A1 : used to filter syscall based on its argument. A_1 means that it is referring to a specific
	 *             argument, in this case the one at index 1 (the second one). In the case of chmod it is 
	 *             referring to the mode option:
	 *             					S_ISUID: set effective user id of the process 
	 *             					S_ISGID: set effective group id of the process
	*/
	
	if(!(
	         seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(chmod), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, S_ISUID, S_ISUID))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(chmod), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, S_ISGID, S_ISGID))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(fchmod), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, S_ISUID, S_ISUID))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(fchmod), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, S_ISGID, S_ISGID))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(fchmodat), 1, SCMP_A2(SCMP_CMP_MASKED_EQ, S_ISUID, S_ISUID))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(fchmodat), 1, SCMP_A2(SCMP_CMP_MASKED_EQ, S_ISGID, S_ISGID))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(unshare), 1, SCMP_A0(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(clone), 1, SCMP_A0(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER))
	      /* This syscall is actually called somewhere and makes the container process die. Actually disabling this syscall is only
	       * usefull if we are using SElinux policies so in our case we can delete it. 
               */
	      //|| seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(ioctl), 1, SCMP_A1(SCMP_CMP_MASKED_EQ, TIOCSTI, TIOCSTI))
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(keyctl), 0)
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(add_key), 0)
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(request_key), 0)
	      || seccomp_rule_add(ctx, SCMP_ACT_KILL_PROCESS, SCMP_SYS(ptrace), 0)
	   )){
		         /* Successfully added filter's rules. Now load the filter into the Kernel. */
			 int lv = seccomp_load(ctx);
		         if(lv<0){
			   fprintf(stderr,"=> ERR: seccomp_load failed.\n");
			   seccomp_release(ctx);
	       		   exit(EXIT_FAILURE);		
			 }
	     }else{
		      fprintf(stderr,"=> ERR: seccomp_rule_add failed.\n");
		      seccomp_release(ctx);
	       	      exit(EXIT_FAILURE);		
	     }

	seccomp_release(ctx);
	fprintf(stderr,"=> syscall filtering done.\n");

}
