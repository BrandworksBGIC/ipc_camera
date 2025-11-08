const t = createTarget("instaview", "../../ipc_middleware/","../../../third/vo-aacenc/")
t.setTargetType('static')
t.addFiles("src/*.c")
t.addIncludeDirs("include")

const arch = config.get('arch')

t.addIncludeDirs(`sdk/${arch}/sdk_inc`)
t.addLdfiles(`sdk/${arch}/sdk_lib/*.so`)

t.build()
