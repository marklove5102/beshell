import {i2c0} from "serial"

const PIN_SDA = 9
const PIN_SCL = 10


function main() {

    i2c0.setup({
        sda: PIN_SDA ,
        scl: PIN_SCL ,
    })

    // 扫描总线上的设备
    i2c0.scan()

    i2c0.addDevice({
        addr: 0x50 ,
        regBits: 8,
        regAddrBits: 8,
    })

    console.log("从地址 100 开始，逐次写入10个字节")
    for(let i=0;i<10;i++) {
        let ret = i2c0.write8(0x50, 100+i, i)
        console.log("write byte", i, "@"+(100+i), ret? "success":"fail")
    }

    console.log("从地址 100，一次读回 10 个字节:")
    console.log( ... i2c0.readU8(0x50, 100, 10) )


}

main()
