#include <CL/opencl.h>
#include "AOCLUtils/aocl_utils.h"
#include "zstd_opencl_mgr.h"

using namespace aocl_utils;

#define STRING_BUFFER_LEN 1024

// OpenCL runtime configuration
static cl_platform_id g_fpga_platform = NULL;
static cl_device_id g_fpga_device = NULL;
static cl_context g_fpga_context = NULL;
static cl_command_queue g_fpga_queue = NULL;
static cl_program g_fpga_program = NULL;
static cl_kernel g_fpga_kernel = NULL;

static cl_mem ip_buff = NULL;
static cl_mem smHashIndexArray_buff = NULL;
static cl_mem lgHashIndexArray_buff = NULL;

static void device_info_ulong(cl_device_id device, cl_device_info param, const char* name);
static void device_info_uint(cl_device_id device, cl_device_info param, const char* name);
static void device_info_bool(cl_device_id device, cl_device_info param, const char* name);
static void device_info_string(cl_device_id device, cl_device_info param, const char* name);
static void display_device_info(cl_device_id device);

void ZSTD_fillDoubleHashTableCL(size_t ipSize, const BYTE* ip, size_t smHashIndexArraySize, U32* smHashIndexArray, size_t lgHashIndexArraySize, U32* lgHashIndexArray)
{
    cl_int err = CL_SUCCESS;

    //printf("ZSTD_fillDoubleHashTable: workSize:%d \n", ipSize);
    err = clEnqueueWriteBuffer(g_fpga_queue, ip_buff, CL_TRUE, 0, ipSize * sizeof(BYTE), ip, 0, NULL, NULL);
    checkError(err, "Failed to clEnqueueWriteBuffer");

    //printf("ZSTD_fillDoubleHashTable: clEnqueueNDRangeKernel() called.\n");
    cl_int ret = clEnqueueNDRangeKernel(g_fpga_queue, g_fpga_kernel, 1, NULL, &smHashIndexArraySize, NULL, 0, NULL, NULL);
    cl_int retFinish = clFinish(g_fpga_queue);

    //printf("ZSTD_fillDoubleHashTable: ret = %d\n", ret);
    err = clEnqueueReadBuffer(g_fpga_queue, smHashIndexArray_buff, CL_TRUE, 0, smHashIndexArraySize * sizeof(U32), smHashIndexArray, 0, NULL, NULL);
    checkError(err, "Failed to clEnqueueReadBuffer");
    err = clEnqueueReadBuffer(g_fpga_queue, lgHashIndexArray_buff, CL_TRUE, 0, lgHashIndexArraySize * sizeof(U32), lgHashIndexArray, 0, NULL, NULL);
    checkError(err, "Failed to clEnqueueReadBuffer");
}

int createOpenCLComponents(void)
{
    cl_int status;

    if (!setCwdToExeDir()) {
        return false;
    }

    // Get the OpenCL platform.
    g_fpga_platform = findPlatform("Intel(R) FPGA SDK for OpenCL(TM)");
    if (g_fpga_platform == NULL) {
        printf("ERROR: Unable to find Intel(R) FPGA OpenCL platform.\n");
        return ZSTD_OPENCL_NG;
    }

    // User-visible output - Platform information
    {
        char char_buffer[STRING_BUFFER_LEN];
        printf("Querying platform for info:\n");
        printf("==========================\n");
        clGetPlatformInfo(g_fpga_platform, CL_PLATFORM_NAME, STRING_BUFFER_LEN, char_buffer, NULL);
        printf("%-40s = %s\n", "CL_PLATFORM_NAME", char_buffer);
        clGetPlatformInfo(g_fpga_platform, CL_PLATFORM_VENDOR, STRING_BUFFER_LEN, char_buffer, NULL);
        printf("%-40s = %s\n", "CL_PLATFORM_VENDOR ", char_buffer);
        clGetPlatformInfo(g_fpga_platform, CL_PLATFORM_VERSION, STRING_BUFFER_LEN, char_buffer, NULL);
        printf("%-40s = %s\n\n", "CL_PLATFORM_VERSION ", char_buffer);
    }

    // Query the available OpenCL devices.
    scoped_array<cl_device_id> devices;
    cl_uint num_devices;

    devices.reset(getDevices(g_fpga_platform, CL_DEVICE_TYPE_ALL, &num_devices));

    // We'll just use the first device.
    g_fpga_device = devices[0];

    // Display some device information.
    display_device_info(g_fpga_device);

    // Create the context.
    g_fpga_context = clCreateContext(NULL, 1, &g_fpga_device, NULL, NULL, &status);
    checkError(status, "Failed to create context");

    // Create the command queue.
#if 1 // For OpenCL 1.0
    g_fpga_queue = clCreateCommandQueue(g_fpga_context, g_fpga_device, CL_QUEUE_PROFILING_ENABLE, &status);
#else // For OpenCL 2.0
    // cl_queue_properties queueProperties = CL_QUEUE_PROFILING_ENABLE;
    // g_fpga_queue = clCreateCommandQueueWithProperties(g_fpga_context, g_fpga_device, &queueProperties, &status);
#endif
    checkError(status, "Failed to create command queue");

    // Create the program.
    //std::string binary_file = getBoardBinaryFile("hello_world", g_fpga_device);
    //std::string binary_file = getBoardBinaryFile("yangle", g_fpga_device);
    std::string binary_file = getBoardBinaryFile("zstdDoubleFast", g_fpga_device);
    printf("Using AOCX: %s\n", binary_file.c_str());
    g_fpga_program = createProgramFromBinary(g_fpga_context, binary_file.c_str(), &g_fpga_device, 1);

    // Build the program that was just created.
    status = clBuildProgram(g_fpga_program, 0, NULL, "", NULL, NULL);
    checkError(status, "Failed to build program");

    // Create the kernel - name passed in here must match kernel name in the
    // original CL file, that was compiled into an AOCX file using the AOC tool
    //const char *kernel_name = "fillDoubleHashTable";  // Kernel name, as defined in the CL file
    const char *kernel_name = "ZSTD_fillDoubleHashTable_cl";
    g_fpga_kernel = clCreateKernel(g_fpga_program, kernel_name, &status);
    printf("clCreateKernel: status = %d\n", status);

	size_t ipSize = 1 << 17;
	size_t workItemMax = ipSize / 3;

    /* create buffer for parameters */
    ip_buff = clCreateBuffer(g_fpga_context, CL_MEM_READ_ONLY, ipSize, NULL, &status);
    checkError(status, "Failed to create buffer");
    smHashIndexArray_buff = clCreateBuffer(g_fpga_context, CL_MEM_WRITE_ONLY, workItemMax * sizeof(U32), NULL, &status);
    checkError(status, "Failed to create buffer");
    lgHashIndexArray_buff = clCreateBuffer(g_fpga_context, CL_MEM_WRITE_ONLY, workItemMax * sizeof(U32) * 3, NULL, &status);
    checkError(status, "Failed to create buffer");

	cl_uint hBitsL = 17;
	cl_uint hBitsS = 16;

    /* set kernel arg */
    //printf("ZSTD_fillDoubleHashTable: clSetKernelArg() called.\n");
    clSetKernelArg(g_fpga_kernel, 0, sizeof(cl_mem), &ip_buff);
    clSetKernelArg(g_fpga_kernel, 1, sizeof(cl_mem), &lgHashIndexArray_buff);
    clSetKernelArg(g_fpga_kernel, 2, sizeof(cl_mem), &smHashIndexArray_buff);
    clSetKernelArg(g_fpga_kernel, 3, sizeof(cl_uint), &hBitsL);
    clSetKernelArg(g_fpga_kernel, 4, sizeof(cl_uint), &hBitsS);

    return ZSTD_OPENCL_OK;
}

void releaseOpenCLComponents(void)
{
    cleanup();
}


// Free the resources allocated during initialization
void cleanup() {
	if (ip_buff) {
		clReleaseMemObject(ip_buff);
	}
	if (smHashIndexArray_buff) {
		clReleaseMemObject(smHashIndexArray_buff);
	}
	if (lgHashIndexArray_buff) {
		clReleaseMemObject(lgHashIndexArray_buff);
	}

    if (g_fpga_kernel) {
        clReleaseKernel(g_fpga_kernel);
    }
    if (g_fpga_program) {
        clReleaseProgram(g_fpga_program);
    }
    if (g_fpga_queue) {
        clReleaseCommandQueue(g_fpga_queue);
    }
    if (g_fpga_context) {
        clReleaseContext(g_fpga_context);
    }
}

// Helper functions to display parameters returned by OpenCL queries
static void device_info_ulong(cl_device_id device, cl_device_info param, const char* name) {
    cl_ulong a;
    clGetDeviceInfo(device, param, sizeof(cl_ulong), &a, NULL);
    printf("%-40s = %lu\n", name, a);
}
static void device_info_uint(cl_device_id device, cl_device_info param, const char* name) {
    cl_uint a;
    clGetDeviceInfo(device, param, sizeof(cl_uint), &a, NULL);
    printf("%-40s = %u\n", name, a);
}
static void device_info_bool(cl_device_id device, cl_device_info param, const char* name) {
    cl_bool a;
    clGetDeviceInfo(device, param, sizeof(cl_bool), &a, NULL);
    printf("%-40s = %s\n", name, (a ? "true" : "false"));
}
static void device_info_string(cl_device_id device, cl_device_info param, const char* name) {
    char a[STRING_BUFFER_LEN];
    clGetDeviceInfo(device, param, STRING_BUFFER_LEN, &a, NULL);
    printf("%-40s = %s\n", name, a);
}

// Query and display OpenCL information on device and runtime environment
static void display_device_info(cl_device_id device) {

    printf("Querying device for info:\n");
    printf("========================\n");
    device_info_string(device, CL_DEVICE_NAME, "CL_DEVICE_NAME");
    device_info_string(device, CL_DEVICE_VENDOR, "CL_DEVICE_VENDOR");
    device_info_uint(device, CL_DEVICE_VENDOR_ID, "CL_DEVICE_VENDOR_ID");
    device_info_string(device, CL_DEVICE_VERSION, "CL_DEVICE_VERSION");
    device_info_string(device, CL_DRIVER_VERSION, "CL_DRIVER_VERSION");
    device_info_uint(device, CL_DEVICE_ADDRESS_BITS, "CL_DEVICE_ADDRESS_BITS");
    device_info_bool(device, CL_DEVICE_AVAILABLE, "CL_DEVICE_AVAILABLE");
    device_info_bool(device, CL_DEVICE_ENDIAN_LITTLE, "CL_DEVICE_ENDIAN_LITTLE");
    device_info_ulong(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, "CL_DEVICE_GLOBAL_MEM_CACHE_SIZE");
    device_info_ulong(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, "CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE");
    device_info_ulong(device, CL_DEVICE_GLOBAL_MEM_SIZE, "CL_DEVICE_GLOBAL_MEM_SIZE");
    device_info_bool(device, CL_DEVICE_IMAGE_SUPPORT, "CL_DEVICE_IMAGE_SUPPORT");
    device_info_ulong(device, CL_DEVICE_LOCAL_MEM_SIZE, "CL_DEVICE_LOCAL_MEM_SIZE");
    device_info_ulong(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, "CL_DEVICE_MAX_CLOCK_FREQUENCY");
    device_info_ulong(device, CL_DEVICE_MAX_COMPUTE_UNITS, "CL_DEVICE_MAX_COMPUTE_UNITS");
    device_info_ulong(device, CL_DEVICE_MAX_CONSTANT_ARGS, "CL_DEVICE_MAX_CONSTANT_ARGS");
    device_info_ulong(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, "CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE");
    device_info_uint(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, "CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS");
    device_info_uint(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, "CL_DEVICE_MEM_BASE_ADDR_ALIGN");
    device_info_uint(device, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE, "CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE");
    device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR");
    device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT");
    device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT");
    device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG");
    device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT");
    device_info_uint(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, "CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE");
    device_info_ulong(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, "CL_DEVICE_MAX_WORK_GROUP_SIZE");

    {
        cl_uint dim = 0;
        cl_ulong* workItemSizes = NULL;
        clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(dim), &dim, NULL);
        
        workItemSizes = new cl_ulong[dim];
        memset(workItemSizes, 0, sizeof(cl_ulong) * dim);
        clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(cl_ulong)* dim, workItemSizes, NULL);
        printf("CL_DEVICE_MAX_WORK_ITEM_SIZES: \n");
        for (cl_uint i = 0; i < dim; i++)
        {
            printf("%lu, ", workItemSizes[i]);
        }
        printf("\n");
    }

    {
        cl_command_queue_properties ccp;
        clGetDeviceInfo(device, CL_DEVICE_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties), &ccp, NULL);
        printf("%-40s = %s\n", "Command queue out of order? ", ((ccp & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) ? "true" : "false"));
        printf("%-40s = %s\n", "Command queue profiling enabled? ", ((ccp & CL_QUEUE_PROFILING_ENABLE) ? "true" : "false"));
    }
}
