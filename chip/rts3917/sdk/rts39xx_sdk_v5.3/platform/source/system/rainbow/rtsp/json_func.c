#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <json-c/json.h>

static int update_rtspServerPortNum_from_json(void)
{
	int ret, portnum = 0;
	struct json_object *root_obj = NULL;
	struct json_object *portnum_obj = NULL;
	char *path = (char *)"/etc/conf/rainbow.json";

	root_obj = json_object_from_file(path);
	if (!root_obj)
		return -1;

	ret = json_object_object_get_ex(
			root_obj, "rtspServerPortNum", &portnum_obj);
	if (ret) {
		portnum = json_object_get_int(portnum_obj);
		ret = portnum;
	}

	if (root_obj)
		json_object_put(root_obj);

	return ret;
}

