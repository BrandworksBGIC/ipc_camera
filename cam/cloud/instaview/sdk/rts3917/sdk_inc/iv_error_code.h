
#ifndef _INSTA_VIEW_ERROR_CODE_HEADER_
#define _INSTA_VIEW_ERROR_CODE_HEADER_
typedef enum
{
	IV_NoErr = 0,
	IV_ErrParam = -1,
	IV_ErrCreate = -2,
	IV_ErrBind = -3,
	IV_ErrListen = -4,
	IV_ErrConnect = -5,
	IV_ErrSelect = -6,
	IV_Timeout = -7,
	IV_ErrSetopt = -8,
	IV_ErrSize = -9,
	IV_ErrRecv = -10,
	IV_ErrSend = -11,
	IV_ErrMem = -12,
	IV_ErrHeader = -13,
	IV_ErrOpen = -14,
	IV_ErrRead = -15,
	IV_ErrWrite = -16,
	IV_ErrNotFound = -17,
	IV_ErrActive = -18,
	IV_ErrEncode = -19,
	IV_ErrDecode = -20,
	IV_ErrNullData = -21,
	IV_ErrNoInit = -22,
	IV_ErrExist = -23,
	IV_ErrOverflow = -24,
	IV_ErrSdpGen = -25,
	IV_ErrBusy = -26,
	IV_ErrUser = -27,
	IV_ErrWaitIFrame = -28,
	IV_ErrDelete = -29,
	IV_ErrCheckSum = -30,
	IV_ErrNotSuport = -31,
	IV_ErrJoin = -32,
	IV_ErrLiveDelay = -33,
	IV_ErrCacheNotEnough = -34
}IV_Error;
#endif


