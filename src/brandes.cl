#include "brandes.h"

#if double == FP_T
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

#define CLLIST_INIT(listName, listMem, listMemSize, listType)\
	listType *_mem##listName = listMem;\
	int _capacity##listName = listMemSize;\
	int _size##listName = 0;\
	unsigned int _head##listName = 0;\
	unsigned int _tail##listName = 0;

#define CLLIST_VECINIT(listName, listMem, listMemSize, listType, noOfLists)\
	listType *_mem##listName[noOfLists];\
	int _capacity##listName[noOfLists];\
	int _size##listName[noOfLists];\
	unsigned int _head##listName[noOfLists];\
	unsigned int _tail##listName[noOfLists];\
	{ int _i; for(_i = 0; _i < noOfLists; _i++) { _mem##listName[_i] = &listMem[_i * listMemSize]; _capacity##listName[_i] = listMemSize; _size##listName[_i] = 0; _head##listName[_i] = 0; _tail##listName[_i] = 0; } }

#define CLLIST_RESET(listName) {\
	_size##listName = 0;\
	_head##listName = 0;\
	_tail##listName = 0;\
}

#define CLLIST_PUSHFRONT(listName, elem) {\
	if(_size##listName < _capacity##listName) {\
		_mem##listName[_head##listName] = elem;\
		_head##listName = _head##listName? (_head##listName - 1) : (_capacity##listName - 1);\
		if(!_size##listName)\
			_tail##listName = (_tail##listName + 1) % _capacity##listName;\
		_size##listName++;\
	}\
}

#define CLLIST_PUSHBACK(listName, elem) {\
	if(_size##listName < _capacity##listName) {\
		_mem##listName[_tail##listName] = elem;\
		_tail##listName = (_tail##listName + 1) % _capacity##listName;\
		if(!_size##listName)\
			_head##listName = _head##listName? (_head##listName - 1) : (_capacity##listName - 1);\
		_size##listName++;\
	}\
}

#define CLLIST_POPFRONT(listName) {\
	if(_size##listName) {\
		_head##listName = (_head##listName + 1) % _capacity##listName;\
		_size##listName--;\
\
		if(!_size##listName) {\
			_tail##listName = _tail##listName? (_tail##listName - 1) : (_capacity##listName - 1);\
		}\
	}\
}

#define CLLIST_ISEMPTY(listName) (!_size##listName)

#define CLLIST_FRONT(listName) (_size##listName? _mem##listName[(_head##listName + 1) % _capacity##listName] : -1)

__kernel void brandesLoop(
	__global int * restrict adjList, __global unsigned int * restrict adjOffsets, unsigned int graphSize,
	//__local int * restrict listSMem, unsigned int listSMemSize,
	//__local int * restrict listQMem, unsigned int listQMemSize,
	//__local int * restrict listsPMem, unsigned int listsPMemSliceSize,
	__global int * restrict listSMem,
	__global int * restrict listQMem,
	__global int * restrict listsPMem,
	__global FP_T * restrict cb
#ifdef TARGET_CPU
	,
	unsigned short chunkSize, unsigned short chunkSizeRem
#endif
) {
	//__local int listSMem[LIST_MAX_SIZE + 1];
	//__local int listQMem[LIST_MAX_SIZE + 1];
	//__local int listsPMem[LIST_SIZE * LIST_SIZE];
	CLLIST_INIT(S, listSMem, LIST_MAX_SIZE + 1, __global int);
	CLLIST_INIT(Q, listQMem, LIST_MAX_SIZE + 1, __global int);
	CLLIST_VECINIT(P, listsPMem, LIST_MAX_SIZE, __global int, LIST_MAX_SIZE);

	int i, s, w, t, v;
	int sigma[LIST_MAX_SIZE];
	int d[LIST_MAX_SIZE];
	FP_T delta[LIST_MAX_SIZE];

#ifdef TARGET_CPU
	unsigned int gid = get_global_id(0);
	unsigned short chunkStart, chunkStop;

	if(gid < chunkSizeRem) {
		chunkStart = gid * (chunkSize + 1);
		chunkStop = chunkStart + chunkSize + 1;
	}
	else {
		chunkStart = (gid * chunkSize) + chunkSizeRem;
		chunkStop = chunkStart + chunkSize;
	}
#endif
	
//#pragma unroll 2
#ifdef TARGET_CPU
	for(s = chunkStart; s < chunkStop; s++) {
#else
	for(s = 0; s < graphSize; s++) {
#endif
		CLLIST_RESET(S);
		CLLIST_RESET(Q);
//#pragma unroll
		for(w = 0; w < graphSize; w++)
				CLLIST_RESET(P[w]);

//#pragma unroll
		for(t = 0; t < graphSize; t++) {
				sigma[t] = 0;
				d[t] = -1;
		}
		sigma[s] = 1;
		d[s] = 0;

		CLLIST_PUSHBACK(Q, s);

#pragma unroll 4
		while(!CLLIST_ISEMPTY(Q)) {
			v = CLLIST_FRONT(Q);
			CLLIST_POPFRONT(Q);
			CLLIST_PUSHFRONT(S, v);

//#pragma unroll 4
			for(i = adjOffsets[v]; i < adjOffsets[v + 1]; i++) {
				w = adjList[i];
				if(d[w] < 0) {
					CLLIST_PUSHBACK(Q, w);
					d[w] = d[v] + 1;
				}

				if((d[v] + 1) == d[w]) {
					sigma[w] = sigma[w] + sigma[v];
					CLLIST_PUSHBACK(P[w], v);
				}
			}
		}

//#pragma unroll
		for(v = 0; v < graphSize; v++)
				delta[v] = 0;

#pragma unroll 4
		while(!CLLIST_ISEMPTY(S)) {
			w = CLLIST_FRONT(S);
			CLLIST_POPFRONT(S);

//#pragma unroll 4
			while(!CLLIST_ISEMPTY(P[w])) {
				v = CLLIST_FRONT(P[w]);
				CLLIST_POPFRONT(P[w]);

				delta[v] = delta[v] + ((sigma[v] / ((FP_T) sigma[w])) * (1 + delta[w]));
			}

			if(w != s) {
#ifdef TARGET_CPU
				cb[w + (graphSize * gid)] += delta[w + (graphSize * gid)];
#else
				cb[w] += delta[w];
#endif
			}
		}
	}
}
