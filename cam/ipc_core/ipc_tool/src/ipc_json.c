#include "ipc_json.h"
#include "cJSON.h"
#include "ipc_config.h"
#include "ipc_misc.h"
#include "ipc_key_manage.h"

enum {
    _NONE_CHECK, ///< No check required             --> Constant marker
    _MRAK_CHEEK, ///< Check external markers         --> Constant marker
    _NEED_CHECK, ///< Start checking                  --> Running marker
    _TOBJ_CHECK, ///< Check for temporary objects (special operations) --> Running marker
};

/*================================================== BUILD ==============================================*/

static cJSON* _build_unsigned(vptr addr, u32 size)
{
    switch (size) {
        case sizeof(u8):
            return cJSON_CreateNumber((f64) * (pu8)addr);
        case sizeof(u16):
            return cJSON_CreateNumber((f64) * (pu16)addr);
        case sizeof(u32):
            return cJSON_CreateNumber((f64) * (pu32)addr);
        case sizeof(u64):
            return cJSON_CreateNumber((f64) * (pu64)addr);
    }
    return NULL;
}

static cJSON* _build_signed(vptr addr, u32 size)
{
    switch (size) {
        case sizeof(s8):
            return cJSON_CreateNumber((f64) * (ps8)addr);
        case sizeof(s16):
            return cJSON_CreateNumber((f64) * (ps16)addr);
        case sizeof(s32):
            return cJSON_CreateNumber((f64) * (ps32)addr);
        case sizeof(s64):
            return cJSON_CreateNumber((f64) * (ps64)addr);
    }
    return NULL;
}

static cJSON* _build_bool(vptr addr, u32 size)
{
    switch (size) {
        case sizeof(s8):
            return cJSON_CreateBool((s32) * (ps8)addr);
        case sizeof(s16):
            return cJSON_CreateBool((s32) * (ps16)addr);
        case sizeof(s32):
            return cJSON_CreateBool((s32) * (ps32)addr);
        case sizeof(s64):
            return cJSON_CreateBool((s32) * (ps64)addr);
    }
    return NULL;
}

static cJSON* _build_double(vptr addr, u32 size)
{
    switch (size) {
        case sizeof(f32):
            return cJSON_CreateNumber((f64) * (pf32)addr);
        case sizeof(f64):
            return cJSON_CreateNumber((f64) * (pf64)addr);
    }
    return NULL;
}

static s32 _table_build(cJSON** root, ipc_json_t h_jsons[], s32 num, s32 addr_offset);
static cJSON* _single_build(ipc_json_p h_json, s32 addr_offset, u32 size)
{
    vptr addr  = h_json->addr + addr_offset; /* Relative offset */
    cJSON* obj = NULL;

    switch (h_json->type) {
        case JSON_BOOL:
            obj = _build_bool(addr, size);
            break;
        case JSON_INT:
            obj = _build_signed(addr, size);
            break;
        case JSON_UINT:
            obj = _build_unsigned(addr, size);
            break;
        case JSON_DOUBLE:
            obj = _build_double(addr, size);
            break;
        case JSON_STRING:
            obj = cJSON_CreateString(addr);
            break;
        case JSON_OBJECT:
            // coverity[CHECKED_RETURN :SUPPRESS]
            _table_build(&obj, h_json->addr, h_json->child_num, addr_offset);
    }

    return obj;
}

static void _get_number(ps32 num_addr, ipc_json_p h_json, s32 addr_offset)
{
    if (!h_json->num_addr || !h_json->num_size)
        return;

    s32 num = 0;
    switch (h_json->num_size) {
        case sizeof(u8):
            num = *(pu8)(h_json->num_addr + addr_offset);
            break;
        case sizeof(u16):
            num = *(pu16)(h_json->num_addr + addr_offset);
            break;
        case sizeof(u32):
            num = *(pu32)(h_json->num_addr + addr_offset);
            break;
        case sizeof(u64):
            num = *(pu64)(h_json->num_addr + addr_offset);
            break;
        default:
            return; /* For the time being, we will rule out other situations and all that can be passed down will be
                       successful assignments */
    }

    *num_addr = MIN(*num_addr, num); /* Compare with the incoming number and take the minimum value to ensure that the
                                        user will not crash when making errors */
}

static cJSON* _row_build(ipc_json_p h_json, s32 addr_offset, u32 father_size, u8 level)
{
    if (level == h_json->array_level) { /* The last layer */
        return _single_build(h_json, addr_offset, father_size);
    }

    s32 idx        = 0;
    u32 child_size = h_json->array_obj_size[level];
    s32 child_num  = father_size / child_size;
    cJSON* child   = NULL;

    if (level == 0)
        _get_number(&child_num, h_json, addr_offset);

    cJSON* array = cJSON_CreateArray();
    if (array == NULL)
        return NULL;

    for (idx = 0; idx < child_num; idx++) {
        child = _row_build(h_json, addr_offset, child_size, level + 1);
        if (child == NULL) {
            cJSON_Delete(array);
            return NULL;
        }
        cJSON_AddItemToArray(array, child);
        addr_offset += child_size;
    }

    return array;
}

static s32 _obj_build(cJSON* obj, ipc_json_p h_json, s32 addr_offset, u32 father_size, u8 level)
{
    if (level == h_json->array_level) { /* The last layer */
        if (obj->type != cJSON_Object)
            return IPC_NOT_MATCH; /* Type mismatch or dimension mismatch */
        return _table_build(&obj, h_json->addr, h_json->child_num, addr_offset);
    }

    if (obj->type != cJSON_Array)
        return IPC_NOT_MATCH; /* Type mismatch or dimension mismatch */

    u32 child_size = h_json->array_obj_size[level];
    s32 child_num  = father_size / child_size;

    if (level == 0)
        _get_number(&child_num, h_json, addr_offset);

    s32 ret      = 0;
    s32 idx      = 0;
    cJSON* child = NULL;
    s32 old_num  = cJSON_GetArraySize(obj);
    s32 num      = MIN(old_num, child_num);

    for (idx = 0; idx < num; idx++) {
        child = cJSON_GetArrayItem(obj, idx);
        if (child) {
            ret = _obj_build(child, h_json, addr_offset, child_size, level + 1);
            if (ret < 0)
                return ret;
        }
        addr_offset += child_size;
    }

    for (; idx < child_num; idx++) { /* If you pass the above level, at least the dimension should be correct. For the
                                        remaining levels, you can freely expand as needed */
        child = _row_build(h_json, addr_offset, child_size, level + 1);
        if (child == NULL)
            return IPC_NOMEM;

        cJSON_AddItemToArray(obj, child);
        addr_offset += child_size;
    }

    return IPC_SUCCESS;
}

/* pstru_root Pass in the pointer address. If the pointer itself is NULL, create a new one and return it. If it is not
 * NULL, use that object */
static s32 _table_build(cJSON** root, ipc_json_t h_jsons[], s32 num, s32 addr_offset)
{
    s32 ret          = 0;
    s32 idx          = 0;
    ipc_json_p h_json = NULL;
    u8 create_flag   = 0;
    s32 will_build   = 0;

    cJSON* obj = NULL; /* This obj */
    cJSON* old = NULL; /* Old child obj */
    cJSON* new = NULL; /* New child obj */

    if (*root) {
        obj = *root; /* If there is one that can be reused, reuse it. If not, create a new one */
    } else {
        obj = cJSON_CreateObject();
        if (obj == NULL)
            return IPC_NOMEM;
        create_flag = 1;
    }

    for (idx = 0; idx < num; idx++) { /* Loop through each child object for parsing */

        h_json = &h_jsons[idx];

        will_build = 1; /* If the number of any row is explicitly set to 0, no creation or merging will be done */
        _get_number(&will_build, h_json, addr_offset);
        if (!will_build)
            continue;

        old = cJSON_GetObjectItem(obj, h_json->key);
        if (h_json->addr == NULL) { /* addr=null, Handle deletion. If it exists, delete it. If not, ignore it */
            if (old)
                cJSON_DeleteItemFromObject(obj, h_json->key);
            continue;
        }
        if (old && h_json->type == JSON_OBJECT) { /* Conflict at the same level with the same key. For objects, perform
                                                     deep-level comparison and merging */
            ret = _obj_build(old, h_json, addr_offset, h_json->size, 0);
            if (ret == IPC_SUCCESS)
                continue;
            if (ret == IPC_NOMEM) { /* Memory issue, no solution, return error */
                if (create_flag)
                    cJSON_Delete(obj); /* Create and delete it yourself */
                return ret;
            }
            /* In case of other problems, replace the entire object by default */
        }

        new = _row_build(h_json, addr_offset, h_json->size, 0);
        if (new == NULL) {
            if (create_flag)
                cJSON_Delete(obj); /* Create and delete it yourself */
            return IPC_NOMEM;
        }

        if (old) { /* Replace the original one if it exists; add the new one if it doesn't */
            cJSON_ReplaceItemInObject(obj, h_json->key, new);
        } else {
            cJSON_AddItemToObject(obj, h_json->key, new);
        }
    }

    *root = obj;

    return IPC_SUCCESS;
}

pv8 ipc_json_build(ipc_json_t h_jsons[], s32 num)
{
    if (!h_jsons || num <= 0)
        return NULL;

    s32 ret     = 0;
    cJSON* root = NULL;

    ret = _table_build(&root, h_jsons, num, 0);
    if (ret < 0 || root == NULL)
        return NULL;

    pv8 json = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);

    return json;
}

pv8 ipc_json_renew(pv8 old, ipc_json_t h_jsons[], s32 num)
{
    if (!old || !h_jsons || num <= 0)
        return NULL;

    s32 ret   = 0;
    pv8 json  = NULL;
    pv8 start = strstr(old, "{");
    if (start == NULL)
        return NULL;

    cJSON* root = cJSON_Parse(start);
    if (root == NULL)
        return NULL;

    ret = _table_build(&root, h_jsons, num, 0);
    if (ret == IPC_SUCCESS) {
        json = cJSON_PrintUnformatted(root);
    }

    cJSON_Delete(root);

    return json;
}

void ipc_json_freed(pv8 json)
{
    cJSON_free(json);
}

/*================================================== PARSE ==============================================*/

static inline void _put_integer(vptr addr, u32 size, f64 value)
{
    switch (size) { /* f64 has only 52 bits of precision and cannot meet the needs of s64 and u64. At the same time,
                       under O2 optimization, f64 to unsigned conversion may have problems and needs to be converted to
                       signed first */
        case sizeof(u8):
            *(pu8)addr = (u8)(s64)value;
            break;
        case sizeof(u16):
            *(pu16)addr = (u16)(s64)value;
            break;
        case sizeof(u32):
            *(pu32)addr = (u32)(s64)value;
            break;
        case sizeof(u64):
            *(pu64)addr = (u64)(s64)value;
            break;
    }
}

static inline void _put_double(vptr addr, u32 size, f64 value)
{
    switch (size) {
        case sizeof(f32):
            *(pf32)addr = (f32)value;
            break;
        case sizeof(f64):
            *(pf64)addr = (f64)value;
            break;
    }
}

static s32 _table_parse(cJSON* root, ipc_json_t h_jsons[], s32 num, s32 addr_offset);
static s32 _table_check(cJSON* root, ipc_json_t h_jsons[], s32 num, s32 addr_offset);

/* Parse individual data */
static s32 _single_parse(cJSON* obj, ipc_json_p h_json, s32 addr_offset, u32 size)
{
    vptr addr = h_json->addr + addr_offset; /* Relative offset */
    s32 ret   = IPC_NOT_FOUND;               /* Default not found */
    u8 check_mode
        = h_json->check == _NEED_CHECK || h_json->check == _TOBJ_CHECK ? 1 : 0; /* Do not assign values in check mode */

    switch (h_json->type) {
        case JSON_BOOL:
            if (obj->type == cJSON_False || obj->type == cJSON_True) {
                if (!check_mode)
                    _put_integer(addr, size, obj->valueint);
                ret = IPC_SUCCESS;
            }
            break;
        case JSON_INT:
        case JSON_UINT:
            if (obj->type == cJSON_Number) {
                if (!check_mode)
                    _put_integer(addr, size, obj->valuedouble);
                ret = IPC_SUCCESS;
            }
            break;
        case JSON_DOUBLE:
            if (obj->type == cJSON_Number) {
                if (!check_mode)
                    _put_double(addr, size, obj->valuedouble);
                ret = IPC_SUCCESS;
            }
            break;
        case JSON_STRING:
            if (obj->type == cJSON_String && obj->valuestring) {
                if (!check_mode)
                    snprintf(addr, size, "%s",
                             obj->valuestring); /* snprintf is more efficient and robust than strncpy */
                ret = IPC_SUCCESS;
            }
            break;
        case JSON_OBJECT:
            if (obj->type == cJSON_Object) {
                if (!check_mode) {
                    ret = _table_parse(obj, h_json->addr, h_json->child_num, addr_offset);
                } else {
                    ret = _table_check(obj, h_json->addr, h_json->child_num, addr_offset);
                    if (ret < 0)
                        ret = IPC_VERIFY_FAILED; /* Special conversion to distinguish it from other ordinary return
                                                   values */
                }
            }
            break;
    }

    return ret;
}

static inline void _put_number(ipc_json_p h_json, s32 num, s32 addr_offset)
{
    if (h_json->num_addr && h_json->num_size && h_json->check != _NEED_CHECK
        && h_json->check != _TOBJ_CHECK) { /* Do not assign values in check mode */
        _put_integer(h_json->num_addr + addr_offset, h_json->num_size, num);
    }
}

/* Parse each row of the table, and also hierarchical parsing of multi-dimensional data */
static s32 _row_parse(cJSON* obj, ipc_json_p h_json, s32 addr_offset, u32 father_size, u8 level)
{
    s32 ret = 0;

    if (level == 0)
        _put_number(h_json, 0, addr_offset); // The outermost dimension is cleared first

    if (level == h_json->array_level) { // Reached the last dimension
        ret = _single_parse(obj, h_json, addr_offset, father_size);
        if (ret == IPC_SUCCESS && level == 0) { // The outermost dimension is parsed successfully, and the number is 1
            _put_number(h_json, 1, addr_offset);
        }

        return ret;
    }

    if (obj->type != cJSON_Array)
        return IPC_NOT_FOUND; // Not an array, not qualified to continue

    s32 idx        = 0;
    u32 child_size = h_json->array_obj_size[level];
    s32 child_num  = father_size / child_size;
    cJSON* child   = NULL;

    s32 parse_num = 0; // Number parsed
    s32 tmp_addr  = addr_offset;

    for (idx = 0; idx < child_num; idx++) { // Maximum fill
        child = cJSON_GetArrayItem(obj, idx);
        if (!child)
            break; // Exceeded the number, exit the loop

        ret = _row_parse(child, h_json, tmp_addr, child_size, level + 1);
        if (ret == IPC_SUCCESS)
            parse_num = idx + 1; // Record the highest idx (allowing holes) that was successfully parsed in the array
        else if (ret == IPC_VERIFY_FAILED && h_json->check == _TOBJ_CHECK)
            return ret; // Need obj check but child table check verification failed
        else if (h_json->check == _NEED_CHECK)
            return ret;
        tmp_addr += child_size;
    }

    if (level == 0)
        _put_number(h_json, parse_num, addr_offset); // Return the number of the outermost dimension

    return parse_num ? IPC_SUCCESS
                     : IPC_NOT_FOUND; // Array definition: As long as one can be parsed successfully, it is considered a
                                     // success, unless none of them can be parsed successfully
}

// Parse a single user-entered table, parse what can be parsed, and skip what cannot be parsed, always returning success
static s32 _table_parse(cJSON* root, ipc_json_t h_jsons[], s32 num, s32 addr_offset)
{
    s32 idx          = 0;
    ipc_json_p h_json = NULL;
    cJSON* obj       = NULL;

    for (idx = 0; idx < num; idx++) { // Loop through each child object for parsing
        h_json = &h_jsons[idx];

        if (h_json->addr == NULL)
            continue; // Skip deleted instructions

        obj = cJSON_GetObjectItem(root, h_json->key);
        if (obj == NULL)
            continue; // Ignore this one, parse the next one
        
        // coverity[CHECKED_RETURN :SUPPRESS]
        _row_parse(obj, h_json, addr_offset, h_json->size, 0); // Parse the row
    }

    return IPC_SUCCESS;
}

// Pre-check mode, go through all the parameters that need to be checked first, without making any assignments. Upon
// encountering an parse mismatch, immediately return an error code
static s32 _table_check(cJSON* root, ipc_json_t h_jsons[], s32 num, s32 addr_offset)
{
    s32 idx          = 0;
    s32 ret          = 0;
    ipc_json_p h_json = NULL;
    cJSON* obj       = NULL;

    for (idx = 0; idx < num; idx++) { // Loop through each child object for parsing
        h_json = &h_jsons[idx];

        if (h_json->addr == NULL)
            continue; // Skip deleted instructions

        if (h_json->check == _MRAK_CHEEK) { // A parameter that requires checking
            obj = cJSON_GetObjectItem(root, h_json->key);
            if (obj == NULL)
                return IPC_NOT_FOUND;

            h_json->check = _NEED_CHECK;                                           // Marked as needing checks
            ret           = _row_parse(obj, h_json, addr_offset, h_json->size, 0); // Parse the row
            h_json->check = _MRAK_CHEEK; // Return the flag status to prevent affecting the parse operation

            if (ret < 0)
                return ret;
        } else if (h_json->type == JSON_OBJECT) { // Special handling of objects, if the json field exists and the type
                                                  // (obj) matches the dimension, continue checking its child table
            obj = cJSON_GetObjectItem(root, h_json->key);
            if (obj == NULL)
                continue;

            h_json->check = _TOBJ_CHECK;                                           // Marked as needing checks
            ret           = _row_parse(obj, h_json, addr_offset, h_json->size, 0); // Parse the row
            h_json->check = _NONE_CHECK; // Clearing the flag to prevent affecting the parse operation

            if (ret == IPC_VERIFY_FAILED)
                return ret;
        }
    }

    return IPC_SUCCESS;
}

s32 ipc_json_parse(pv8 json_str, ipc_json_t h_jsons[], s32 num)
{
    if (!json_str || !h_jsons || num <= 0)
        return IPC_INVALID_ARGS;

    s32 ret   = 0;
    pv8 start = strstr(json_str, "{");
    if (start == NULL)
        return IPC_FAILED;

    cJSON* root = cJSON_Parse(start);
    if (root == NULL)
        return IPC_FAILED;

    ret = _table_check(root, h_jsons, num, 0); // Pre-check
    if (ret == IPC_SUCCESS) {                   // If the check passes, Parse for real
        _table_parse(root, h_jsons, num, 0);
    }

    cJSON_Delete(root);

    return ret;
}

/******************************************** CONFIG *********************************************/
#include "ipc_dfs.h"

#ifndef IPC_CONF_SAVE_PATH
#define IPC_CONF_SAVE_PATH "/conf"
#endif

#ifndef IPC_CONF_MAX_SIZE
#define IPC_CONF_MAX_SIZE (2 * 1042)
#endif

// Reads configuration from JSON file using session ID
s32 ipc_json_rdconf(pv8 session, ipc_json_t h_jsons[], s32 num)
{
    // Check if inputs are valid (session name, config objects, count)
    if (!session || !session[0] || !h_jsons || num <= 0)
        return IPC_INVALID_ARGS;

    // Create buffers for config data and file path
    v8 buff[IPC_CONF_MAX_SIZE] = { 0 };
    v8 path[128]              = { 0 };

    // Build file path: if starts with '/', use directly, else add standard path
    if (session[0] == '/') {
        snprintf(path, sizeof(path), "%s.json", session);
    } else {
        snprintf(path, sizeof(path), "%s/%s.json", IPC_CONF_SAVE_PATH, session);
    }

    // Read encrypted config file contents into buffer
    s32 ret = ipc_file_read_once(path, buff, sizeof(buff), NULL);
    if (ret < 0)
        return ret;

    // Decrypt the config data using security keys
    key_manage_decrypt_with_conf_key_1(path, buff, ret);

    // Convert JSON text into structured data objects
    return ipc_json_parse(buff, h_jsons, num);
}

// Updates existing JSON config with new values
static pv8 _json_update(pv8 old, ipc_json_t h_jsons[], s32 num)
{
    // Initialize variables for JSON processing
    s32 ret     = 0;
    pv8 json    = NULL;
    cJSON* root = NULL;
    pv8 start   = NULL;

    // Find the actual JSON content start in old data
    start = strstr(old, "{");
    if (start)
        root = cJSON_Parse(start); // Convert text to JSON structure

    // Update values in the JSON structure
    ret = _table_build(&root, h_jsons, num, 0);
    if (ret == IPC_SUCCESS) {
        json = cJSON_Print(root); // Convert updated JSON back to text
    }

    // Clean up temporary JSON structure
    if (root)
        cJSON_Delete(root);

    return json; // Return updated JSON text
}

// Writes updated configuration back to JSON file
s32 ipc_json_wrconf(pv8 session, ipc_json_t h_jsons[], s32 num)
{
    // Validate input parameters
    if (!session || !session[0] || !h_jsons || num <= 0)
        return IPC_INVALID_ARGS;

    // Prepare buffers for config data and file path
    v8 buff[IPC_CONF_MAX_SIZE];
    v8 path[128];

    // Create full file path same as reading function
    if (session[0] == '/') {
        snprintf(path, sizeof(path), "%s.json", session);
    } else {
        snprintf(path, sizeof(path), "%s/%s.json", IPC_CONF_SAVE_PATH, session);
    }

    // Open config file for reading and writing
    ipc_file_t h_file;
    s32 ret = ipc_file_open(h_file, path, IPC_FILE_RDWR, NULL);
    if (ret < 0)
        return ret;

    // Read existing encrypted config content
    s32 len = ipc_file_read(h_file, buff, sizeof(buff) - 1);
    if (len < 0) {
        ipc_file_close(h_file);
        return len;
    }
    buff[len] = '\0'; // Mark end of text

    // Decrypt existing configuration
    key_manage_decrypt_with_conf_key_1(path, buff, len);

    // Get updated JSON text with new changes
    pv8 json = _json_update(buff, h_jsons, num);
    if (json == NULL) {
        ipc_file_close(h_file);
        return IPC_NOMEM;
    }

    // Check if changes need to be saved (length or content changed)
    s32 new_len = strlen(json);
    if (new_len != len || strcmp(json, buff)) {
        ipc_file_clear(h_file); // Clear old file contents

        // Encrypt updated config before saving
        key_manage_encrypt_with_conf_key_1(path, json, new_len);

        ret = ipc_file_write(h_file, json, new_len); // Write new data
    } else {
        cpdebug("Not need update json configure"); // No changes needed
    }

    // Final cleanup: close file and free memory
    ipc_file_close(h_file);
    ipc_json_freed(json);

    return ret < 0 ? ret : IPC_SUCCESS;
}
