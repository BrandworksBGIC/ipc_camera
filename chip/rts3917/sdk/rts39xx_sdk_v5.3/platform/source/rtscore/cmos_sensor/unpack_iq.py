#!/usr/bin/env python3

import os
import json
import struct
import google.protobuf.json_format as json_format
from isp_iq_table_pb2 import *


def dump_packed_iq(packed_iq):
    def json_name(index):
        return '{}_iq{}.json'.format(os.path.splitext(packed_iq)[0], index)

    with open(packed_iq, 'rb') as f:
        header_fmt = '<IIQBBB13s'
        group_fmt = '<32sBB14s'
        binary_fmt = '<II8s'
        header_size = struct.calcsize(header_fmt)
        group_size = struct.calcsize(group_fmt)
        binary_size = struct.calcsize(binary_fmt)

        buf = f.read()
        _, _, _, group_num, _, binary_num, _ = \
                struct.unpack(header_fmt, buf[:header_size])
        for i in range(group_num):
            start = header_size + i * group_size
            end = start + group_size
            name, day_id, night_id, _ = struct.unpack(group_fmt, buf[start:end])
            name = name.decode().rstrip('\0')

            print('{}:'.format(name))
            print('  day => {}'.format(json_name(day_id)))
            print('  night => {}'.format(json_name(night_id)))

        for i in range(binary_num):
            binary_start = header_size + group_num * group_size
            start = binary_start + i * binary_size
            end = start + binary_size
            offset, length, _ = struct.unpack(binary_fmt, buf[start:end])
            iq = isp_iq_table_t()
            iq.ParseFromString(buf[offset:offset + length])
            data = json_format.MessageToJson(iq,
                    including_default_value_fields=True,
                    preserving_proto_field_name=True)
            data = prettyjson(json.loads(data))
            with open(json_name(i), 'w') as out_f:
                out_f.write(data)

def dump_naked_iq(naked_iq):
    json_iq = '{}.json'.format(os.path.splitext(naked_iq)[0])
    with open(naked_iq, 'rb') as f:
        iq = isp_iq_table_t()
        iq.ParseFromString(f.read())
        data = json_format.MessageToJson(iq,
                including_default_value_fields=True,
                preserving_proto_field_name=True)
        data = prettyjson(json.loads(data))
        with open(json_iq, 'w') as out_f:
            out_f.write(data)
        print('Generate {} from naked iq {}'.format(json_iq, naked_iq))


# https://stackoverflow.com/a/56497521/104668

def prettyjson(obj, indent=2, maxlinelength=80):
    """Renders JSON content with indentation and line splits/concatenations
    to fit maxlinelength.  Only dicts, lists and basic types are supported"""

    items, _ = getsubitems(obj, itemkey="", islast=True,
                           maxlinelength=maxlinelength - indent, indent=indent)
    return indentitems(items, indent, level=0) + "\n"


def getsubitems(obj, itemkey, islast, maxlinelength, indent):
    items = []
    is_inline = True

    isdict = isinstance(obj, dict)
    islist = isinstance(obj, list)
    istuple = isinstance(obj, tuple)
    isbasictype = not (isdict or islist or istuple)

    if isbasictype:
        keyseparator  = "" if itemkey == "" else ": "
        itemseparator = "" if islast else ","
        items.append(itemkey + keyseparator +
                     basictype2str(obj) + itemseparator)

    else:
        if isdict:    opening, closing, keys = ("{", "}", iter(obj.keys()))
        elif islist:  opening, closing, keys = ("[", "]", range(0, len(obj)))
        elif istuple: opening, closing, keys = ("[", "]", range(0, len(obj)))

        if itemkey != "": opening = itemkey + ": " + opening
        if not islast: closing += ","

        count = 0
        itemkey = ""
        subitems = []

        for (i, k) in enumerate(keys):
            islast_ = i == len(obj)-1
            itemkey_ = ""
            if isdict: itemkey_ = basictype2str(k)
            inner, is_inner_inline = getsubitems(obj[k], itemkey_, islast_,
                                                 maxlinelength - indent, indent)
            subitems.extend(inner)
            is_inline = is_inline and is_inner_inline

        if is_inline:
            multiline = True
            if (isdict): multiline = False
            if (islist): multiline = True

            if (multiline):
                lines = []
                current_line = ""
                current_index = 0

                for (i, item) in enumerate(subitems):
                    item_text = item
                    if i < len(inner)-1: item_text = item + ","

                    if len (current_line) > 0:
                        try_inline = current_line + " " + item_text
                    else:
                        try_inline = item_text

                    if (len(try_inline) > maxlinelength):
                        if len(current_line) > 0: lines.append(current_line)
                        current_line = item_text
                    else:
                        current_line = try_inline

                    if (i == len (subitems)-1): lines.append(current_line)

                subitems = lines
                if len(subitems) > 1: is_inline = False
            else:
                totallength = len(subitems)-1
                for item in subitems: totallength += len(item)
                if (totallength <= maxlinelength):
                    str = ""
                    for item in subitems: str += item + " "
                    subitems = [ str.strip() ]
                else:
                    is_inline = False


        if is_inline:
            item_text = ""
            if len(subitems) > 0: item_text = subitems[0]
            if len(opening) + len(item_text) + len(closing) <= maxlinelength:
                items.append(opening + item_text + closing)
            else:
                is_inline = False

        if not is_inline:
            items.append(opening)
            items.append(subitems)
            items.append(closing)

    return items, is_inline


def basictype2str(obj):
    if isinstance (obj, str):
        strobj = "\"" + str(obj) + "\""
    elif isinstance(obj, bool):
        strobj = { True: "true", False: "false" }[obj]
    else:
        strobj = str(obj)
    return strobj


def indentitems(items, indent, level):
    """Recursively traverses the list of json lines,
    adds indentation based on the current depth"""
    res = ""
    indentstr = " " * (indent * level)
    for (i, item) in enumerate(items):
        if isinstance(item, list):
            res += indentitems(item, indent, level+1)
        else:
            islast = (i==len(items)-1)
            if level==0 and islast:
                res += indentstr + item
            else:
                res += indentstr + item + "\n"
    return res

def print_help():
    print('''\
Usage:
    {0} packed_iq.bin
    {0} --naked naked_iq.bin'''.format(os.sys.argv[0]))
    os.sys.exit()

if __name__ == '__main__':
    if len(os.sys.argv) < 2 or os.sys.argv[1] in {'-h', '--help'}:
        print_help()

    if len(os.sys.argv) == 2:
        dump_packed_iq(os.sys.argv[1])
    elif len(os.sys.argv) == 3 and os.sys.argv[1] == '--naked':
        dump_naked_iq(os.sys.argv[2])
