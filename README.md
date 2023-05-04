Skynet 调试器

## 构建skynet

请使用以下skynet版本。skynet的windows版请参考[https://github.com/firedtoad/skynet-mingw](https://github.com/firedtoad/skynet-mingw) 。同时需要解决io.stdin的select问题方可使用该调试插件。

[https://github.com/socoding/skynet](https://github.com/socoding/skynet)

## 安装扩展

在VSCode的`Extensions`面板中搜索`Skynet Debug`安装这个插件。

## 配置launch.json

插件安装完毕之后，用VSCode打开skynet工程，在`Run and Debug`面板中创建一个`launch.json`文件，内容如下：

```json
{
	"name": "skynet debug",
	"type": "lua",
	"request": "launch",
	"workdir": "${workspaceFolder}",
	"program": "./skynet",
	"config": "./examples/config_vsc",
	"service": "./service;./preinit",
	"envFile": "./env_game.sh",
	"fileEnvPrefix": "game_",
	"envs": [],
}
```

- `workdir` 程序工作目录，默认为 VSCode 打开的这个目录。
- `program` skynet可执行程序路径，相对于workdir；也可以用绝对路径。
- `config`  skynet运行的配置文件，相对于workdir；也可以用绝对路径。
- `service` 禁用调试的服务路径，相对于workdir。
- `envFile` 自定义环境变量的文件路径，相对于workdir；也可以用绝对路径。
- `fileEnvPrefix` 自定义环境变量的前缀规则，仅对envFile中的环境变量生效。
- `envs` 自定义环境变量，不受fileEnvPrefix影响。

config_vsc文件的内容如下：

```lua
root = "./"
thread = 4
logger = "vscdebuglog" -- 将默认logger指定为`vscdebuglog`
logservice = "snlua"

logpath = "."
harbor = 0
start = "testvscdebug"	-- main script
bootstrap = "snlua bootstrap"	-- The service for bootstrap

luaservice = root.."service/?.lua;"..root.."test/?.lua;"..root.."examples/?.lua;"..root.."test/?/init.lua"
lualoader = root .. "lualib/loader.lua"
lua_path = root.."lualib/?.lua;"..root.."lualib/?/init.lua"
lua_cpath = root .. "luaclib/?.so"
snax = root.."examples/?.lua;"..root.."test/?.lua"
cpath = root.."cservice/?.so"
```

- 指定 logger 为 vscdebuglog，这样在 VSCode 的 `DEBUG CONSOLE` 才能看到日志输出。
- root 为skynet的根目录，这个目录是相对于上面 workdir 的。
- 这一份配置一般用于开发期的调试使用，发布的版本要用正式的config。

## 开始调试

到这里准备工作已经完毕，你可以在代码(比如：testvscdebug)中设置断点，然后按`F5`开始调试，效果如下图所示：

![sn1.png](vscext/images/sn1.png)

如果F5之后没有成功调试，你可以CD到插件目录，比如：

- `~/.vscode-server/extensions/socoding.skynet-debug-x.x.x/bin/linux/` 或
- `~/.vscode/extensions/socoding.skynet-debug-x.x.x/bin/macosx/`或
- `~/.vscode/extensions/socoding.skynet-debug-x.x.x/bin/windows/`

里面有一个debug.log文件，查看里面的文件，查找错误原因。

## vscdebug的功能

vscdebug实现了大多数常用的调试功能：

- 它可以将skynet.error输出到`DEBUG CONSOLE`面板，点击日志还可跳转到相应的代码行。
- 除了可以设置普通断点外，还支持以下几种断点：
    - 条件断点：当表达式为true时停下来。
    - Hit Count断点：命中一定次数后停下来。
    - 日志断点：命中时输出日志。
- 当程序命中断点后停了下来，你就可以：
    - 查看调用堆栈。
    - 查看每一层栈帧的局部变量，自由变量。
    - 通过`WATCH`面板增加监控的表达式。
    - 可在`DEBUG CONSOLE`底部输入表达式，该表达式会在当前栈帧环境中执行，并得到结果输出。
- 支持`Step into`, `Step over`, `Step out`, `Continue`等调试命令。
