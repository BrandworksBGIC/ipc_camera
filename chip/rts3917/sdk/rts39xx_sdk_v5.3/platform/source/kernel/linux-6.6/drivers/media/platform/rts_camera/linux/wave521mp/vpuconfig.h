//-----------------------------------------------------------------------------
// COPYRIGHT (C) 2020   CHIPS&MEDIA INC. ALL RIGHTS RESERVED
//
// This file is distributed under BSD 3 clause and LGPL2.1 (dual license)
// SPDX License Identifier: BSD-3-Clause
// SPDX License Identifier: LGPL-2.1-only
//
// The entire notice above must be reproduced on all authorized copies.
//
// Description  :
//-----------------------------------------------------------------------------

#ifndef _VPU_CONFIG_H_
#define _VPU_CONFIG_H_

/* config */

#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WIN32) || defined(__MINGW32__)
#	define PLATFORM_WIN32
#elif defined(linux) || defined(__linux) || defined(ANDROID)
#	define PLATFORM_LINUX
#elif defined(unix) || defined(__unix)
#   define PLATFORM_QNX
#else
#	define PLATFORM_NON_OS
#endif

#if defined(_MSC_VER)
#	include <windows.h>
#	define inline _inline
#elif defined(__GNUC__)
#elif defined(__ARMCC__)
#else
#  error "Unknown compiler."
#endif

#define API_VERSION_MAJOR       5
#define API_VERSION_MINOR       5
#define API_VERSION_PATCH       68
#define API_VERSION             ((API_VERSION_MAJOR<<16) | (API_VERSION_MINOR<<8) | API_VERSION_PATCH)

#if defined(PLATFORM_NON_OS) || defined (ANDROID) || defined(MFHMFT_EXPORTS) || defined(PLATFORM_QNX) || defined(CNM_SIM_PLATFORM)
//#define SUPPORT_FFMPEG_DEMUX
#else
#define SUPPORT_FFMPEG_DEMUX
#endif
#define SUPPORT_ENC_SEI_VUI_HRD
#define REPORT_MVCOL
//------------------------------------------------------------------------------
// COMMON
//------------------------------------------------------------------------------
#if defined(linux) || defined(__linux) || defined(ANDROID) || defined(CNM_FPGA_HAPS_INTERFACE)
#define SUPPORT_MULTI_INST_INTR
// #define SUPPORT_MULTI_INST_INTR_IN_API
#endif
#if defined(CNM_FPGA_HAPS_INTERFACE) || defined(CNM_FPGA_SIM_PLATFORM)
#else
#if defined(linux) || defined(__linux) || defined(ANDROID)
#define SUPPORT_INTERRUPT
#endif
#endif



// do not define BIT_CODE_FILE_PATH in case of multiple product support. because wave410 and coda980 has different firmware binary format.
#define CORE_0_BIT_CODE_FILE_PATH   "coda960.out"     // for coda960
#define CORE_1_BIT_CODE_FILE_PATH   "coda980.out"     // for coda980
#define CORE_6_BIT_CODE_FILE_PATH   "chagall.bin"     // for wave521
#define CORE_7_BIT_CODE_FILE_PATH "vincent.bin" // for wave517

//------------------------------------------------------------------------------
// OMX
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
// WAVE521C
//------------------------------------------------------------------------------
//#define SUPPORT_SOURCE_RELEASE_INTERRUPT
//#define SUPPORT_READ_COMP_IN_ENCODER

#define SUPPORT_FRAME_DROP_PROTECT

#define SUPPORT_REALTEK
#ifdef SUPPORT_REALTEK
//#define SUPPORT_W5ENC_REPORT_VAR
#define SUPPORT_CHANGE_QP_OFFSET
#endif /* SUPPORT_REALTEK */

//#define SUPPORT_HW_UART
#ifdef SUPPORT_HW_UART
#define WAVE521_HW_UART_BAUDRATE	38400
#endif




//#define SUPPORT_SW_UART
//#define SUPPORT_SW_UART_V2	// WAVE511 or WAVE521C
// #define SUPPORT_SW_UART_ON_NONOS
#ifdef SUPPORT_SW_UART_ON_NONOS
    #if defined(SUPPORT_SW_UART) || defined(SUPPORT_SW_UART_V2)
    #else
    #error "SUPPORT_SW_UART_ON_NONOS define needs (#if defined(SUPPORT_SW_UART_V2) || defined(SUPPORT_SW_UART))"
    #endif
#endif
#ifdef SUPPORT_SW_UART_ON_NONOS
#ifndef SUPPORT_MULTI_INST_INTR_IN_API
#define SUPPORT_MULTI_INST_INTR_IN_API
#endif
#undef SUPPORT_MULTI_INST_INTR
#undef SUPPORT_INTERRUPT
#endif

/* vpuconfig */

#define ENC_STREAM_BUF_COUNT 2
#define ENC_STREAM_BUF_SIZE  0x200000

#define BODA950_CODE                    0x9500
#define CODA960_CODE                    0x9600
#define CODA980_CODE                    0x9800

#define WAVE517_CODE                    0x5170
#define WAVE511_CODE                    0x5110
#define WAVE521_CODE                    0x5210
#define WAVE521C_CODE                   0x521c
#define WAVE521C_DUAL_CODE              0x521d  // wave521 dual core

#define PRODUCT_CODE_W_SERIES(x)        (x == WAVE517_CODE || x == WAVE511_CODE || x == WAVE521_CODE || x == WAVE521C_CODE || x == WAVE521C_DUAL_CODE)
#define PRODUCT_CODE_NOT_W_SERIES(x)    (x == BODA950_CODE || x == CODA960_CODE || x == CODA980_CODE)

#define WAVE5_MAX_CODE_BUF_SIZE         (1024*1024)
#define WAVE517_WORKBUF_SIZE            (2*1024*1024)
#define WAVE521ENC_WORKBUF_SIZE         (128*1024)      //HEVC 128K, AVC 40K
#define WAVE521DEC_WORKBUF_SIZE         (1784*1024)

#define MAX_INST_HANDLE_SIZE            48              /* DO NOT CHANGE THIS VALUE */
#define MAX_NUM_INSTANCE                4
#define MAX_NUM_VPU_CORE                1
#define MAX_NUM_VCORE                   1

    #define MAX_ENC_AVC_PIC_WIDTH       4096
    #define MAX_ENC_AVC_PIC_HEIGHT      2304
#define MAX_ENC_PIC_WIDTH               4096
#define MAX_ENC_PIC_HEIGHT              2304
#define MIN_ENC_PIC_WIDTH               96
#define MIN_ENC_PIC_HEIGHT              16

// for WAVE420
#define W4_MIN_ENC_PIC_WIDTH            256
#define W4_MIN_ENC_PIC_HEIGHT           128
#define W4_MAX_ENC_PIC_WIDTH            8192
#define W4_MAX_ENC_PIC_HEIGHT           8192

#define MAX_DEC_PIC_WIDTH               4096
#define MAX_DEC_PIC_HEIGHT              2304

#define MAX_CTU_NUM                     0x4000      // CTU num for max resolution = 8192x8192/(64x64)
#define MAX_SUB_CTU_NUM	                (MAX_CTU_NUM*4)
#define MAX_MB_NUM                      0x40000     // MB num for max resolution = 8192x8192/(16x16)

//  Application specific configuration
#if   defined (SUPPORT_DEC_RINGBUFFER_PERFORMANCE)
#define VPU_DEC_TIMEOUT                 (60000*10)
#define VPU_BUSY_CHECK_TIMEOUT          (10000*10)
#else
#define VPU_ENC_TIMEOUT                 60000
#define VPU_DEC_TIMEOUT                 60000
#define VPU_BUSY_CHECK_TIMEOUT          10000
#endif

// codec specific configuration
#define VPU_REORDER_ENABLE              1   // it can be set to 1 to handle reordering DPB in host side.
#define CBCR_INTERLEAVE			        1 //[default 1 for BW checking with CnMViedo Conformance] 0 (chroma separate mode), 1 (chroma interleave mode) // if the type of tiledmap uses the kind of MB_RASTER_MAP. must set to enable CBCR_INTERLEAVE
#define VPU_ENABLE_BWB			        1

#define HOST_ENDIAN                     VDI_128BIT_LITTLE_ENDIAN
#define VPU_FRAME_ENDIAN                HOST_ENDIAN
#define VPU_STREAM_ENDIAN               HOST_ENDIAN
#define VPU_USER_DATA_ENDIAN            HOST_ENDIAN
#define VPU_SOURCE_ENDIAN               HOST_ENDIAN
#define DRAM_BUS_WIDTH                  16


// for WAVE Encoder
#define USE_SRC_PRP_AXI         0
#define USE_SRC_PRI_AXI         1
#define DEFAULT_SRC_AXI         USE_SRC_PRP_AXI

/************************************************************************/
/* VPU COMMON MEMORY                                                    */
/************************************************************************/
#define VLC_BUF_NUM              (3)
#define COMMAND_QUEUE_DEPTH             (1)
#define REPORT_QUEUE_COUNT       COMMAND_QUEUE_DEPTH

        #ifdef SUPPORT_TESTCASE_CQ_16
            #define ENC_SRC_BUF_NUM             (20+COMMAND_QUEUE_DEPTH)
        #else
            #define ENC_SRC_BUF_NUM             20       // [FIX ME]
        #endif

#define ONE_TASKBUF_SIZE_FOR_W5DEC_CQ         (8*1024*1024)   /* upto 8Kx4K, need 8Mbyte per task*/
#define ONE_TASKBUF_SIZE_FOR_W5ENC_CQ         (8*1024*1024)  /* upto 8Kx8K, need 8Mbyte per task.*/
#define ONE_TASKBUF_SIZE_FOR_W511DEC_CQ       (8*1024*1024)  /* upto 8Kx8K, need 8Mbyte per task.*/

/* FW will return one TASKBUF size base on MaxCPB (according to the SPEC), but this size will be quite big depend on profile/level.*/
/* ex) main10, 8kx8k = 180Mbytes will be returned */
/* Thus, if host can set size limitation for one TASKBUF size. (but, small size limitation can cause processing error)  */
#define ONE_TASKBUF_MAX_SIZE_LIMIT_DEC          (8*1024*1024)
#define ONE_TASKBUF_MAX_SIZE_LIMIT_ENC          (20*1024*1024)

    #define ONE_TASKBUF_SIZE_FOR_CQ     0
    #define SIZE_COMMON                 (2*1024*1024)

#define SIZE_CODE_BUFFER		(1*1024*1024)
#define SIZE_HANDLE_PARAM_BUFFER	(260*1024)

//=====4. VPU REPORT MEMORY  ======================//
#define SIZE_REPORT_BUF                 (0x10000)

#define STREAM_END_SIZE                 0
#define STREAM_END_SET_FLAG             0
#define STREAM_END_CLEAR_FLAG           -1
#define EXPLICIT_END_SET_FLAG           -2

#define UPDATE_NEW_BS_BUF               0

#define USE_BIT_INTERNAL_BUF            1
#define USE_IP_INTERNAL_BUF             1
#define USE_DBKY_INTERNAL_BUF           1
#define USE_DBKC_INTERNAL_BUF           1
#define USE_OVL_INTERNAL_BUF            1
#define USE_BTP_INTERNAL_BUF            1
#define USE_ME_INTERNAL_BUF             1

/* WAVE410 only */
#define USE_BPU_INTERNAL_BUF            1
#define USE_VCE_IP_INTERNAL_BUF         1
#define USE_VCE_LF_ROW_INTERNAL_BUF     1

/* WAVE420 only */
#define USE_IMD_INTERNAL_BUF            1
#define USE_RDO_INTERNAL_BUF            1
#define USE_LF_INTERNAL_BUF             1


#define WAVE5_UPPER_PROC_AXI_ID     0x0

#define WAVE5_PROC_AXI_ID           0x0
#define WAVE5_PRP_AXI_ID            0x0
#define WAVE5_FBD_Y_AXI_ID          0x0
#define WAVE5_FBC_Y_AXI_ID          0x0
#define WAVE5_FBD_C_AXI_ID          0x0
#define WAVE5_FBC_C_AXI_ID          0x0
#define WAVE5_SEC_AXI_ID            0x0
#define WAVE5_PRI_AXI_ID            0x0


#endif  /* _VPU_CONFIG_H_ */

