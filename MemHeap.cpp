/********************************

	Memory allocator

	2010/12/10	Peter Pon

*********************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "MemHeap.h"

const mem_size_t g_block_header_size = sizeof( sMemBlock );

struct sAllocBlock
{
	address_t m_address_begin;
	address_t m_address_header;
	address_t m_address_alloc;
	address_t m_address_end;
};

inline sAllocBlock calculate_alloc_block( void *p_begin, mem_size_t size, mem_size_t alignment )
{
	sAllocBlock block;

	// where it starts
	block.m_address_begin = reinterpret_cast< address_t > ( p_begin );
	// reserves space for memory block header
	block.m_address_header = address_alignment( block.m_address_begin, g_memory_header_alignment );
	// address for the real thing
	block.m_address_alloc = address_alignment( block.m_address_header + g_block_header_size, alignment );
	// where it ends
	block.m_address_end = address_alignment( block.m_address_alloc + size, g_memory_header_alignment );

	address_t gap = block.m_address_alloc - block.m_address_header;

	if ( gap > g_memory_block_size )
	{
		block.m_address_header = block.m_address_alloc - g_memory_block_size;
	}

	return block;
}

bool sFreeList::fit( mem_size_t size, mem_size_t alignment )
{
	if ( size > m_size )
	{
		// early return
		return false;
	}

	// alignment has to be a multiplier of g_memory_header_alignment
	assert( (alignment % g_memory_header_alignment)==0 );

	sAllocBlock block = calculate_alloc_block( this, size, alignment );

	// real allocation size
	address_t mem_required = block.m_address_end - block.m_address_begin;

	return m_size >= mem_required;
}

CMemHeap::CMemHeap( const char *p_name, mem_size_t size )
{
	m_method = ALLOC_BEST_FIT;
	mp_buffer = NULL;
	mp_free_list = NULL;

	m_size = size;
	m_free = 0;
	m_num_fragments = 0;

	strcpy( m_name, p_name );
};

const bool CMemHeap::inside_heap( void *p )
{
	address_t address = reinterpret_cast< address_t > ( p );
	address_t address_begin = reinterpret_cast< address_t > ( mp_buffer );
	return ( address > address_begin && address < address_begin + m_size );
}

void CMemHeap::init( void* p_buffer, mem_size_t size )
{
	mp_buffer = reinterpret_cast<unsigned char *> ( p_buffer );

	m_size = size;
	m_num_fragments = 1;

	address_t address = reinterpret_cast<address_t> ( p_buffer );
	address_t address_begin = address_alignment( address, g_memory_header_alignment );
	address_t wasted = address_begin - address;

	mp_free_list = reinterpret_cast< sFreeList *> ( address );

	mp_free_list->mp_next = NULL;
	mp_free_list->mp_prev = NULL;
	mp_free_list->m_size = m_size - wasted;
	mp_free_list->calculate_tag();

	m_free = mp_free_list->m_size;
}

void* CMemHeap::mem_alloc( mem_size_t size, mem_size_t alignment )
{
	assert( mp_free_list );

	sFreeList* p_list = mp_free_list;
	sFreeList* p_selected_list = NULL;

	mem_size_t alloc_size = size > g_memory_block_size ? size : g_memory_block_size;
	
	switch( m_method )
	{
	case ALLOC_FIRST_FIT:
		while( p_list )
		{
			if ( p_list->fit( alloc_size, alignment ) )
			{
				p_selected_list = p_list;
				break;
			}
			p_list = p_list->mp_next;
		}
		break;
	case ALLOC_BEST_FIT:
		{
			mem_size_t best_fit_size = 0xffffffff;

			while( p_list )
			{
				if ( p_list->fit( alloc_size, alignment ) && best_fit_size > p_list->m_size )
				{
					p_selected_list = p_list;
					best_fit_size = p_list->m_size;
				}
				p_list = p_list->mp_next;
			}
		}
		break;
	}

	// out of memory
	if ( NULL == p_selected_list )
	{
		return NULL;
	}

	// update free list
	const mem_size_t block_header_size = sizeof( sMemBlock );

	sAllocBlock block = calculate_alloc_block( p_selected_list, size, alignment );

	mem_size_t mem_occupied = block.m_address_end - block.m_address_begin;
	mem_size_t mem_left = p_selected_list->m_size - mem_occupied;

	if ( mem_left > g_memory_minimum_freelist_size )
	{
		// shrink free list
		sFreeList* p_free_list = reinterpret_cast< sFreeList * > ( block.m_address_end );

		if ( p_selected_list->mp_prev )
		{
			p_selected_list->mp_prev->mp_next = p_free_list;
		}

		if ( p_selected_list->mp_next )
		{
			p_selected_list->mp_next->mp_prev = p_free_list;
		}

		p_free_list->mp_prev = p_selected_list->mp_prev;
		p_free_list->mp_next = p_selected_list->mp_next;
		p_free_list->m_size = mem_left;
		p_free_list->calculate_tag();
	}
	else
	{
		// give it all to this mem block
		mem_occupied = p_selected_list->m_size;
		mem_left = 0;

		// remove free list
		if ( p_selected_list->mp_prev )
		{
			p_selected_list->mp_prev->mp_next = p_selected_list->mp_next;
		}

		if ( p_selected_list->mp_next )
		{
			p_selected_list->mp_next->mp_prev = p_selected_list->mp_prev;
		}

		m_num_fragments--;
		assert( m_num_fragments > 0 );
	}

	sMemBlock *p_block = reinterpret_cast< sMemBlock * > ( block.m_address_header );

	p_block->m_begin = block.m_address_begin;
	p_block->m_size = mem_occupied;
	p_block->calculate_tag();

	// update free list head if necessary
	address_t free_list_address = reinterpret_cast< address_t > ( mp_free_list );

	if ( block.m_address_begin == free_list_address )
	{
		mp_free_list = reinterpret_cast< sFreeList * > ( block.m_address_end );
	}

	m_free -= p_block->m_size;

	printf("%x = alloc(%d) -> alloc_real(%d)\n", block.m_address_alloc, size, p_block->m_size );

	void *p_mem = reinterpret_cast< void * > ( block.m_address_alloc );
	return p_mem;
}

void CMemHeap::mem_free( void *p )
{
	address_t address = reinterpret_cast< address_t > ( p );
	address_t heap_begin = reinterpret_cast< address_t > ( mp_buffer );
	address_t heap_end = heap_begin + m_size;

	assert( address >= heap_begin + g_block_header_size && address < heap_end );
	assert( (address % g_memory_header_alignment)==0 );

	sMemBlock *p_block = reinterpret_cast< sMemBlock * > ( address - g_block_header_size );
	
	assert( p_block->validate_tag()==true );

	address_t address_begin = p_block->m_begin;
	address_t address_end = address_begin + p_block->m_size;

	assert( mp_free_list );

	sFreeList* p_list = mp_free_list;
	sFreeList* p_insert_list = mp_free_list;

	bool processed = false;
	mem_size_t reclaimed_size = 0;

	while( p_list )
	{
		assert( p_list->validate_tag()==true );

		address_t list_address_begin = reinterpret_cast< address_t > ( p_list );
		address_t list_address_end = list_address_begin + p_list->m_size;

		if ( address_end == list_address_begin )
		{
			expand_forward( &p_list, address_begin );
			address_end = list_address_end;
			processed = true;
			break;
		}
		else if ( address_begin == list_address_end )
		{
			expand_backward( &p_list, address_end );
			address_begin = list_address_begin;
			processed = true;
			break;
		}
		else if ( address_begin > list_address_begin )
		{
			p_insert_list = p_list;
			break;
		}

		p_list = p_list->mp_next;
	}

	if ( !processed && p_insert_list )
	{
		// add a new fragment

		sFreeList *p_new_list = reinterpret_cast< sFreeList * > ( address_begin );
		
		if ( p_new_list > p_insert_list )
		{
			// add to the back
			p_new_list->mp_prev = p_insert_list;
			p_new_list->mp_next = p_insert_list->mp_next;
			p_new_list->m_size = p_block->m_size;
			p_new_list->calculate_tag();

			p_insert_list->mp_next = p_new_list;
		}
		else
		{
			// add to the front
			p_new_list->mp_prev = p_insert_list->mp_prev;
			p_new_list->mp_next = p_insert_list;
			p_new_list->m_size = p_block->m_size;
			p_new_list->calculate_tag();

			p_insert_list->mp_prev = p_new_list;
		}

		// update free list head if necessary
		address_t free_list_address = reinterpret_cast< address_t > ( mp_free_list );
		
		if ( address_begin < free_list_address )
		{
			mp_free_list = reinterpret_cast< sFreeList * > ( address_begin );
		}

		m_num_fragments++;
		processed = true;
	}

	assert( address_end > address_begin );

	m_free += p_block->m_size;

	assert( processed==true );

	printf("free(%x) %d bytes\n", p, p_block->m_size);
}

void CMemHeap::expand_forward( sFreeList **p_list, address_t address_begin )
{
	address_t address = reinterpret_cast< address_t > ( *p_list );
	address_t expansion = address - address_begin;
	assert( address > address_begin );

	sFreeList *list = reinterpret_cast< sFreeList * > ( address_begin );
	
	*list = **p_list;	
	assert( list->validate_tag()==true );

	list->m_size += expansion;
	list->calculate_tag();

	if ( *p_list==mp_free_list )
	{
		mp_free_list = reinterpret_cast< sFreeList * > ( address_begin );
	}

	*p_list = reinterpret_cast< sFreeList * > ( address_begin );
}

void CMemHeap::expand_backward( sFreeList **p_list, address_t address_end )
{
	address_t address_begin = reinterpret_cast< address_t > ( *p_list );
	address_t address_end_original = address_begin + (*p_list)->m_size;
	assert( address_end > address_begin && address_end > address_end_original );

	sFreeList *list = reinterpret_cast< sFreeList * > ( address_begin );
	*list = **p_list;
	list->m_size = address_end - address_begin;
	list->calculate_tag();
}

const bool CMemHeap::valid_heap( void )
{
	sFreeList *p_list = mp_free_list;
	int num_free_blocks = 0;
	int num_free_bytes = 0;

	while( p_list )
	{
		num_free_blocks++;
		num_free_bytes += p_list->m_size;
		p_list = p_list->mp_next;
	}

	return num_free_blocks == m_num_fragments && num_free_bytes == m_free;
}

const void CMemHeap::dump( void )
{
	sFreeList *p_list = mp_free_list;

	int num_free_blocks = 0;
	int num_free_bytes = 0;

	printf( "heap %s, %d fragments\n", m_name, m_num_fragments );
	printf( "{\n");

	while( p_list )
	{
		int kb = p_list->m_size / 1024;
		float mb = static_cast<float> ( p_list->m_size ) / ( 1024.0f * 1024.0f );

		printf(
			"  block(%d) address:%x size:(%d bytes, %d KBs, %6.3f MBs)\n", 
			num_free_blocks, p_list, p_list->m_size, kb, mb 
		);

		num_free_blocks++;
		num_free_bytes += p_list->m_size;

		p_list = p_list->mp_next;
	}

	printf( "}\n");

	if ( num_free_blocks != m_num_fragments || num_free_bytes != m_free )
	{
		printf("bug!\n");
	}
}