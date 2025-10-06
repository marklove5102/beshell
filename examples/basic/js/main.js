import * as fs from "fs"

let examples = fs.listDirSync("/example")
  .sort((a,b)=>a.localeCompare(b))
  .reduce((lst,filename)=>{
      return lst + `    run /example/${filename}\n`
  },'')


console.log(`
  This is a sample demo for BeShell

* Enter \`ls /example\` to list all examples
* Enter \`run <full example path>\` to run example:
${examples}
* Enter \`reboot\` to back to this menu
* Enter \`help\` or \`?\` to list all commands
* Enter any javascript code to run in interactive mode
`)

console.log('')

console.log(`
  这是一个BeShell的简单例程在串口或命令行中：

* 输入 \`ls /example\` 列出所有例子
* 输入 \`run <full example path>\` 执行例子:
${examples}
* 输入 \`reboot\` 回到此菜单
* 输入 \`help\` 或 \`?\` 列出命令
* 输入 javascript 代码以交互模式运行
`)