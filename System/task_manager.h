/***********************************************************************************/
/* Copyright (c) 2013, Wictor Lund. All rights reserved.			   */
/* Copyright (c) 2013, Åbo Akademi University. All rights reserved.		   */
/* 										   */
/* Redistribution and use in source and binary forms, with or without		   */
/* modification, are permitted provided that the following conditions are met:	   */
/*      * Redistributions of source code must retain the above copyright	   */
/*        notice, this list of conditions and the following disclaimer.		   */
/*      * Redistributions in binary form must reproduce the above copyright	   */
/*        notice, this list of conditions and the following disclaimer in the	   */
/*        documentation and/or other materials provided with the distribution.	   */
/*      * Neither the name of the Åbo Akademi University nor the		   */
/*        names of its contributors may be used to endorse or promote products	   */
/*        derived from this software without specific prior written permission.	   */
/* 										   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND */
/* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED   */
/* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE	   */
/* DISCLAIMED. IN NO EVENT SHALL ÅBO AKADEMI UNIVERSITY BE LIABLE FOR ANY	   */
/* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES	   */
/* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;	   */
/* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND	   */
/* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT	   */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS   */
/* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 		   */
/***********************************************************************************/

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <rtu.h>

#include <elf.h>

typedef struct task_section_cons_t {
	const char			*name;
	Elf32_Half			 section_index;
	size_t				 asize;
	void				*amem;
	struct task_section_cons_t	*next;
} task_section_cons_t;

typedef struct task_register_cons_t {
	const char			*name;
	Elf32_Ehdr			*elfh;
	xTaskHandle			 task_handle;
	request_hook_fn_t		 request_hook;
	struct task_register_cons_t	*next;
} task_register_cons;

task_register_cons	*task_find(const char *name);
int			 task_link(task_register_cons *trc);
int			 task_start(task_register_cons *trc);
task_register_cons	*task_register(const char *name, Elf32_Ehdr *elfh);

#endif /* TASK_MANAGER_H */
