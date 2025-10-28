import * as gpio from "gpio"

gpio.setMode(4, "input")
gpio.pull(4, "up")
gpio.setMode(5, "output")

let outputValue = 0
setInterval(() => {

    // 主动读取输入
    const value = gpio.read(4)
    console.log("Read GPIO4:", value)

    // 输出
    gpio.write(5, outputValue)
    console.log("Wrote GPIO5:", outputValue)
    if(outputValue === 0) {
        outputValue = 1
    }

}, 1000)

// 监听输入变化
gpio.watch(4, "both", (pin, value) => {
    console.log(`GPIO${pin} changed to ${value}`)
})
