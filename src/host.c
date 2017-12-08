#include <CL/opencl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "brandes.h"

#include "common.h"
#include "graph.h"

#define MAX_STR_SZ 256

#if double == FP_T
#define FP_T_PRINTF_STRING "%lf"
#define FP_T_BUILD_STRING "-DFP_T=double"
#else
#define FP_T_PRINTF_STRING "%f"
#define FP_T_BUILD_STRING "-DFP_T=float"
#endif

/**
 * @brief Swap the extension of a file (or add it if the file has none.
 * @param inputFilename The input filename.
 * @param extesion The new extension, without initial dot.
 * @return The filename with the new extension, developer should free it after use.
 */
char *swapOrAddExtension(char *inputFilename, char *extension) {
	int i;
	char *outputFilename = NULL;

	/* Find for the dot in the input filename */
	for(i = strnlen(inputFilename, MAX_STR_SZ); i >= 0; i--) {
		/* Dot found, make swap */
		if('.' == inputFilename[i]) {
			outputFilename = calloc(i + strnlen(extension, MAX_STR_SZ) + 2, sizeof(char));
			strncpy(outputFilename, inputFilename, i + 1);
			strcpy(&outputFilename[i + 1], extension);
			break;
		}
	}

	/* No dot found, just add the extension */
	if(-1 == i) {
		outputFilename = calloc(strnlen(inputFilename, MAX_STR_SZ) + strnlen(extension, MAX_STR_SZ) + 2, sizeof(char));
		strcpy(outputFilename, inputFilename);
		outputFilename[strnlen(inputFilename, MAX_STR_SZ)] = '.';
		strcpy(&outputFilename[strnlen(inputFilename, MAX_STR_SZ) + 1], extension);
	}

	return outputFilename;
}

/**
 * @brief Standard statements for function error handling and printing.
 *
 * @param funcName Function name that failed.
 */
#define FUNCTION_ERROR_STATEMENTS(funcName) {\
	rv = EXIT_FAILURE;\
	PRINT_FAIL();\
	fprintf(stderr, "Error: %s failed with return code %d.\n", funcName, fRet);\
}

/**
 * @brief Standard statements for POSIX error handling and printing.
 *
 * @param arg Arbitrary string to the printed at the end of error string.
 */
#define POSIX_ERROR_STATEMENTS(arg) {\
	rv = EXIT_FAILURE;\
	PRINT_FAIL();\
	fprintf(stderr, "Error: %s: %s\n", strerror(errno), arg);\
}

int main(int argc, char *argv[]) {
	int rv = EXIT_SUCCESS;

	int i = 0, j;
	unsigned int zero = 0;

	char *inputFilename, *outputFilename = NULL;
	FILE *inputFile = NULL, *outputFile = NULL;
	int n, m, np;
	graph_t *graph = NULL;
	int *compactGraph = NULL;
	unsigned int *compactGraphOffsets = NULL;
	FP_T *cb = NULL;
#ifdef TARGET_CPU
	FP_T *cbReduced = NULL;
	unsigned int chunkSize, chunkSizeRem;
#endif

	cl_int platformsLen, devicesLen, fRet;
	cl_platform_id *platforms = NULL;
	cl_device_id *devices = NULL;
	cl_context context = NULL;
	cl_command_queue queueBrandesLoop = NULL;
	FILE *programFile = NULL;
	long programSz;
	char *programContent = NULL;
	cl_int programRet;
	cl_program program = NULL;
	cl_kernel kernelBrandesLoop = NULL;
	cl_mem adjListK = NULL;
	cl_mem adjOffsetsK = NULL;
	cl_mem listSMemK = NULL;
	cl_mem listQMemK = NULL;
	cl_mem listsPMemK = NULL;
	cl_mem cbK = NULL;
	cl_uint workDim = 1;
	size_t globalSize[1] = {1};

	/* Check if command line arguments were passed correctly */
	ASSERT_CALL((2 == argc) || (3 == argc), fprintf(stderr, "Usage: %s INPUTFILE [WORKITEMS]\n", argv[0]));
	inputFilename = argv[1];
	if(3 == argc)
		globalSize[0] = atoi(argv[2]);
	outputFilename = swapOrAddExtension(inputFilename, "btw");

	/* Open input and output files and check their existence */
	inputFile = fopen(inputFilename, "r");
	ASSERT_CALL(inputFile, fprintf(stderr, "Error: %s: %s\n", strerror(errno), inputFilename));
	outputFile = fopen(outputFilename, "w");
	ASSERT_CALL(outputFile, fprintf(stderr, "Error: %s: %s\n", strerror(errno), outputFilename));

	/* Read file header and allocate stuff */
	fscanf(inputFile, "%d", &n);
	fscanf(inputFile, "%d", &m);
	np = n + 1;
	graph_create(&graph, n, m);

	/* Read edges from file */
	unsigned int orig, dest;
	for(i = 0; i < m; i++) {
		fscanf(inputFile, "%d %d", &orig, &dest);
		graph_putEdge(graph, orig, dest);
		graph_putEdge(graph, dest, orig);
	}
	compactGraph = graph_compact(graph, &compactGraphOffsets);
	compactGraphOffsets = realloc(compactGraphOffsets, np * sizeof(int));
	compactGraphOffsets[n] = 2 * m;
	cb = malloc(n * globalSize[0] * sizeof(FP_T));
#ifdef TARGET_CPU
	cbReduced = malloc(n * sizeof(FP_T));
	chunkSize = n / globalSize[0];
	chunkSizeRem = n % globalSize[0];
#endif
	np = n + 1;

	/* Get platforms IDs */
	PRINT_STEP("Getting platforms IDs...");
	fRet = clGetPlatformIDs(0, NULL, &platformsLen);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clGetPlatformIDs"));
	platforms = malloc(platformsLen * sizeof(cl_platform_id));
	fRet = clGetPlatformIDs(platformsLen, platforms, NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clGetPlatformIDs"));
	PRINT_SUCCESS();

	/* Get devices IDs for first platform availble */
	PRINT_STEP("Getting devices IDs for first platform...");
	fRet = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, 0, NULL, &devicesLen);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clGetDevicesIDs"));
	devices = malloc(devicesLen * sizeof(cl_device_id));
	fRet = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, devicesLen, devices, NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clGetDevicesIDs"));
	PRINT_SUCCESS();

	/* Create context for first available device */
	PRINT_STEP("Creating context...");
	context = clCreateContext(NULL, 1, devices, NULL, NULL, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateContext"));
	PRINT_SUCCESS();

	/* Create command queue for kernelBrandesLoop */
	PRINT_STEP("Creating command queueBrandesLoop...");
	queueBrandesLoop = clCreateCommandQueue(context, devices[0], 0, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateCommandQueue"));
	PRINT_SUCCESS();

#ifdef TARGET_CPU
	/* Open binary file */
	PRINT_STEP("Opening program binary...");
	programFile = fopen("brandes.cl", "rb");
	ASSERT_CALL(programFile, POSIX_ERROR_STATEMENTS("program.aocx"));
	PRINT_SUCCESS();
#else
	/* Open binary file */
	PRINT_STEP("Opening program binary...");
	programFile = fopen("program.aocx", "rb");
	ASSERT_CALL(programFile, POSIX_ERROR_STATEMENTS("program.aocx"));
	PRINT_SUCCESS();
#endif

	/* Get size and read file */
	PRINT_STEP("Reading program binary...");
	fseek(programFile, 0, SEEK_END);
	programSz = ftell(programFile);
	fseek(programFile, 0, SEEK_SET);
	programContent = malloc(programSz);
	fread(programContent, programSz, 1, programFile);
	fclose(programFile);
	programFile = NULL;
	PRINT_SUCCESS();

#ifdef TARGET_CPU
	/* Create program from source file */
	PRINT_STEP("Creating program from source...");
	program = clCreateProgramWithSource(context, 1, (const char **) &programContent, &programSz, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateProgramWithSource"));
	PRINT_SUCCESS();

	/* Build program */
	PRINT_STEP("Building program...");
	fRet = clBuildProgram(program, 1, devices, FP_T_BUILD_STRING " -DTARGET_CPU", NULL, NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clBuildProgram"));
	PRINT_SUCCESS();
#else
	/* Create program from binary file */
	PRINT_STEP("Creating program from binary...");
	program = clCreateProgramWithBinary(context, 1, devices, &programSz, (const unsigned char **) &programContent, &programRet, &fRet);
	ASSERT_CALL(CL_SUCCESS == programRet, FUNCTION_ERROR_STATEMENTS("clCreateProgramWithBinary (when loading binary)"));
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateProgramWithBinary"));
	PRINT_SUCCESS();

	/* Build program */
	PRINT_STEP("Building program...");
	fRet = clBuildProgram(program, 1, devices, NULL, NULL, NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clBuildProgram"));
	PRINT_SUCCESS();
#endif

	/* Create kernelBrandesLoop */
	PRINT_STEP("Creating kernel \"brandesLoop\" from program...");
	kernelBrandesLoop = clCreateKernel(program, "brandesLoop", &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateKernel"));
	PRINT_SUCCESS();

	/* Create input and output buffers */
	PRINT_STEP("Creating buffers...");
	adjListK = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,  2 * m * sizeof(int), compactGraph, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateBuffer (adjListK)"));
	adjOffsetsK = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,  np * sizeof(unsigned int), compactGraphOffsets, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateBuffer (adjOffsetsK)"));
	listSMemK = clCreateBuffer(context, CL_MEM_WRITE_ONLY,  LIST_MAX_SIZE * sizeof(int), NULL, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateBuffer (listSMemK)"));
	listQMemK = clCreateBuffer(context, CL_MEM_WRITE_ONLY,  LIST_MAX_SIZE * sizeof(int), NULL, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateBuffer (listQMemK)"));
	listsPMemK = clCreateBuffer(context, CL_MEM_WRITE_ONLY,  LISTS_P_MAX_SIZE * sizeof(int), NULL, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateBuffer (listsPMemK)"));
	cbK = clCreateBuffer(context, CL_MEM_READ_WRITE,  n * globalSize[0] * sizeof(FP_T), NULL, &fRet);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clCreateBuffer (cbK)"));
	PRINT_SUCCESS();

	/* Set kernel arguments for brandesLoop */
	PRINT_STEP("Setting kernel arguments for \"brandesLoop\"...");
	fRet = clSetKernelArg(kernelBrandesLoop, 0, sizeof(cl_mem), &adjListK);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (adjListK)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 1, sizeof(cl_mem), &adjOffsetsK);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (adjOffsetsK)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 2, sizeof(unsigned int), &n);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (n)"));
#if 0
	fRet = clSetKernelArg(kernelBrandesLoop, 5, (n + 1) * sizeof(int), NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (__local 5)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 6, sizeof(unsigned int), &np);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (np)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 7, (n + 1) * sizeof(int), NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (__local 7)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 8, sizeof(unsigned int), &np);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (np)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 9, (n + 1) * sizeof(int), NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (__local 9)"));
#endif
	fRet = clSetKernelArg(kernelBrandesLoop, 3, sizeof(cl_mem), &listSMemK);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (listSMemK)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 4, sizeof(cl_mem), &listQMemK);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (listQMemK)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 5, sizeof(cl_mem), &listsPMemK);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (listsPMemK)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 6, sizeof(cl_mem), &cbK);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (cbK)"));
#ifdef TARGET_CPU
	fRet = clSetKernelArg(kernelBrandesLoop, 7, sizeof(unsigned int), &chunkSize);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (chunkSize)"));
	fRet = clSetKernelArg(kernelBrandesLoop, 8, sizeof(unsigned int), &chunkSizeRem);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clSetKernelArg (chunkSizeRem)"));
#endif
	PRINT_SUCCESS();

	PRINT_STEP("Running kernels...");
	fRet = clEnqueueNDRangeKernel(queueBrandesLoop, kernelBrandesLoop, workDim, NULL, globalSize, NULL, 0, NULL, NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clEnqueueNDRangeKernel"));
	clFinish(queueBrandesLoop);
	PRINT_SUCCESS();

	/* Get output buffers */
	PRINT_STEP("Getting kernel arguments...");
	fRet = clEnqueueReadBuffer(queueBrandesLoop, cbK, CL_TRUE, 0, n * globalSize[0] * sizeof(FP_T), cb, 0, NULL, NULL);
	ASSERT_CALL(CL_SUCCESS == fRet, FUNCTION_ERROR_STATEMENTS("clEnqueueReadBuffer"));
	PRINT_SUCCESS();

#ifdef TARGET_CPU
	for(i = 0; i < n; i++) {
		cbReduced[i] = 0;
		for(j = 0; j < globalSize[0]; j++)
			cbReduced[i] += cb[i + (j * n)];
		printf("cb[%d] = " FP_T_PRINTF_STRING "\n", i, cbReduced[i] / 2.0);
	}
#else
	for(i = 0; i < n; i++)
		printf("cb[%d] = " FP_T_PRINTF_STRING "\n", i, cb[i] / 2.0);
#endif

_err:
	/* Print build log if build failed */
	if((rv != EXIT_SUCCESS) && program && devices) {
		size_t buildLogLen;
		char *buildLog;

		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogLen);
		buildLog = malloc(buildLogLen);
		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, buildLogLen, buildLog, NULL);

		printf("Build output:\n%s", buildLog);

		free(buildLog);
	}

	/* Dealloc buffers */
	if(cbK)
		clReleaseMemObject(cbK);
	if(listsPMemK)
		clReleaseMemObject(listsPMemK);
	if(listQMemK)
		clReleaseMemObject(listQMemK);
	if(listSMemK)
		clReleaseMemObject(listSMemK);
	if(adjOffsetsK)
		clReleaseMemObject(adjOffsetsK);
	if(adjListK)
		clReleaseMemObject(adjListK);

	/* Dealloc kernels */
	if(kernelBrandesLoop)
		clReleaseKernel(kernelBrandesLoop);

	/* Dealloc program */
	if(program)
		clReleaseProgram(program);
	if(programContent)
		free(programContent);
	if(programFile)
		fclose(programFile);

	/* Dealloc queues */
	if(queueBrandesLoop)
		clReleaseCommandQueue(queueBrandesLoop);

	/* Last OpenCL variables */
	if(context)
		clReleaseContext(context);
	if(devices)
		free(devices);
	if(platforms)
		free(platforms);

#ifdef TARGET_CPU
	if(cbReduced)
		free(cbReduced);
#endif
	if(cb)
		free(cb);
	if(compactGraphOffsets)
		free(compactGraphOffsets);
	if(compactGraph)
		free(compactGraph);
	if(graph)
		graph_destroy(&graph);

	if(outputFile)
		fclose(outputFile);
	if(inputFile)
		fclose(inputFile);
	if(outputFilename)
		free(outputFilename);

	return rv;
}
