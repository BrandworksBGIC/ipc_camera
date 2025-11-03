#!/usr/bin/env python3

import sys
import re


class Migrate:
    blank_pattern = re.compile(r'^\n$')
    func_begin_pattern = re.compile(r'^\{\n$')
    func_end_pattern = re.compile(r'^\}\n$')
    info_begin_pattern = re.compile(r'^static int (\w+)_get_info\(.*\n$')
    info_interface_pattern = re.compile(
        r'^\s+(info->interface\.|MIPI_LANE).*\n$')
    info_i2c_pattern = re.compile(r'^\s+info->i2c.*\n$')
    info_crop_pattern = re.compile(r'^\s+info->(size\.[wh]|crop\.[xy]).*\n$')
    info_size_pattern = re.compile(r'^\s+info->crop\.[wh].*\n$')
    init_info_pattern = re.compile(
        r'^static int \w+_get_init_info\(.*\n$')
    init_info_timing_pattern = re.compile(
        r'^\s+info->(hts|pclk|min_vts|max_vts) = .*\n$')
    start_func_pattern = re.compile(r'^static int \w+_start\(.*\n$')
    exp_gain_pattern = re.compile(
        r'^static int \w+_get_exposure_gain_info\(.*\n$')
    fpga_fps_pattern = re.compile(
        r'^static\s+const\s+struct\s+fps_info\s+(\w+fps_info(_dvp|_mipi)?_fpga)\[\] = \{\n$')
    fps_pattern = re.compile(
        r'^static\s+const\s+struct\s+fps_info\s+(\w+fps_info(_dvp|_mipi)?(_asic)?)\[\] = \{\n$')
    fps_assign_pattern = re.compile(
        r'^\s+info->fps.fps\[0\] = (.*);\n$')

    def __init__(self, filename: str):
        self.filename = filename
        self.file = open(self.filename, 'r')
        self.code = self.file.readlines()
        self.cur_line = 0
        self.lines = len(self.code)

        self.outfile = None

        self.name = ''

        self.header = ''

        self.info_header = ''
        self.info_interface = ''
        self.info_i2c = ''
        self.info_crop = ''
        self.info_size = 0, 0
        self.info_power = ''
        self.info_fps = ''
        self.info_footer = ''

        self.init_info_pre = ''
        self.init_info_header = ''
        self.init_info_timing = ''
        self.init_info_footer = ''

        self.start_func = ''

        self.exp_gain_mapping = ''
        self.exp_gain_func = ''

        self.footer = ''

        self.has_fpga = False
        self.fps = 0
        self.fpga_fps = 0

        self.has_last_exposure = False
        self.has_reg_status = False
        self.gain_reg_name = ''
        self.again_reg_name = ''
        self.again_reg_type = ''
        self.dgain_reg_name = ''
        self.dgain_reg_type = ''
        self.get_reg_statement = ''
        self.get_gain_statement = ''

        self.new_code = ''

    def __del__(self):
        if self.file:
            self.file.close()
        if self.outfile:
            self.outfile.close()

    @property
    def cur_code(self) -> str:
        return self.code[self.cur_line] if self.line_valid() else ''

    def line_valid(self) -> bool:
        return self.cur_line < self.lines

    def next_line(self):
        self.cur_line += 1

    def reset_line(self):
        self.cur_line = 0

    def drop_blank(self):
        while self.line_valid() and self.blank_pattern.match(self.cur_code):
            self.next_line()

    def _parse_header(self):
        while self.line_valid():
            res = self.info_begin_pattern.match(self.cur_code)
            if not res:
                self.header += self.cur_code
                self.next_line()
            else:
                self.name = res.group(1)
                break
        else:
            raise Exception('can not found header end')
        self.header = re.sub(r'\n\n$', r'\n', self.header)

    def _parse_info_header(self):
        if not self.info_begin_pattern.match(self.cur_code):
            raise Exception('can not found get_info func')
        while self.line_valid():
            if not self.info_interface_pattern.match(self.cur_code):
                self.info_header += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info header end')
        self.info_header = re.sub(r'\n\n$', r'\n', self.info_header)

    def _parse_info_interface(self):
        if not self.info_interface_pattern.match(self.cur_code):
            raise Exception('can not found info interface')
        while self.line_valid():
            self.drop_blank()
            if not self.info_i2c_pattern.match(self.cur_code):
                self.info_interface += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info interface end')

    def _parse_info_i2c(self):
        self.drop_blank()
        if not self.info_i2c_pattern.match(self.cur_code):
            raise Exception('can not found info i2c')
        while self.line_valid():
            self.drop_blank()
            if self.info_i2c_pattern.match(self.cur_code):
                self.info_i2c += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info i2c end')

    def _parse_info_crop(self):
        self.drop_blank()
        if not self.info_crop_pattern.match(self.cur_code):
            raise Exception('can not found info crop')
        while self.line_valid():
            self.drop_blank()
            if self.info_crop_pattern.match(self.cur_code):
                self.info_crop += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info i2c end')

    def _parse_info_size(self):
        self.drop_blank()
        if not self.info_size_pattern.match(self.cur_code):
            raise Exception('can not found info size')
        info_size = ''
        while self.line_valid():
            self.drop_blank()
            if self.info_size_pattern.match(self.cur_code):
                info_size += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info i2c end')
        self.info_size = tuple(i for i in re.findall(
            r'(?<== ).*(?=;)', info_size))

    def _parse_info_power(self):
        self.drop_blank()
        if not re.match(r'^\s+(i = 0|up->num = 0);\n$', self.cur_code):
            return
        while self.line_valid():
            self.drop_blank()
            self.info_power += self.cur_code
            if not re.match(r'^\s+down->num.*\n$', self.cur_code):
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info power end')

    def _parse_info_footer(self):
        while self.line_valid():
            if not re.match(r'^\s+return RTS_ISP_OK;\n$', self.cur_code):
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found info footer end')
        while self.line_valid():
            self.info_footer += self.cur_code
            if self.func_end_pattern.match(self.cur_code):
                self.next_line()
                break
            self.next_line()
        else:
            raise Exception('can not found info footer end')

    def _parse_and_split_info(self):
        self._parse_info_header()
        self._parse_info_interface()
        self._parse_info_i2c()
        self._parse_info_crop()
        self._parse_info_size()
        self._parse_info_power()
        self._parse_info_footer()

    def _parse_init_info_pre(self):
        self.drop_blank()
        while self.line_valid():
            if not self.init_info_pattern.match(self.cur_code):
                self.init_info_pre += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found init info header end')
        self.init_info_pre = re.sub(r'\n\n$', r'\n', self.init_info_pre)

    def _parse_init_info_header(self):
        self.drop_blank()
        while self.line_valid():
            if not self.init_info_timing_pattern.match(self.cur_code):
                self.init_info_header += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found init info header end')
        self.init_info_header = re.sub(r'\n\n$', r'\n', self.init_info_header)

    def _parse_init_info_timing(self):
        self.drop_blank()
        if not self.init_info_timing_pattern.match(self.cur_code):
            raise Exception('can not found init info timing')
        while self.line_valid():
            if self.init_info_timing_pattern.match(self.cur_code):
                self.init_info_timing += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found init info timing end')
        self.init_info_timing = re.sub(r'\n\n$', r'\n', self.init_info_timing)

    def _parse_init_info_footer(self):
        self.drop_blank()
        while self.line_valid():
            self.init_info_footer += self.cur_code
            if self.func_end_pattern.match(self.cur_code):
                self.next_line()
                break
            self.next_line()
        else:
            raise Exception('can not found init info footer end')

    def _parse_and_split_init_info(self):
        self._parse_init_info_pre()
        self._parse_init_info_header()
        self._parse_init_info_timing()
        self._parse_init_info_footer()

    def _parse_start_func(self):
        self.drop_blank()
        if not self.start_func_pattern.match(self.cur_code):
            return
        while self.line_valid():
            self.start_func += self.cur_code
            if self.func_end_pattern.match(self.cur_code):
                self.next_line()
                break
            else:
                self.next_line()
        else:
            raise Exception('can not found exp gain function end')

    def _parse_gain_mapping(self):
        self.drop_blank()
        while self.line_valid():
            if not self.exp_gain_pattern.match(self.cur_code):
                self.exp_gain_mapping += self.cur_code
                self.next_line()
            else:
                break
        else:
            raise Exception('can not found exp gain function')
        self.exp_gain_mapping = re.sub(r'\n\n$', r'\n', self.exp_gain_mapping)

    def _parse_exp_gain(self):
        if not self.exp_gain_pattern.match(self.cur_code):
            raise Exception('can not found exp gain function')
        while self.line_valid():
            self.exp_gain_func += self.cur_code
            if self.func_end_pattern.match(self.cur_code):
                self.next_line()
                break
            else:
                self.next_line()
        else:
            raise Exception('can not found exp gain function end')
        self.has_last_exposure = self.exp_gain_func.find('last_exposure') >= 0
        gain_reg_name = re.search(r'\w+(?= = get_sensor_gain_reg)',
                                  self.exp_gain_func)
        if gain_reg_name:
            self.gain_reg_name = gain_reg_name.group(0)
            self.get_reg_statement = \
                re.search(r'get_sensor_gain_reg\(.*?(\n.*?)?\);\n',
                          self.exp_gain_func).group(0)
            self.get_reg_statement = \
                re.sub(r'get_sensor_gain_reg\(.*?(\n.*?)?(?=(,|\)))',
                       'get_sensor_gain_reg(again[0]', self.get_reg_statement)
            self.get_gain_statement = \
                re.search(r'get_sensor_real_gain\(.*(\n.*)?\);\n',
                          self.exp_gain_func).group(0)
            self.has_reg_status = self.get_reg_statement.find('status') >= 0
        if self.get_reg_statement:
            return
        gain_reg_name = re.search(r'get_sensor_gain_reg(?:_value)?\(.*?,\n?\s+&(\w+), &(\w+)\);',
                                  self.exp_gain_func)
        if gain_reg_name:
            self.get_reg_statement = gain_reg_name.group(0)
            self.get_reg_statement = \
                re.sub(r'\(.*?,\n?\s+', '(again[0], ', self.get_reg_statement)
            self.again_reg_name = gain_reg_name.group(1)
            self.dgain_reg_name = gain_reg_name.group(2)
            self.get_gain_statement = \
                re.search(r'get_sensor_real_gain\(.*(\n.*)?\);\n',
                          self.exp_gain_func).group(0)
            self.again_reg_type = \
                re.search(r'\t(\w+) {};'.format(self.again_reg_name),
                          self.exp_gain_func).group(1)
            self.dgain_reg_type = \
                re.search(r'\t(\w+) {};'.format(self.dgain_reg_name),
                          self.exp_gain_func).group(1)

    def _parse_and_split_exp_gain(self):
        self._parse_gain_mapping()
        self._parse_exp_gain()

    def _parse_footer(self):
        self.drop_blank()
        while self.line_valid():
            self.footer += self.cur_code
            self.next_line()
        self.footer = re.sub(r'\n\n$', r'\n', self.footer)

    def _parse_fps(self):
        fps_pattern = re.compile(r'^\s+\{\s*(\d+),')
        self.reset_line()
        while self.line_valid():
            if self.fpga_fps_pattern.match(self.cur_code):
                fpga_fps_name = self.fpga_fps_pattern.match(
                    self.cur_code).group(1)
                self.has_fpga = True
                self.next_line()
                i = 0
                fps = 0
                while self.line_valid():
                    res = fps_pattern.match(self.cur_code)
                    if res:
                        cur_fps = int(res.group(1))
                        if cur_fps > fps:
                            fps = cur_fps
                            self.fpga_fps = '{}[{}].fps'.format(
                                fpga_fps_name, i)
                        self.next_line()
                        i += 1
                    else:
                        break
            elif self.fps_pattern.match(self.cur_code):
                fps_name = self.fps_pattern.match(self.cur_code).group(1)
                self.next_line()
                i = 0
                fps = 0
                while self.line_valid():
                    res = fps_pattern.match(self.cur_code)
                    if res:
                        cur_fps = int(res.group(1))
                        if cur_fps > fps:
                            fps = cur_fps
                            self.fps = '{}[{}].fps'.format(fps_name, i)
                        self.next_line()
                        i += 1
                    else:
                        break
            else:
                self.next_line()
        if not self.fps:
            self.reset_line()
            while self.line_valid():
                res = self.fps_assign_pattern.match(self.cur_code)
                if res:
                    self.fps = res.group(1)
                    break
                self.next_line()
        if not self.fps:
            raise Exception('Can not find fps info')

    def _parse_special_info(self):
        self._parse_fps()

    def _parse_code(self):
        self._parse_header()
        self._parse_and_split_info()
        self._parse_and_split_init_info()
        self._parse_start_func()
        self._parse_and_split_exp_gain()
        self._parse_footer()

    def _modify_info_code(self):
        self.info_crop = \
            self.info_crop.replace('info->crop.', 'info->start.')
        self.info_interface = \
            self.info_interface.replace('yuv_type',
                                        'type_config.yuv.order.yuv422')

    def _modify_init_info_code(self):
        length = len('static int _get_init_info(') + len(self.name)
        space = '\t' * (length // 8) + ' ' * (length % 8)
        self.init_info_header = \
            self.init_info_header.replace(
                ', uint16_t fps,',
                ',\n{}const struct rts_isp_sensor_mode *mode,'.format(space))
        self.init_info_header = \
            self.init_info_header.replace('get fps %u', 'get fps %.1f')
        self.init_info_header = \
            re.sub(r'(?<!(->|\("|t ))\bfps\b',
                   'mode->fps', self.init_info_header)
        self.init_info_timing = \
            re.sub(r'(?<!(->|\("|t ))\bfps\b',
                   'mode->fps', self.init_info_timing)
        self.init_info_footer = \
            re.sub(r'(?<!(->|\("|t ))\bfps\b',
                   'mode->fps', self.init_info_footer)
        if self.has_last_exposure:
            self.init_info_footer = \
                self.init_info_footer.replace(
                    '\tstatus->last_exposure = 0;\n', '')

    def _modify_start_code(self):
        if self.has_last_exposure:
            if self.start_func:
                self.start_func = \
                    self.start_func.replace(
                        '\treturn RTS_ISP_OK;\n}',
                        '\tstatus->last_exposure = 0;\n\n\treturn RTS_ISP_OK;\n}')
            else:
                self.start_func = \
                    'static int {}_start(uint32_t isp_id)\n'.format(self.name)
                self.start_func += '{\n'
                self.start_func += '\tstruct {}_status *status;\n\n'.format(
                    self.name)
                self.start_func += '\tif (isp_id >= SUPPORTED_ISP_NUM)\n'
                self.start_func += '\t\treturn -RTS_ISP_EINVAL;\n\n'
                self.start_func += '\tstatus = &g_status[isp_id];\n\n'
                self.start_func += '\tstatus->last_exposure = 0;\n\n'
                self.start_func += '\treturn RTS_ISP_OK;\n}\n'

    def _modify_exp_gain_code(self):
        if self.get_reg_statement:
            self.exp_gain_func = \
                re.sub(r'\n\s+exp_gain->analog_gain = .*;\n',
                       '\n', self.exp_gain_func)
            self.exp_gain_func = \
                re.sub(r'\n\s+exp_gain->digital_gain = .*;\n',
                       '\n', self.exp_gain_func)
        self.exp_gain_func = \
            self.exp_gain_func.replace(
                'struct rts_isp_exp_gain *',
                'const struct rts_isp_sensor_exp_gain *')
        self.exp_gain_func = \
            self.exp_gain_func.replace(
                'exp_gain->exposure', 'exp_gain->exposure[0]')
        self.exp_gain_func = \
            self.exp_gain_func.replace(
                'exp_gain->analog_gain', 'exp_gain->analog_gain[0]')
        self.exp_gain_func = \
            self.exp_gain_func.replace(
                'exp_gain->digital_gain', 'exp_gain->digital_gain[0]')
        self.exp_gain_func = \
            self.exp_gain_func.replace(
                'status->min_vts + exp_gain->extra_dummy', 'exp_gain->vts')
        self.exp_gain_func = \
            self.exp_gain_func.replace(
                'exp_gain->extra_dummy + status->min_vts', 'exp_gain->vts')
        self.exp_gain_func = \
            re.sub(r'reg\[i\].info.delay_frames = (\d+);\n+\s+reg\[i\].info.interrupt = (\w+);',
                   r'set_sync_info(&reg[i++], \1, \2);', self.exp_gain_func)
        self.exp_gain_func = \
            re.sub(r'reg\[i\].info.interrupt = (\w+);\n+\s+reg\[i\].info.delay_frames = (\d+);',
                   r'set_sync_info(&reg[i++], \2, \1);', self.exp_gain_func)
        if self.has_reg_status:
            self.exp_gain_func = \
                re.sub(r'\t\w+ {};\n'.format(self.gain_reg_name),
                       '', self.exp_gain_func)
            self.exp_gain_func = \
                re.sub(r'\t{} = get_sensor_gain_reg\(.*?(\n.*?)?\);\n'
                       .format(self.gain_reg_name),
                       '', self.exp_gain_func)
        self.exp_gain_func = \
            self.exp_gain_func.replace('\n\n\n', '\n\n')

    def _modify_footer_code(self):
        again_code = '\t.get_tuned_again = {}_get_tuned_again,\n'\
            .format(self.name)
        dgain_code = '\t.get_tuned_dgain = {}_get_tuned_dgain,\n'\
            .format(self.name)
        self.footer = self.footer.replace('.ops_version = SENSOR_OPS_VERSION',
                                          '.api_version = SENSOR_API_VERSION')
        self.footer = self.footer.replace(
            '\t.get_exposure_gain_info',
            '{}{}\t.get_exposure_gain_info'.format(again_code, dgain_code))
        if self.footer.find('.start =') < 0:
            self.footer = re.sub(
                r'(?<=\.get_init_info = {}_get_init_info,\n)'.format(self.name),
                '\t.start = {}_start,\n'.format(self.name), self.footer)
        self.footer = re.sub(
            r'const struct rts_isp_sensor_ops\s+\*rts_isp_get_sensor_ops\(void\)\n\{\n\s+return \&(\w+);\n\}\n+',
            r'RTS_ISP_DEFINE_SENSOR_PLUGIN(\1)', self.footer)

    def _modify_code(self):
        self._modify_info_code()
        self._modify_init_info_code()
        self._modify_start_code()
        self._modify_exp_gain_code()
        self._modify_footer_code()

    def _construct_header_code(self):
        self.new_code += self.header

    def _construct_info_code(self):
        self.new_code += '\n' + self.info_header + '\n'
        self.new_code += '\tinfo->modes.mode[0].hdr = RTS_ISP_HDR_NONE;\n'
        self.new_code += '\tinfo->modes.mode[0].size.w = {};\n'\
            .format(self.info_size[0])
        self.new_code += '\tinfo->modes.mode[0].size.h = {};\n'\
            .format(self.info_size[1])
        if self.fpga_fps:
            self.new_code += '\tif (isp_driver_is_fpga())\n'
            self.new_code += '\t\tinfo->modes.mode[0].fps = {};\n'\
                .format(self.fpga_fps)
            self.new_code += '\telse\n'
            self.new_code += '\t\tinfo->modes.mode[0].fps = {};\n'\
                .format(self.fps)
        else:
            self.new_code += '\tinfo->modes.mode[0].fps = {};\n'\
                .format(self.fps)
        self.new_code += '\tinfo->modes.num = 1;\n'
        self.new_code += '\n' + self.info_i2c
        self.new_code += '\n' + self.info_power
        self.new_code += '\n' + self.info_footer

    def _construct_init_info_code(self):
        self.new_code += '\n' + self.init_info_pre
        self.new_code += '\n' + self.init_info_header
        self.new_code += '\n' + self.info_interface
        self.new_code += '\n' + self.info_crop
        self.new_code += '\n' + self.init_info_timing
        self.new_code += '\n' + self.init_info_footer

    def _construct_start_code(self):
        if self.start_func:
            self.new_code += '\n' + self.start_func

    def _generate_again_code(self):
        length = len('static int _get_tuned_again(') + len(self.name)
        space = '\t' * (length // 8) + ' ' * (length % 8)
        code = ''
        code += 'static int {}_get_tuned_again(uint32_t isp_id,\n{}float again[RTS_ISP_HDR_CHAN_MAX])\n'\
            .format(self.name, space)
        code += '{\n'
        if self.gain_reg_name:
            code += '\tint {};\n'.format(self.gain_reg_name)
            if self.has_reg_status:
                code += '\tstruct {}_status *status;\n'.format(self.name)
            code += '\n\tif (isp_id >= SUPPORTED_ISP_NUM || !again)\n'
            code += '\t\treturn -RTS_ISP_EINVAL;\n\n'
            if self.has_reg_status:
                code += '\tstatus = &g_status[isp_id];\n\n'.format(self.name)
            code += '\t{} = {}'.format(self.gain_reg_name,
                                       self.get_reg_statement)
            code += '\tagain[0] = {}\n'.format(self.get_gain_statement)
        elif self.again_reg_name:
            code += '\t{} {};\n'.format(self.again_reg_type,
                                        self.again_reg_name)
            code += '\t{} {};\n'.format(self.dgain_reg_type,
                                        self.dgain_reg_name)
            code += '\n\tif (isp_id >= SUPPORTED_ISP_NUM || !again)\n'
            code += '\t\treturn -RTS_ISP_EINVAL;\n\n'
            code += '\t{}\n'.format(self.get_reg_statement)
            code += '\tagain[0] = {}\n'.format(self.get_gain_statement)
        code += '\treturn RTS_ISP_OK;\n}\n'
        return code

    def _generate_dgain_code(self):
        length = len('static int _get_tuned_dgain(') + len(self.name)
        space = '\t' * (length // 8) + ' ' * (length % 8)
        code = ''
        code += 'static int {}_get_tuned_dgain(uint32_t isp_id,\n{}float dgain[RTS_ISP_HDR_CHAN_MAX])\n'\
            .format(self.name, space)
        code += '{\n'
        if self.get_reg_statement:
            code += '\tif (isp_id >= SUPPORTED_ISP_NUM || !dgain)\n'
            code += '\t\treturn -RTS_ISP_EINVAL;\n\n'
            code += '\tdgain[0] = 1.0f;\n\n'
        code += '\treturn RTS_ISP_OK;\n}\n'
        return code

    def _construct_exp_gain_code(self):
        self.new_code += '\n' + self.exp_gain_mapping
        self.new_code += '\n' + self._generate_again_code()
        self.new_code += '\n' + self._generate_dgain_code()
        self.new_code += '\n' + self.exp_gain_func

    def _construct_footer_code(self):
        self.new_code += '\n' + self.footer + '\n'

    def _construct_code_post(self):
        self.new_code = self.new_code.replace('row_time', 'exp_step')

    def _construct_code(self):
        self._construct_header_code()
        self._construct_info_code()
        self._construct_init_info_code()
        self._construct_start_code()
        self._construct_exp_gain_code()
        self._construct_footer_code()
        self._construct_code_post()

    def migrate(self, outfile: str):
        self._parse_code()
        self._parse_special_info()
        self._modify_code()
        self._construct_code()
        self.outfile = open(outfile, 'w+')
        self.outfile.write(self.new_code)


def migrate(infile: str, outfile: str):
    print('parsing {} => {}'.format(infile, outfile))
    m = Migrate(infile)
    m.migrate(outfile)


if __name__ == '__main__':
    if len(sys.argv) == 2:
        migrate(sys.argv[1], sys.argv[1])
    elif len(sys.argv) == 3:
        migrate(sys.argv[1], sys.argv[2])
    else:
        print("Usage python3 migrate.py sensor.c [out.c]")
