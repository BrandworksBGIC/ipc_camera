path = require('path')



const allDriver = createTarget('allDriver', 'ipc_driver_motor','ipc_gpio_driver','m433_driver','rtl8188FU');
allDriver.setTargetType('void')