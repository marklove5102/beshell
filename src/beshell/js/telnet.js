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

// mqtt
let mqttChannel = null
let mqttClient = null
let mqttTopicIn = null
exportValue(telnet, "mqtt", {
    start(mqtt, topicIn = null, topicOut = null) {
        if(mqttChannel) {
            throw new Error("telnet.mqtt already started")
        }
        if(!topicIn) {
            topicIn = "beshell/repl/in/" + process.readMac("base", true)
        }
        if(!topicOut) {
            topicOut = "beshell/repl/out/" + process.readMac("base", true)
        }

        mqtt.sub(topicIn)

        mqttChannel = new telnet.TelnetChannel()

        mqttChannel.on("output-stream", (data) => {
            mqtt.push(topicOut, data)
        })
        mqtt.on("msg", (msg) => {
            if (msg.topic === topicIn) {
                mqttChannel.process(msg.buffer)
            }
        })
        mqtt.on("error", () => {
            mqttChannel = null
        })
        mqtt.on("close", () => {
            mqttChannel = null
        })

        mqttClient = mqtt
        mqttTopicIn = topicIn
    } ,

    stop() {
        mqttChannel = null
        if(mqttClient) {
            mqttClient.unsub(mqttTopicIn)
            mqttClient = null
        }
    }
})
