import * as wifi from "wifi"


// authmode
const mapAuthModeName = [
    "open", "wep", "wpa-psk", "wpa2-psk", "wpa-wpa2-psk", "enterprise", "enterprise",
    "wpa3-psk", "wpa2-wpa3-psk", "wapi-psk", "owe", "wpa3-ent-192", "wpa3-ext-psk",
    "wpa3-ext-psk-mixed-mode", "dpp", "wpa3-enterprise", "wpa2-wpa3-enterprise"
]

async function main(){

    wifi.start()

    console.log("\n\n")
    console.log("Scanning WiFi APs ...")

    let lst = await wifi.scan()
    if(!lst){
        console.log("WiFi scan failed")
        return
    }

    lst
        // 按信号强度排序
        // sort by rssi
        .sort((a, b) => b.rssi - a.rssi)

        // 跳过隐藏的 SSID
        // skip hidden SSIDs
        .filter((ap)=>!!ap.ssid)

        // 输出找到的 AP 列表
        // print the found APs
        .forEach((ap)=>{
            console.log(`SSID:  ${ap.ssid||"[hidden name]"},  RSSI: ${ap.rssi},  AUTH: ${mapAuthModeName[ap.authmode]||("unknown:"+ap.authmode)}`)
        })
    
    console.log(`\nFound ${lst.length} APs\n`)
}

main()