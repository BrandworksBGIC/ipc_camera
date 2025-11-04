#ifndef __IPC_JSON_H__
#define __IPC_JSON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <ipc_std.h>
/**
 * Feature Description:
 * 1. Memory overwrite assignment will only be performed when the key exists and the type matches;
 * 2. When parsing an array, if one of the objects fails to parse, the corresponding memory area of the array will not
 * be assigned; For example, an int type or null type suddenly appears in the middle of parsing a string array (unless
 * something goes wrong, this generally does not happen)
 * 3. [Not supported] Parsing and building of null types, because it feels like there is no place to use this thing;
 * 4. The function has done strict checking and matching of the JSON string, but has not checked each parameter input by
 * the function (there is no way to do this with macros);
 * 5. Supports array and object nesting, and currently supports up to three dimensions of arrays (if you need higher
 * dimensions, it can be easily expanded);
 * 6. Due to the re-encapsulation of cjson, the API has lost some flexibility and brought more constraints, especially
 * when it comes to array nesting data; However, the constraints have brought more convenient and batch mutual mapping
 * methods to complex data structures;
 * 7. The key specification only exists once. If the same key exists in the same layer of JSON, only the first one can
 * be parsed; Similarly, if the same key exists in the table when building, the latter key will overwrite the previous
 * key, except for objects; For objects with the same key and dimension, a merging mechanism will be triggered, and the
 * two objects will be merged together;
 * 8. Please refer to the demo for specific usage, where there will be more detailed constraint demonstrations;
 */
/********************************* For internal use ********************************************/

/* The upper limit of multi-dimensional array support, currently three-dimensional, you can change the supported array
 * dimension by changing here */
#define _JSON_MAX_ARRAY_LEVEL 3
#define _JSON_ARRAY1(obj)                                                                                              \
    {                                                                                                                  \
        sizeof(obj[0]), 0, 0                                                                                           \
    }
#define _JSON_ARRAY2(obj)                                                                                              \
    {                                                                                                                  \
        sizeof(obj[0]), sizeof(obj[0][0]), 0                                                                           \
    }
#define _JSON_ARRAY3(obj)                                                                                              \
    {                                                                                                                  \
        sizeof(obj[0]), sizeof(obj[0][0]), sizeof(obj[0][0][0])                                                        \
    }

/* Object description, for tabulation, already aligned */
typedef struct {
    /* Basic information */
    pv8 key;       /* json key */
    vptr addr;     /* Target object address */
    u32 size;      /* Object size (entire) */
    u16 child_num; /* Number of child objects */
    u8 type;       /* json type */
    /* Advanced feature extensions */
    u8 num_size;   /* Variable size of the parsed parameter number/set array parameter number passed externally, such as
                      s/u 8 16 32 64 */
    vptr num_addr; /* Address of the variable indicating the number of parsed parameters/parameters set in the array
                      passed in from the outside */
    u8 check;      /* Flag to check if the value exists or not. If not, the parse fails immediately */
    u8 array_level;                            /* Dimensions of array. Zero means it is not an array */
    u16 array_obj_size[_JSON_MAX_ARRAY_LEVEL]; /* The object size in each dimension of the array */
} ipc_json_t, *ipc_json_p;

/*=================== Extended feature commands =======================*/

/* According to the cmd mapping corresponding macro for extended parameter processing, when expanding the command,
 * because the macro rules cannot be recursive, it can only be compiled through like the following [copy each command
 * and change the name] */
/* Here, in order to stabilize, all advanced expansion data is cleared first */
#define _JSON_EXPAND(obj, cmd, ...) _JSON_CMD_##cmd(obj, ##__VA_ARGS__)
/* Empty command, that is, nothing to do, all parameter deduction will eventually reach here */
#define _JSON_CMD_(obj)
/* BIND_NUM, bind the variable address of [the number of parsed returns] [the size of the generated array is specified]
ps: For multi-dimensional arrays, there is a constraint that sub-arrays must be aligned (because recording the number of
all dimensions of a multi-dimensional array with misaligned sub-arrays is too costly to implement and there is no useful
place to go) */
#define _JSON_EXPAND_NUMBER(obj, cmd, ...) _JSON_CMD_##cmd(obj, ##__VA_ARGS__)
#define _JSON_CMD_NUMBER(obj, num, ...) .num_addr = &num, .num_size = sizeof(num), _JSON_EXPAND_NUMBER(obj, __VA_ARGS__)
/* Primary key, set this flag, if this key is missing during parsing, the parse will return failure directly, return
 * MVVCD_EC_NOT_FOUND */
#define _JSON_EXPAND_CHECK(obj, cmd, ...) _JSON_CMD_##cmd(obj, ##__VA_ARGS__)
#define _JSON_CMD_CHECK(obj, ...) .check = 1, _JSON_EXPAND_CHECK(obj, __VA_ARGS__)
/* Array support, the parameter following it is the dimension of the array */
#define _JSON_EXPAND_ARRAY(obj, cmd, ...) _JSON_CMD_##cmd(obj, ##__VA_ARGS__)
#define _JSON_CMD_ARRAY(obj, level, ...)                                                                               \
    .array_level = level, .array_obj_size = _JSON_ARRAY##level(obj), _JSON_EXPAND_ARRAY(obj, __VA_ARGS__)

/*======================================================*/

/* Basic type enumeration */
enum {
    JSON_BOOL,   /* Boolean value */
    JSON_INT,    /* Signed integer */
    JSON_UINT,   /* Unsigned integer */
    JSON_DOUBLE, /* floating point number */
    JSON_STRING, /* string */
    JSON_OBJECT, /* object */
};

/********************************* For external use ********************************************/

/* Key Object [Subtable] Extension */ /* Key Address Size Sub-list size Type Extension parameter */
#define json_bool(key, obj, ...)                                                                                       \
    {                                                                                                                  \
        key, &obj, sizeof(obj), 0, JSON_BOOL, _JSON_EXPAND(obj, __VA_ARGS__)                                           \
    }
#define json_int(key, obj, ...)                                                                                        \
    {                                                                                                                  \
        key, &obj, sizeof(obj), 0, JSON_INT, _JSON_EXPAND(obj, __VA_ARGS__)                                            \
    }
#define json_uint(key, obj, ...)                                                                                       \
    {                                                                                                                  \
        key, &obj, sizeof(obj), 0, JSON_UINT, _JSON_EXPAND(obj, __VA_ARGS__)                                           \
    }
#define json_double(key, obj, ...)                                                                                     \
    {                                                                                                                  \
        key, &obj, sizeof(obj), 0, JSON_DOUBLE, _JSON_EXPAND(obj, __VA_ARGS__)                                         \
    }
#define json_string(key, obj, ...)                                                                                     \
    {                                                                                                                  \
        key, &obj, sizeof(obj), 0, JSON_STRING, _JSON_EXPAND(obj, __VA_ARGS__)                                         \
    }
#define json_object(key, obj, cld, ...)                                                                                \
    {                                                                                                                  \
        key, &cld, sizeof(obj), sizeof(cld) / sizeof(cld[0]), JSON_OBJECT, _JSON_EXPAND(obj, __VA_ARGS__)              \
    }
/* Delete any specified key json node */
#define json_delete(key)                                                                                               \
    {                                                                                                                  \
        key, NULL, 0                                                                                                   \
    }

/* One-dimensional array, compatible with old api, and the one-dimensional use frequency is high, providing syntactic
 * sugar, please use extended commands (ARRAY, dimension) when multi-dimensional */
#define json_arrbool(key, obj, ...) json_bool(key, obj, ARRAY, 1, __VA_ARGS__)
#define json_arrint(key, obj, ...) json_int(key, obj, ARRAY, 1, __VA_ARGS__)
#define json_arrunt(key, obj, ...) json_uint(key, obj, ARRAY, 1, __VA_ARGS__)
#define json_arrdbl(key, obj, ...) json_double(key, obj, ARRAY, 1, __VA_ARGS__)
#define json_arrstr(key, obj, ...) json_string(key, obj, ARRAY, 1, __VA_ARGS__)
#define json_arrobj(key, obj, cld, ...) json_object(key, obj, cld, ARRAY, 1, __VA_ARGS__)

EXAPI s32 ipc_json_parse(pv8 json_str, ipc_json_t h_json[], s32 num);
EXAPI pv8 ipc_json_renew(pv8 json_str, ipc_json_t h_json[], s32 num);
EXAPI pv8 ipc_json_build(ipc_json_t h_json[], s32 num);
EXAPI void ipc_json_freed(pv8 json_str);
EXAPI s32 ipc_json_wrconf(pv8 session, ipc_json_t h_json[], s32 num);
EXAPI s32 ipc_json_rdconf(pv8 session, ipc_json_t h_json[], s32 num);

#ifdef __cplusplus
}
#endif

#endif //__IPC_JSON_H__
