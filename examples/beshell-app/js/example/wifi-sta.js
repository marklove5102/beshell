import * as wifi from "wifi"

const ssid = "your SSID"
const pwd = "your PASSWORD"

async function main(){

    console.log("\n")
    console.log("Connecting to AP", ssid, ", PWD:", pwd, "...")
    
    // connect to wifi hotspot
    if( ! await wifi.connect(ssid, pwd) ){
        console.log("Failed to connect to AP")
        return
    }

    console.log("Connected to AP\n")

    // wait for getting IP address
    console.log("Getting IP address...")
    let status = await wifi.waitIP()
    if( !status ){
        console.log("got ip failed")
    } else {
        console.log("STA IP:", status.ip)
    }
    
}

main()