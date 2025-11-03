git.cloneTag('https://github.com/spock2300/ed25519/', 'v1.0.3', "src")

const t = createTarget("ed25519")
t.setTargetType('static')
t.addFiles('src/src/*.c')
t.addIncludeDirs('src/src', true)
t.build()