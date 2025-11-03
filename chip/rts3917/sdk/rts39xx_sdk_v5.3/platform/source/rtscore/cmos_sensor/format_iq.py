#!/usr/bin/env python3

import json
import sys


def format_json(file_name):
    with open(file_name, 'r+') as f:
        data = f.read()
        data = prettyjson(json.loads(data))
        f.seek(0)
        f.truncate()
        f.write(data)


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


if __name__ == '__main__':
    if (len(sys.argv) != 2):
        print('Usage: {} iq_json_file'.format(sys.argv[0]))
        sys.exit()
    format_json(sys.argv[1])
