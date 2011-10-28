/*
 *  MemMgr.cpp
 *  iOS
 *
 *  Created by peterpon on 12/7/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "MemHeap.h"
#include "MemMgr.h"

static CMemMgr g_manager;

CMemMgr::CMemMgr(void)
{
	memset( this, 0, sizeof( CMemMgr ) );
}

CMemMgr& CMemMgr::GetInstance( void )
{
	return g_manager;
}

bool CMemMgr::Init( CMemHeap *p_heaps, int num_heaps )
{
	mp_heaps = p_heaps;
	m_num_heaps = num_heaps;
	
	for ( int i=0; i<m_num_heaps; i++ )
	{
		void *p_buffer = malloc( mp_heaps[i].get_size() );
		
		if ( NULL == p_buffer )
		{
			return false;
		}

		mp_heaps[i].init( p_buffer, mp_heaps[i].get_size() );

		m_current_heap = i;
	}
	
	return true;
}

void CMemMgr::Release( CMemHeap *p_heaps, int num_heaps )
{
	for ( int i=0; i<m_num_heaps; i++ )
	{
		const void *p_buffer = mp_heaps[i].get_buffer();
		free( (void *) p_buffer );
	}
}

void CMemMgr::UseHeap( int heap )
{
	assert( heap >= 0 && heap < m_num_heaps );
	assert( mp_heaps[ heap ].get_buffer() );
	m_current_heap = heap;
}

void CMemMgr::UseHeap( const char *name )
{
	for ( int i=0; i<m_num_heaps; i++ )
	{
		if ( !strcmp( name, mp_heaps[i].get_name() ) )
		{
			UseHeap( i );
			return;
		}
	}
	assert(0);
}

int CMemMgr::PushHeap( void )
{
	assert( m_stack_top >= 0 && m_stack_top < HEAP_STACK_SIZE );
	m_stack[ m_stack_top++ ] = m_current_heap;
	return m_current_heap;
}

int CMemMgr::PopHeap( void )
{
	assert( m_stack_top > 0 && m_stack_top < HEAP_STACK_SIZE );
	UseHeap( m_stack[ m_stack_top-- ] );
	return m_current_heap;
}

int CMemMgr::FindHeap( void *p )
{
	assert( m_current_freed_heap >= 0 && m_current_freed_heap < m_num_heaps );

	unsigned int address = (unsigned int) p;
	int index = m_current_freed_heap;

	for ( int i=0; i<m_num_heaps; i++ )
	{
		if ( mp_heaps[ index ].inside_heap( p ) )
		{
			return index;
		}
		else
		{
			index = ( index + 1 ) % m_num_heaps;
		}
	}

	return -1;
}

void *CMemMgr::mem_alloc( int size, int alignment )
{
	assert( m_current_heap >= 0 && m_current_heap < m_num_heaps );
	return mp_heaps[ m_current_heap ].mem_alloc( size, alignment );
}

void CMemMgr::mem_free( void *p, bool bAutoHeap )
{
	if ( bAutoHeap )
	{
		// find the current heap
		m_current_freed_heap = FindHeap( p );
		assert( m_current_freed_heap >=0 && m_current_freed_heap < m_num_heaps );
		
		PushHeap(); // keep current heap
		UseHeap( m_current_freed_heap ); // switch to the owner
		mp_heaps[ m_current_heap ].mem_free( p );
		PopHeap();	// restore current heap
	}
	else
	{
		mp_heaps[ m_current_heap ].mem_free( p );
	}
}

void* operator new( unsigned int num_bytes )
{
	return g_manager.mem_alloc( num_bytes );
}

void* operator new[]( unsigned int num_bytes )
{
	return g_manager.mem_alloc( num_bytes );
}

void  operator delete( void *p )
{
	g_manager.mem_free( p );
}

void  operator delete[]( void *p )
{
	g_manager.mem_free( p );
}
