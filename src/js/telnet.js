import { importSync, exportValue } from 'loader'
import * as telnet from 'telnet'
import * as mg from 'mg'

// websocket
const ws = {
    connect(url) {

        let channel = new telnet.TelnetChannel()

        let conn = mg.connect(url, (ev, data, isBinary) => {
            if (ev == "ws.msg") {
                channel && channel.process(data)
            }
            else if (['error', 'close'].includes(ev)) {
                channel = null
            }
        })

        channel.on("output-stream", (data) => {
            if (conn && conn.isConnected()) {
                conn.send(data)
            }
        })

        return conn
    }
}

exportValue(telnet, "ws", ws)

// BLE
try {
    var bt = importSync("bt")
} catch (e) { }

let bleStarted = false
let bleChannel = null

const ble = {
    start(advName = null, serviceId = "0B0C", charId = "0512") {
        if (!bt) {
            throw new Error("bt module not used, call `beshell.use<BT>()` in C++ first")
        }
        if (bleStarted) {
            throw new Error("telnet.ble already started")
        }
        bt.periph.init()

        bt.setMTU(256)

        bt.periph.addService({
            uuid: serviceId,
            chars: [{
                uuid: charId,
                props: ["write", "notify"]
            }]
        })

        bt.periph.char[charId].on("write", (data) => {
            bleChannel && bleChannel.process(data)
        })

        bt.on("disconnect", () => {
            bleChannel = null
            bt.startAdv()
        })

        bt.on("connect", () => {
            bleChannel = new telnet.TelnetChannel()
            bleChannel.on("output-stream", (data) => {
                bt.periph.char[charId] && bt.periph.char[charId].notify(data)
            })
        })

        if (advName) {
            let u8arr = new Uint8Array(advName.length + 2);
            u8arr[0] = advName.length + 1; // length
            u8arr[1] = 0x09; // type
            for (let i = 0; i < advName.length; i++) {
                u8arr[i + 2] = advName.charCodeAt(i); // data
            }
            bt.setAdvData(u8arr.buffer)
            bt.startAdv({
                min: 160,
                max: 170, // 间隔时间: 250-300ms
            })
        }

        bleStarted = true
    }
}

exportValue(telnet, "ble", ble)

// USB CDC
try {
    var cdc = importSync("cdc")
} catch (e) { }
exportValue(telnet, "cdc", {
    start() {
        if (!cdc) {
            throw new Error("cdc module not used, call `beshell.use<CDC>()` in C++ first")
        }
        try { cdc.setup() } catch (e) { }
        cdc.on("data", (data) => {
            cdcChannel && cdcChannel.process(data)
        })
        let cdcChannel = new telnet.TelnetChannel()
        cdcChannel.on("output-stream", (data) => {
            cdc && cdc.write(data)
        })
    }
})

// log
;(function () {

    let logger = null
    let openedFile = -1
    let storeDir = "/store/log"
    let writingFilePath = null
    let wroteBytes = 0
    let fileSize = 1024
    let maxFiles = 50

    function startLog(options) {
        const fs = importSync("fs")
        if (logger) {
            throw new Error("telnet.startLog already started")
        }
        if(!options){
            throw new Error("telnet.startLog requires options")
        }
        if(!options.dir){
            throw new Error("telnet.startLog requires options.dir")
        }
        storeDir = options.dir
        if( !fs.mkdirSync(storeDir, true) ){
            throw new Error("telnet.startLog failed to create directory: " + storeDir)
        }
        fileSize = parseInt(options.fileSize)
        if(isNaN(fileSize)){
            fileSize = 1024*100
        }
        maxFiles = parseInt(options.maxFiles)
        if(isNaN(maxFiles)){
            maxFiles = 20
        }

        logger = new telnet.TelnetChannel()
        logger.on("output", writeLog)
        logger.enableEventOutput()
    }
    exportValue(telnet, "startLog", startLog)
    exportValue(telnet, "stopLog", function stopLog() {
        logger = null
    })
    function writeLog (data) {

        if( !writingFilePath || wroteBytes>fileSize ) {
            createNewLogFile()
        }

        let content = `[${new Date().toISOString().replace("T", ' ').slice(0,19)}] ${data.asString()}`
        if(content[content.length-1] != '\n') {
            content += '\n'
        }
        fs.writeFileSync(writingFilePath, content, true)
        wroteBytes+= content.length
    }
    function createNewLogFile() {
        let logFiles = findLastFile(storeDir)
        if (logFiles.length > maxFiles) {
            logFiles.slice(0, logFiles.length - maxFiles).forEach(item => {
                fs.rmSync(storeDir+"/"+item.name)
            })
        }

        if(logFiles.length > 0) {
            var fileId = logFiles.pop().fileId + 1
        } else {
            var fileId = 0
        }

        writingFilePath = `${storeDir}/${fileId}-${new Date().toISOString().replace(/[-:]/g, '').replace("T",'_').slice(2, 13)}.txt`
        return writingFilePath
    }
    function findLastFile(dir) {
        return fs.listDirSync(dir,true).filter(item => {
            if(item.type=="file") {
                let res = item.name.match(/^(\d+)\-\d{6}_\d{4}\.txt$/i)
                if(!res) {
                    return false
                }
                item.fileId = parseInt(res[1])
                return true
            }
            return false
        }).sort((a, b) => {
            return a.fileId - b.fileId
        })
    }

})()
