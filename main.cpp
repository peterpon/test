#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>

#include "MemMgr.h"

#define ARRAY_SIZE(x) ( sizeof(x)/sizeof(x[0]) )

const unsigned int g_OneMegaBytes = 1024 * 1024;

void main( void )
{
	CMemHeap heaps[] =
	{
		CMemHeap("heap0", 16 * g_OneMegaBytes ),
		CMemHeap("heap1", 16 * g_OneMegaBytes ),
		CMemHeap("heap2", 16 * g_OneMegaBytes ),
		CMemHeap("heap3", 16 * g_OneMegaBytes ),
	};

	CMemMgr &mgr = CMemMgr::GetInstance();
	mgr.Init( heaps, ARRAY_SIZE(heaps) );
	mgr.UseHeap( 0 );

	const int dump_frequency = 1;
	const int test_count = 8;
	const int test_max_alloc = 1024;
	const int buffers = 4;

	char *p_array[buffers];
	memset( p_array, 0, sizeof(p_array) );

	time_t t = time(0);
	srand( t );

	heaps[0].dump();

	for ( int i=0; i<test_count; i++ )
	{
		int alloc_size = rand() % test_max_alloc + 1;
		int heap = rand() % ARRAY_SIZE( heaps );
		int item = rand() % buffers;

		if ( NULL == p_array[item] )
		{
			p_array[item] = new char[alloc_size];
		}
		else
		{
			delete [] p_array[item];
			p_array[item] = NULL;
		}

		if ( (i % dump_frequency)==0 )
		{
			heaps[0].dump();
		}
	}

	for ( int i=0; i<buffers; i++ )
	{
		if ( p_array[i] )
		{
			delete [] p_array[i];
			p_array[i] = NULL;
		}
	}

	heaps[0].dump();

	mgr.Release( heaps, ARRAY_SIZE(heaps) );
}