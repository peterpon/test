#ifndef _MEM_HEAP_
#define _MEM_HEAP_

#include <assert.h>

typedef unsigned int mem_size_t;
typedef unsigned int address_t;

#define HEAP_NAME_SIZE  32

const mem_size_t g_memory_header_alignment = 16;
const mem_size_t g_memory_block_size = 16;

inline address_t generate_tag( void *p, mem_size_t size )
{
	address_t address = reinterpret_cast< address_t > ( p );
	address_t tag = address ^ size;
	return tag;
}

inline address_t address_alignment( address_t address, mem_size_t alignment )
{
	assert( alignment >= 1 );

	mem_size_t r = address % alignment;
	mem_size_t offset = r==0 ? 0 : alignment - r;
	mem_size_t aligned_address = address + offset;

	return aligned_address;
}

struct sFreeList
{
	sFreeList *mp_prev;
	sFreeList *mp_next;

	mem_size_t m_size;
	address_t  m_tag;

	void init( void )
	{
		mp_prev = mp_next = NULL;
		m_size = 0;
	}

	bool fit( mem_size_t size, mem_size_t alignment );

	address_t calculate_tag( void ) 
	{ 
		m_tag = generate_tag( this, m_size ); 
		return m_tag; 
	}

	bool validate_tag( void )
	{
		address_t tag = generate_tag( this, m_size );
		return tag == m_tag;
	}
};

const mem_size_t g_memory_minimum_freelist_size = ( sizeof(sFreeList) + g_memory_block_size );

struct sMemBlock
{
	mem_size_t	m_size;
	address_t	m_begin;
	address_t	m_tag;
	address_t	m_padding;

	sMemBlock(void)
	{
		m_size = 0;
		m_begin = 0;
		m_tag = 0;
	}

	address_t calculate_tag( void )
	{
		m_tag = generate_tag( this, m_size );
		m_padding = ~m_tag;
		return m_tag;
	}

	bool validate_tag( void )
	{
		address_t tag = generate_tag( this, m_size );
		return ( tag == m_tag ) && ( m_padding == ~tag );
	}
};

enum AllocMethod_e
{
	ALLOC_FIRST_FIT,
	ALLOC_BEST_FIT
};

class CMemHeap
{
private:
	char		m_name[HEAP_NAME_SIZE];
	mem_size_t	m_size;
	mem_size_t	m_free;
	mem_size_t  m_num_fragments;
	AllocMethod_e m_method;
	
	unsigned char*	mp_buffer;
	sFreeList* mp_free_list;

public:
	CMemHeap( const char *p_name, mem_size_t size );
	
	const mem_size_t get_size( void ) { return m_size; }
	const void*	get_buffer( void ) { return mp_buffer; }
	const char*	get_name( void ) { return m_name; }
	const bool	inside_heap( void *p );
	const bool	valid_heap( void );
	const void  dump( void );

	void init( void *p_buffer, mem_size_t size );

	void set_alloc_method( AllocMethod_e method ) { m_method = method; }

	void*mem_alloc( mem_size_t bytes, mem_size_t alignment = 16 );
	void mem_free( void *p );

	void expand_forward( sFreeList** p_list, address_t address_begin );
	void expand_backward( sFreeList** p_list, address_t address_end );
};

#endif // _MEM_HEAP_