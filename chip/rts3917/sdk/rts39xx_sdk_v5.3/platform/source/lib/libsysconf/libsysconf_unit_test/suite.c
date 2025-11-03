/*
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <CUnit/Basic.h>

#include "cunit_suit.h"
#include "../sysconf.c"

const char *tokener_cat = "{ \
	   \"name\":\"mewl\", \
	   \"pet\":\"cat\", \
	   \"addr\":\"Suzhou\", \
	   \"color\":\"white\", \
	   \"age\":2, \
	   \"favor\":[\"shrimp\", \"fish\", \"chicken\"], \
	   \"height\":[5, 8, 11, 15, 22, 30, 50],\
	   \"weight\":[0.5, 1.1, 2.2, 3.3],\
	   \"inoculate\": [ \
		{\"date\":\"2013-07-20\", \"name\":\"panleukopenia\", \"cost\":120}, \
		{\"date\":\"2013-07-27\", \"name\":\"rhinotracheitis\", \"cost\":150}, \
		{\"date\":\"2013-08-05\", \"name\":\"rabies vaccine\", \"cost\":80}, \
	   ], \
	   \"master\":{ \
		\"name\":\"tony\", \
		\"career\":\"programer\", \
		\"height\":173, \
		\"vision\":1.5, \
	   }, \
	}";


static int __init()
{
	struct json_object *o;

	o = json_tokener_parse(tokener_cat);
	json_object_to_file_ext("/var/conf/test.json",
			o, JSON_C_TO_STRING_PRETTY);


	return 0;
}

static int __cleanup()
{

	return 0;
}

#define KEY_TEST1	"test1"
#define KEY_TEST2	"test2"

static int __test_create_ctrl_elements(struct ctrl_element **ctrls,
		const char *fmt, ...)
{
	int num = 0;
	va_list vl;

	va_start(vl, fmt);

	num = create_ctrl_elements(fmt, ctrls, vl);
	va_end(vl);

	return num;
}

static void test_rts_conf_scanf()
{
	char vc[128] = {0};
	int age = 0;
	int size = 0;
	int ret = 0;
	int cost = 0;
	int height = 0;

	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%d", "age", &age);
	CU_ASSERT(0 == ret);
	CU_ASSERT(2 == age);

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%l%s", "name", sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "mewl"));

	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%z", "favor", &size);
	CU_ASSERT(0 == ret);
	CU_ASSERT(3 == size);

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%i%l%s", "favor", 0,
			sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "shrimp"));

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%k%l%s", "master", "name",
			sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "tony"));

	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%k%d", "master", "height",
			&height);
	CU_ASSERT(0 == ret);
	CU_ASSERT(173 == height);

	double vision = 0.0;
	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%k%f", "master", "vision",
			&vision);
	CU_ASSERT(0 == ret);
	CU_ASSERT(fabs(vision - 1.5) < 0.001);

	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%i%k%d",
			"inoculate", 1, "cost", &cost);
	CU_ASSERT(0 == ret);
	CU_ASSERT(150 == cost);

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%i%k%l%s",
			"inoculate", 1, "name", sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "rhinotracheitis"));

	do {
		int buf[32] = {0};
		ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%l%a",
				"height", sizeof(buf), buf);
		CU_ASSERT(ret > 0);
		CU_ASSERT(buf[0] == 5)
		CU_ASSERT(buf[1] == 8)
		CU_ASSERT(buf[2] == 11)
		CU_ASSERT(buf[3] == 15)
		CU_ASSERT(buf[4] == 22)
		CU_ASSERT(buf[5] == 30)
		CU_ASSERT(buf[6] == 50)

	} while (0);

	do {
		double buf[32] = {0};
		ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%l%p",
				"weight", sizeof(buf), buf);
		CU_ASSERT(ret > 0);
		CU_ASSERT(fabs(buf[0] - 0.5) < 0.01);
		CU_ASSERT(fabs(buf[1] - 1.1) < 0.01);
		CU_ASSERT(fabs(buf[2] - 2.2) < 0.01);
		CU_ASSERT(fabs(buf[3] - 3.3) < 0.01);

	} while (0);
}

static void test_rts_conf_scanf_array()
{
	char filepath[128] = {0};
	struct json_object *root, *obj_height;
	struct array_list *al;
	int buf[32] = {0};
	int get = 0;
	int ret = 0;

	get_filepath(CFG_DOMAIN_TEST, filepath, sizeof(filepath));

	root = json_object_from_file(filepath);
	CU_ASSERT(root != NULL);
	get = json_object_object_get_ex(root, "height", &obj_height);
	CU_ASSERT(get);

	al = json_object_get_array(obj_height);
	CU_ASSERT(al != NULL);

	ret = rts_conf_get_array_form_json(root, (char *) buf,
			sizeof(buf), json_type_int);
	CU_ASSERT(ret == -E_INVALID_OBJ_TYPE);
	ret = rts_conf_get_array_form_json(obj_height,
			(char *) buf, 8, json_type_int);
	CU_ASSERT(ret == 8);
	ret = rts_conf_get_array_form_json(obj_height,
			(char *) buf, sizeof(buf), json_type_int);
	CU_ASSERT((ret / sizeof(*buf)) == al->length);

	do {
		int i = 0;
		for(i = 0; i < al->length; i++) {
			struct json_object *obj = al->array[i];
			int value = json_object_get_int(obj);
			CU_ASSERT(buf[i] == value);
		}
	} while (0);
}

static void test_get_param_from_json()
{
	struct json_object *o;
	struct ctrl_element *ctrls = NULL;
	char vc[128] = {0};
	int  vi = 0;
	int num = 0;
	int size = 0;
	int ret = 0;

	o = json_tokener_parse(tokener_cat);

	memset(vc, 0, sizeof(vc));
	num = __test_create_ctrl_elements(&ctrls, "%k%l%s",
			"name",
			sizeof(vc),
			vc);
	CU_ASSERT(3 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "mewl"));
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%d",
			"age",
			&vi);
	CU_ASSERT(2 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(2 == vi);
	free(ctrls);

	size = 0;
	num = __test_create_ctrl_elements(&ctrls, "%k%z",
			"favor",
			&size);
	CU_ASSERT(2 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(3 == size);
	free(ctrls);

	memset(vc, 0, sizeof(vc));
	num = __test_create_ctrl_elements(&ctrls, "%k%i%l%s",
			"favor",
			1,
			sizeof(vc),
			vc);
	CU_ASSERT(4 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "fish"));
	free(ctrls);

	memset(vc, 0, sizeof(vc));
	num = __test_create_ctrl_elements(&ctrls, "%k%k%l%s",
			"master",
			"career",
			sizeof(vc),
			vc);
	CU_ASSERT(4 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "programer"));
	free(ctrls);

	vi = 0;
	num = __test_create_ctrl_elements(&ctrls, "%k%k%d",
			"master", "height", &vi);
	CU_ASSERT(3 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(173 == vi);
	free(ctrls);

	vi = 0;
	num = __test_create_ctrl_elements(&ctrls, "%k%i%k%d",
			"inoculate", 2, "cost", &vi);
	CU_ASSERT(4 == num);
	ret = rts_conf_get_value_from_json(ctrls, num, o);
	CU_ASSERT(0 == ret);
	CU_ASSERT(80 == vi);
	free(ctrls);
}

static void test_verify_scanf_elements()
{
	struct ctrl_element *ctrls = NULL;
	int num = 0;
	int ret = 0;

	num = __test_create_ctrl_elements(&ctrls, "%k%k%d",
			KEY_TEST1,
			KEY_TEST2,
			NULL);
	CU_ASSERT(3 == num);
	ret = verify_scanf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%i",
			KEY_TEST1,
			KEY_TEST2,
			NULL);
	CU_ASSERT(3 == num);
	ret = verify_scanf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%s",
			KEY_TEST1,
			KEY_TEST2,
			NULL);
	CU_ASSERT(3 == num);
	ret = verify_scanf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%a",
			KEY_TEST1,
			KEY_TEST2,
			NULL);
	CU_ASSERT(3 == num);
	ret = verify_scanf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%l%a",
			KEY_TEST1,
			KEY_TEST2,
			20,
			NULL);
	CU_ASSERT(4 == num);
	ret = verify_scanf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

}

static void test_create_ctrl_elements()
{
	struct ctrl_element *ctrls = NULL;
	int num = 0;
	int size = 16;

	num = __test_create_ctrl_elements(&ctrls, "%k%k%d",
			KEY_TEST1,
			KEY_TEST2,
			&size);
	CU_ASSERT(3 == num);
	CU_ASSERT(ctrls != NULL);
	CU_ASSERT('k' == ctrls[0].ctrl);
	CU_ASSERT('k' == ctrls[1].ctrl);
	CU_ASSERT('d' == ctrls[2].ctrl);
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[0].data.p));
	CU_ASSERT((const char *) KEY_TEST2 == (const char *) (ctrls[1].data.p));
	CU_ASSERT(&size == (int *) (ctrls[2].data.p));
	CU_ASSERT(16 == *(int *)(ctrls[2].data.p));
	free(ctrls);
	ctrls = NULL;

	int index = 100;
	num = __test_create_ctrl_elements(&ctrls, "%k%k%k%i%",
			KEY_TEST1,
			KEY_TEST1,
			KEY_TEST2,
			index);
	CU_ASSERT(4 == num);
	CU_ASSERT(ctrls != NULL);
	CU_ASSERT('k' == ctrls[0].ctrl);
	CU_ASSERT('k' == ctrls[1].ctrl);
	CU_ASSERT('k' == ctrls[2].ctrl);
	CU_ASSERT('i' == ctrls[3].ctrl);
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[0].data.p));
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[1].data.p));
	CU_ASSERT((const char *) KEY_TEST2 == (const char *) (ctrls[2].data.p));
	CU_ASSERT(100 == ctrls[3].data.i);
	free(ctrls);

	char value[128];
	num = __test_create_ctrl_elements(&ctrls, "%k%k%k%l%s",
			KEY_TEST1,
			KEY_TEST1,
			KEY_TEST2,
			sizeof(value),
			value);
	CU_ASSERT(5 == num);
	CU_ASSERT(ctrls != NULL);
	CU_ASSERT('k' == ctrls[0].ctrl);
	CU_ASSERT('k' == ctrls[1].ctrl);
	CU_ASSERT('k' == ctrls[2].ctrl);
	CU_ASSERT('l' == ctrls[3].ctrl);
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[0].data.p));
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[1].data.p));
	CU_ASSERT((const char *) KEY_TEST2 == (const char *) (ctrls[2].data.p));
	CU_ASSERT(sizeof(value) == ctrls[3].data.i);
	CU_ASSERT(&value[0] == (ctrls[4].data.p));
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%k%r",
			KEY_TEST1,
			KEY_TEST1,
			KEY_TEST2);
	CU_ASSERT(4 == num);
	CU_ASSERT(ctrls != NULL);
	CU_ASSERT('k' == ctrls[0].ctrl);
	CU_ASSERT('k' == ctrls[1].ctrl);
	CU_ASSERT('k' == ctrls[2].ctrl);
	CU_ASSERT('r' == ctrls[3].ctrl);
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[0].data.p));
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[1].data.p));
	CU_ASSERT((const char *) KEY_TEST2 == (const char *) (ctrls[2].data.p));
	CU_ASSERT(NULL == (ctrls[3].data.p));
	free(ctrls);

	int value2[128];
	num = __test_create_ctrl_elements(&ctrls, "%k%k%k%l%d",
			KEY_TEST1,
			KEY_TEST1,
			KEY_TEST2,
			sizeof(value2),
			value2);
	CU_ASSERT(5 == num);
	CU_ASSERT(ctrls != NULL);
	CU_ASSERT('k' == ctrls[0].ctrl);
	CU_ASSERT('k' == ctrls[1].ctrl);
	CU_ASSERT('k' == ctrls[2].ctrl);
	CU_ASSERT('l' == ctrls[3].ctrl);
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[0].data.p));
	CU_ASSERT((const char *) KEY_TEST1 == (const char *) (ctrls[1].data.p));
	CU_ASSERT((const char *) KEY_TEST2 == (const char *) (ctrls[2].data.p));
	CU_ASSERT(sizeof(value2) == ctrls[3].data.i);
	CU_ASSERT(&value2[0] == (ctrls[4].data.p));
	free(ctrls);
}

static void test_get_ctrls_num()
{
	CU_ASSERT(0 == get_ctrl_element_num("%%"));
	CU_ASSERT(0 == get_ctrl_element_num("%%k"));
	CU_ASSERT(0 == get_ctrl_element_num("i%%%%%"));
	CU_ASSERT(1 == get_ctrl_element_num("%%%k"));
	CU_ASSERT(2 == get_ctrl_element_num("%k%d"));
	CU_ASSERT(2 == get_ctrl_element_num("%k%k%"));
	CU_ASSERT(2 == get_ctrl_element_num("----%k%k---"));
	CU_ASSERT(3 == get_ctrl_element_num("%k%k%d"));
	CU_ASSERT(4 == get_ctrl_element_num("%k%k%k%i%"));
	CU_ASSERT(5 == get_ctrl_element_num("%k%k%i%k%d"));
	CU_ASSERT(4 == get_ctrl_element_num("%k%k%l%a"));
}

static void test_rts_conf_printf()
{
	int ret = 0;
	char filepath[128] = {0};
	get_filepath(CFG_DOMAIN_TEST, filepath, sizeof(filepath));

	do {
		struct json_object *root;
		json_object *o;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%s",
				"name", "Mewl");
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "name", &o);
		CU_ASSERT(get);
		CU_ASSERT(0 == strcmp("Mewl", json_object_get_string(o)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%d",
				"age", 5);
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "age", &o);
		CU_ASSERT(get);
		CU_ASSERT(5 == json_object_get_int(o));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST,  "%k%i%s",
				"favor", 2, "CHICKENC");
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "favor", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 2);
		CU_ASSERT(0 == strcmp("CHICKENC", json_object_get_string(e)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST,  "%k%i%k%d",
				"inoculate", 2, "cost", 119);
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 2);
		get = json_object_object_get_ex(e, "cost", &ee);
		CU_ASSERT(get);
		CU_ASSERT(119 == json_object_get_int(ee));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST,  "%k%i%k%s",
				"inoculate", 1, "name", "fake_rhinotracheitis");
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 1);
		get = json_object_object_get_ex(e, "name", &ee);
		CU_ASSERT(get);
		CU_ASSERT(0 == strcmp("fake_rhinotracheitis",
					json_object_get_string(ee)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *oo;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%d",
				"master", "height", 189);
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "height", &oo);
		CU_ASSERT(189 == json_object_get_int(oo));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *oo;
		int get = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%f",
				"master", "height", 189.5);
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "height", &oo);
		CU_ASSERT((fabs(189.5 - json_object_get_double(oo)) < 0.001));

		json_object_put(root);
	} while (0);


	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;

		/* add new object to array */
		ret = rts_conf_printf(CFG_DOMAIN_TEST,  "%k%i%k%s",
				"inoculate", 3, "name", "fake_inoculate");
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 3);
		CU_ASSERT(e != NULL);
		if (e) {
			get = json_object_object_get_ex(e, "name", &ee);
			CU_ASSERT(get);
			CU_ASSERT(0 == strcmp("fake_inoculate",
						json_object_get_string(ee)));

		}
		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		int get = 0;

		/* add new object to object */
		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%d",
				"master", "weight", 63);
		CU_ASSERT(0 == ret);

		root = json_object_from_file(filepath);
		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "weight", &e);
		CU_ASSERT(get);
		if (get)
			CU_ASSERT(63 == json_object_get_int(e));
		json_object_put(root);
	} while (0);

	do {
		char vc[128] = {0};
		int vi = 0;

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%k%s",
				"parent", "father", "type", "Korat");
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%k%s",
				"parent", "father", "color", "White");
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%k%d",
				"parent", "father", "age", 10);
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%k%s",
				"parent", "mother", "color", "Black");
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf(CFG_DOMAIN_TEST, "%k%k%k%d",
				"parent", "mother", "weight", 3);
		CU_ASSERT(0 == ret);

		ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%k%k%l%s",
			"parent", "father", "color", sizeof(vc), vc);
		CU_ASSERT(0 == ret);
		CU_ASSERT(0 == strcmp("White", vc));

		ret = rts_conf_scanf(CFG_DOMAIN_TEST, "%k%k%k%d",
			"parent", "mother", "weight", &vi);
		CU_ASSERT(0 == ret);
		CU_ASSERT(3 == vi);

	} while (0);
}

static void test_put_param_to_json()
{
	struct ctrl_element *ctrls = NULL;
	int num = 0;
	int ret = 0;


	do {
		struct json_object *root;
		json_object *o;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		num = __test_create_ctrl_elements(&ctrls, "%k%s",
				"name", "Mewl");
		CU_ASSERT(num == 2);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "name", &o);
		CU_ASSERT(get);
		CU_ASSERT(0 == strcmp("Mewl", json_object_get_string(o)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		num = __test_create_ctrl_elements(&ctrls, "%k%d",
				"age", 5);
		CU_ASSERT(num == 2);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "age", &o);
		CU_ASSERT(get);
		CU_ASSERT(5 == json_object_get_int(o));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		num = __test_create_ctrl_elements(&ctrls, "%k%i%s",
				"favor", 2, "CHICKENC");
		CU_ASSERT(num == 3);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "favor", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 2);
		CU_ASSERT(0 == strcmp("CHICKENC", json_object_get_string(e)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		num = __test_create_ctrl_elements(&ctrls, "%k%i%k%d",
				"inoculate", 2, "cost", 119);
		CU_ASSERT(num == 4);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 2);
		get = json_object_object_get_ex(e, "cost", &ee);
		CU_ASSERT(get);
		CU_ASSERT(119 == json_object_get_int(ee));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		num = __test_create_ctrl_elements(&ctrls, "%k%i%k%s",
				"inoculate", 1, "name", "fake_rhinotracheitis");
		CU_ASSERT(num == 4);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 1);
		get = json_object_object_get_ex(e, "name", &ee);
		CU_ASSERT(get);
		CU_ASSERT(0 == strcmp("fake_rhinotracheitis",
					json_object_get_string(ee)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *oo;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		num = __test_create_ctrl_elements(&ctrls, "%k%k%d",
				"master", "height", 189);
		CU_ASSERT(num == 3);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "height", &oo);
		CU_ASSERT(189 == json_object_get_int(oo));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		/* add new object to array */
		num = __test_create_ctrl_elements(&ctrls, "%k%i%k%s",
				"inoculate", 3, "name", "fake_inoculate");
		CU_ASSERT(num == 4);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 3);
		CU_ASSERT(e != NULL);
		if (e) {
			get = json_object_object_get_ex(e, "name", &ee);
			CU_ASSERT(get);
			CU_ASSERT(0 == strcmp("fake_inoculate",
						json_object_get_string(ee)));

		}
		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		/* add new object to object */
		num = __test_create_ctrl_elements(&ctrls, "%k%k%d",
				"master", "weight", 62);
		CU_ASSERT(num == 3);
		ret = rts_conf_put_value_to_json(ctrls, num, root);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "weight", &e);
		CU_ASSERT(get);
		if (get)
			CU_ASSERT(62 == json_object_get_int(e));
		json_object_put(root);
	} while (0);


}

static void test_verify_printf_elements()
{
	struct ctrl_element *ctrls = NULL;
	int num = 0;
	int ret = 0;

	num = __test_create_ctrl_elements(&ctrls, "%k%k%d",
			KEY_TEST1,
			KEY_TEST2,
			2);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%s",
			KEY_TEST1,
			KEY_TEST2,
			NULL);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%i%s",
			KEY_TEST1,
			KEY_TEST2,
			2,
			NULL);
	CU_ASSERT(4 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%i%d",
			KEY_TEST1,
			KEY_TEST2,
			2,
			NULL);
	CU_ASSERT(4 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%i%r",
			KEY_TEST1,
			KEY_TEST2,
			2);
	CU_ASSERT(4 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%r",
			KEY_TEST1,
			KEY_TEST2);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(0 == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%i",
			KEY_TEST1,
			KEY_TEST2,
			NULL);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%i%s", NULL);
	CU_ASSERT(2 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%i%i%s", NULL);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%i%r", NULL);
	CU_ASSERT(2 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%s", NULL);
	CU_ASSERT(1 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%d", NULL);
	CU_ASSERT(1 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%d%k", NULL);
	CU_ASSERT(2 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%i%s%k", NULL);
	CU_ASSERT(4 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%r%r%d", NULL);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%k%l%s",
			KEY_TEST1,
			KEY_TEST2,
			2,
			NULL);
	CU_ASSERT(4 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%r", NULL);
	CU_ASSERT(1 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%r%k", NULL);
	CU_ASSERT(2 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%d%r", NULL);
	CU_ASSERT(2 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

	num = __test_create_ctrl_elements(&ctrls, "%k%r%k", NULL);
	CU_ASSERT(3 == num);
	ret = verify_printf_ctrl_elements(ctrls, num);
	CU_ASSERT(-E_FS_INVALID == ret);
	free(ctrls);

}

static void test_rts_conf_reset()
{

}

static void test_rts_conf_get_metadata()
{
	void *metadata = NULL;
	struct json_object *root = NULL;

	metadata = rts_conf_get_metadata(CFG_DOMAIN_TEST, SHARED_ACCESS);
	CU_ASSERT(NULL != metadata);
	CU_ASSERT(0 == ((struct metadata_object *)metadata)->dirty);

	root = json_object_from_file("/var/conf/test.json");
	CU_ASSERT(NULL != root);
	void *root2 = ((struct metadata_object *)metadata)->root;

	CU_ASSERT(strcmp(json_object_to_json_string(root2),
		json_object_to_json_string(root)) == 0);
	json_object_put(root);
	free(metadata);
}

static void test_rts_conf_put_metadata()
{
	struct json_object *root1 = NULL;
	struct json_object *root2 = NULL;
	const char *str1 = NULL;
	const char *str2 = NULL;
	int ret = 0;
	struct metadata_object *metadata = NULL;

	metadata = (struct metadata_object *)malloc(
			sizeof(struct metadata_object));
	root1 = json_tokener_parse(tokener_cat);
	CU_ASSERT(NULL != root1);

	str1 = json_object_to_json_string_ext(root1, JSON_C_TO_STRING_PRETTY);
	metadata->root = (void *)root1;
	metadata->dirty = 1;
	ret = rts_conf_put_metadata(CFG_DOMAIN_TEST, metadata);
	CU_ASSERT(0 == ret);

	root2 = json_object_from_file("/var/conf/test.json");
	CU_ASSERT(NULL != root2);
	str2 = json_object_to_json_string_ext(root2, JSON_C_TO_STRING_PRETTY);
	CU_ASSERT(0 == strcmp(str1, str2));

	json_object_put(root1);
	json_object_put(root2);
	free(metadata);
}

static void test_rts_conf_scanf_ex()
{

	struct json_object *root = NULL;
	root = json_object_from_file("/var/conf/test.json");
	CU_ASSERT(NULL != root);

	char vc[128] = {0};
	int age = 0;
	int size = 0;
	int ret = 0;
	int cost = 0;
	int height = 0;
	struct metadata_object *metadata = NULL;

	metadata = (struct metadata_object *)malloc(
			sizeof(struct metadata_object));
	metadata->root = (void *)root;
	ret = rts_conf_scanf_ex(metadata, "%k%d", "age", &age);
	CU_ASSERT(0 == ret);
	CU_ASSERT(2 == age);

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf_ex(metadata, "%k%l%s", "name", sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "mewl"));

	ret = rts_conf_scanf_ex(metadata, "%k%z", "favor", &size);
	CU_ASSERT(0 == ret);
	CU_ASSERT(3 == size);

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf_ex(metadata, "%k%i%l%s", "favor", 0,
			sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "shrimp"));

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf_ex(metadata, "%k%k%l%s", "master", "name",
			sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "tony"));

	ret = rts_conf_scanf_ex(metadata, "%k%k%d", "master", "height",
			&height);
	CU_ASSERT(0 == ret);
	CU_ASSERT(173 == height);

	double vision = 0.0;
	ret = rts_conf_scanf_ex(metadata, "%k%k%f", "master", "vision",
			&vision);
	CU_ASSERT(0 == ret);
	CU_ASSERT(fabs(vision - 1.5) < 0.001);

	ret = rts_conf_scanf_ex(metadata, "%k%i%k%d",
			"inoculate", 1, "cost", &cost);
	CU_ASSERT(0 == ret);
	CU_ASSERT(150 == cost);

	memset(vc, 0, sizeof(vc));
	ret = rts_conf_scanf_ex(metadata, "%k%i%k%l%s",
			"inoculate", 1, "name", sizeof(vc), vc);
	CU_ASSERT(0 == ret);
	CU_ASSERT(0 == strcmp(vc, "rhinotracheitis"));

	do {
		int buf[32] = {0};
		ret = rts_conf_scanf_ex(metadata, "%k%l%a",
				"height", sizeof(buf), buf);
		CU_ASSERT(ret > 0);
		CU_ASSERT(buf[0] == 5)
		CU_ASSERT(buf[1] == 8)
		CU_ASSERT(buf[2] == 11)
		CU_ASSERT(buf[3] == 15)
		CU_ASSERT(buf[4] == 22)
		CU_ASSERT(buf[5] == 30)
		CU_ASSERT(buf[6] == 50)

	} while (0);

	do {
		double buf[32] = {0};
		ret = rts_conf_scanf_ex(metadata, "%k%l%p",
				"weight", sizeof(buf), buf);
		CU_ASSERT(ret > 0);
		CU_ASSERT(fabs(buf[0] - 0.5) < 0.01);
		CU_ASSERT(fabs(buf[1] - 1.1) < 0.01);
		CU_ASSERT(fabs(buf[2] - 2.2) < 0.01);
		CU_ASSERT(fabs(buf[3] - 3.3) < 0.01);

	} while (0);

	json_object_put(root);
	free(metadata);
}

static void test_rts_conf_printf_ex()
{
	int ret = 0;
	struct metadata_object *metadata = NULL;

	metadata = (struct metadata_object *)malloc(
			sizeof(struct metadata_object));
	do {
		struct json_object *root;
		json_object *o;
		int get = 0;

		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;

		ret = rts_conf_printf_ex(metadata, "%k%s",
				"name", "Mewl");
		CU_ASSERT(0 == ret);
		CU_ASSERT(1 == ((struct metadata_object *)metadata)->dirty);
		get = json_object_object_get_ex(root, "name", &o);
		CU_ASSERT(get);
		CU_ASSERT(0 == strcmp("Mewl", json_object_get_string(o)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata, "%k%d",
				"age", 5);
		CU_ASSERT(0 == ret);
		get = json_object_object_get_ex(root, "age", &o);
		CU_ASSERT(get);
		CU_ASSERT(5 == json_object_get_int(o));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata,  "%k%i%s",
				"favor", 2, "CHICKENC");
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "favor", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 2);
		CU_ASSERT(0 == strcmp("CHICKENC", json_object_get_string(e)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata,  "%k%i%k%d",
				"inoculate", 2, "cost", 119);
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 2);
		get = json_object_object_get_ex(e, "cost", &ee);
		CU_ASSERT(get);
		CU_ASSERT(119 == json_object_get_int(ee));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata,  "%k%i%k%s",
				"inoculate", 1, "name", "fake_rhinotracheitis");
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 1);
		get = json_object_object_get_ex(e, "name", &ee);
		CU_ASSERT(get);
		CU_ASSERT(0 == strcmp("fake_rhinotracheitis",
					json_object_get_string(ee)));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *oo;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata, "%k%k%d",
				"master", "height", 189);
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "height", &oo);
		CU_ASSERT(189 == json_object_get_int(oo));

		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *oo;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata, "%k%k%f",
				"master", "height", 189.5);
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "height", &oo);
		CU_ASSERT((fabs(189.5 - json_object_get_double(oo)) < 0.001));

		json_object_put(root);
	} while (0);


	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		json_object *ee;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		/* add new object to array */
		ret = rts_conf_printf_ex(metadata,  "%k%i%k%s",
				"inoculate", 3, "name", "fake_inoculate");
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "inoculate", &o);
		CU_ASSERT(get);
		e = json_object_array_get_idx(o, 3);
		CU_ASSERT(e != NULL);
		if (e) {
			get = json_object_object_get_ex(e, "name", &ee);
			CU_ASSERT(get);
			CU_ASSERT(0 == strcmp("fake_inoculate",
						json_object_get_string(ee)));

		}
		json_object_put(root);
	} while (0);

	do {
		struct json_object *root;
		json_object *o;
		json_object *e;
		int get = 0;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		/* add new object to object */
		ret = rts_conf_printf_ex(metadata, "%k%k%d",
				"master", "weight", 63);
		CU_ASSERT(0 == ret);

		get = json_object_object_get_ex(root, "master", &o);
		CU_ASSERT(get);
		get = json_object_object_get_ex(o, "weight", &e);
		CU_ASSERT(get);
		if (get)
			CU_ASSERT(63 == json_object_get_int(e));
		json_object_put(root);
	} while (0);

	do {
		char vc[128] = {0};
		int vi = 0;
		struct json_object *root;
		root = json_tokener_parse(tokener_cat);
		metadata->root = (void *)root;
		ret = rts_conf_printf_ex(metadata, "%k%k%k%s",
				"parent", "father", "type", "Korat");
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf_ex(metadata, "%k%k%k%s",
				"parent", "father", "color", "White");
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf_ex(metadata, "%k%k%k%d",
				"parent", "father", "age", 10);
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf_ex(metadata, "%k%k%k%s",
				"parent", "mother", "color", "Black");
		CU_ASSERT(0 == ret);

		ret = rts_conf_printf_ex(metadata, "%k%k%k%d",
				"parent", "mother", "weight", 3);
		CU_ASSERT(0 == ret);

		ret = rts_conf_scanf_ex(metadata, "%k%k%k%l%s",
			"parent", "father", "color", sizeof(vc), vc);
		CU_ASSERT(0 == ret);
		CU_ASSERT(0 == strcmp("White", vc));

		ret = rts_conf_scanf_ex(metadata, "%k%k%k%d",
			"parent", "mother", "weight", &vi);
		CU_ASSERT(0 == ret);
		CU_ASSERT(3 == vi);
		json_object_put(root);
	} while (0);
	free(metadata);
}

static void test_rts_conf_free_metadata()
{

}

static struct test_item sysconf_test_items_gen[] = {
	{"test reset conf domain", test_rts_conf_reset},
	{NULL, NULL},
};

static struct test_item sysconf_test_items_scanf[] = {
	{"test get ctrls number", test_get_ctrls_num},
	{"test ctrl elements", test_create_ctrl_elements},
	{"test verify ctrl elements", test_verify_scanf_elements},
	{"test get array form json", test_rts_conf_scanf_array},
	{"test get param from json", test_get_param_from_json},
	{"test get param", test_rts_conf_scanf},
	{NULL, NULL},
};

static struct test_item sysconf_test_items_new_api[] = {
	{"test get metadata", test_rts_conf_get_metadata},
	{"test scanf_ex", test_rts_conf_scanf_ex},
	{"test printf_ex", test_rts_conf_printf_ex},
	{"test put metadata", test_rts_conf_put_metadata},
	{"test free metadata", test_rts_conf_free_metadata},
	{NULL, NULL},
};


static struct test_suite test_suite_scanf = {
	.name = "scanf",
	.init = __init,
	.cleanup = __cleanup,
	.items = sysconf_test_items_scanf,
};

static struct test_item sysconf_test_items_printf[] = {
	{"test verify ctrl elements", test_verify_printf_elements},
	{"test put param to json", test_put_param_to_json},
	{"test put param", test_rts_conf_printf},
	{NULL, NULL},
};

static struct test_suite test_suite_printf = {
	.name = "printf",
	.init = __init,
	.cleanup = __cleanup,
	.items = sysconf_test_items_printf,
};

static struct test_suite test_suite_gen = {
	.name = "generic",
	.init = __init,
	.cleanup = __cleanup,
	.items = sysconf_test_items_gen,
};

static struct test_suite test_suite_new_api = {
	.name = "new_api",
	.init = __init,
	.cleanup = __cleanup,
	.items = sysconf_test_items_new_api,
};

static CU_pSuite add_suite(struct test_suite *suite)
{
	CU_pSuite __suite = NULL;
	struct test_item *item = suite->items;

	__suite = CU_add_suite(suite->name, suite->init, suite->cleanup);

	if (!__suite)
		return NULL;

	if (!item)
		return __suite;

	do {
		CU_pTest test = NULL;
		if (!item->name || !item->func)
			break;

		test = CU_add_test(__suite, item->name, item->func);
		if (!test)
			break;
		item++;
	} while(1);

	return __suite;
}

/* extern struct test_suite test_suite_entropy; */

int main(int argc, char **argv)
{
	int ret = 0;

	ret = CU_initialize_registry();
	if (CUE_SUCCESS != ret)
		goto out;
	add_suite(&test_suite_scanf);
	add_suite(&test_suite_printf);
	add_suite(&test_suite_gen);
	/* add_suite(&test_suite_entropy); */
	add_suite(&test_suite_new_api);

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
out:
	CU_cleanup_registry();
	return CU_get_error();
}
