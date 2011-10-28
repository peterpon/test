/*
 *  MemMgr.h
 *  iOS
 *
 *  Created by peterpon on 12/7/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _MEMMGR_
#define _MEMMGR_

#include "MemHeap.h"

#define HEAP_MAX_COUNT	16
#define HEAP_STACK_SIZE 16

class CMemMgr
{
private:
	CMemHeap* mp_heaps;
	
	int	m_num_heaps;
	int m_current_heap;			// allocation
	int m_current_freed_heap;	// free
	int	m_stack_top;
	int m_stack[HEAP_STACK_SIZE];

public:
	CMemMgr(void);

	static CMemMgr &GetInstance( void );

	bool Init( CMemHeap *p_heaps, int num_heaps );
	void Release( CMemHeap *p_heaps, int num_heaps );
	
	void UseHeap( int index );
	void UseHeap( const char *name );

	int  PushHeap( void );
	int  PopHeap( void );
	
	int	 FindHeap( void *p );

	void* mem_alloc( int size, int alignment = 16 );
	void  mem_free( void *p, bool bAutoHeap = true );
};

void* operator new( unsigned int num_bytes );
void* operator new[]( unsigned int num_bytes );
void  operator delete( void *p );
void  operator delete[]( void *p );

#endif // _MEMMGR_