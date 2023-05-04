local cjson = require "cjson"
cjson.encode_empty_table_as_array(true)
local vscaux = require "vscaux"

local workdir = ""
local skynet = ""
local config = ""
local service = ""
local open_debug = true
local breakpoints = {}
local envs = {}

local reqfuncs = {}

function reqfuncs.initialize(req)
    vscaux.send_response(req.command, req.seq, {
        supportsConfigurationDoneRequest = true,
        supportsSetVariable = false,
        supportsConditionalBreakpoints = true,
        supportsHitConditionalBreakpoints = true,
    })
    vscaux.send_event("initialized")
    vscaux.send_event("output", {
        category = "console",
        output = "skynet debugger start!\n",
    })
end

local function calc_hitcount(hitexpr)
    if not hitexpr then return 0 end
    
    local f, msg = load("return " .. hitexpr, "=hitexpr")
    if not f then return 0 end
    
    local ok, ret = pcall(f)
    if not ok then return 0 end
    
    return tonumber(ret) or 0
end

function reqfuncs.setBreakpoints(req)
    local args = req.arguments
    local src = args.source.path
    local bpinfos = {}
    local bps = {}
    for _, bp in ipairs(args.breakpoints) do
        local logmsg
        if bp.logMessage and bp.logMessage ~= "" then
            logmsg = bp.logMessage .. '\n'
        end
        bpinfos[#bpinfos+1] = {
            source = {path = src},
            line = bp.line,
            logMessage = logmsg,
            condition = bp.condition,
            hitCount = calc_hitcount(bp.hitCondition),
            currHitCount = 0,
        }
        bps[#bps+1] = {
            verified = true,
            source = {path = src},
            line = bp.line,
        }
    end
    breakpoints[src] = bpinfos
    vscaux.send_response(req.command, req.seq, {
        breakpoints = bps,
    })
end

function reqfuncs.setExceptionBreakpoints(req)
    vscaux.send_response(req.command, req.seq)
end

function reqfuncs.configurationDone(req)
    vscaux.send_response(req.command, req.seq)
end

---@param path string
---@return boolean
local function is_abspath(path)
    return path:byte(1, 1) == ("/"):byte() or path:find("^%a:\\")
end

---@param path string
---@return string
local function prune_path(path)
    local last_byte = path:byte(-1)
	return (last_byte == ("/"):byte() or last_byte == ("\\"):byte()) and path:sub(1, -2) or path
end

local function parse_file_envs(cwd, env_file, env_prefix)
    if not env_file or env_file == "" or not env_prefix or env_prefix == "" then
        return
    end

    local winos = os.getenv("vscdbg_platform") == "windows"

    if not is_abspath(env_file) then
        env_file = cwd .. (winos and "\\" or "/") .. env_file
    end

    local file, errmsg
    if winos then
        file, errmsg = io.popen("@echo off & call " .. env_file .. " & set", 'r')
    else
        file, errmsg = io.popen("source " .. env_file .. "; export", 'r')
    end
    if file == nil then
        error(errmsg)
    end
    
    local reg_list = {
        "^("..env_prefix.."[%w_]+)=\"?(.-)\"?$",
        "^export[%s]+("..env_prefix.."[%w_]+)=\"?(.-)\"?$",
        "^declare -x[%s]+("..env_prefix.."[%w_]+)=\"?(.-)\"?$",
    }

    while true do
        local line = file:read()
        if not line then
            break
        end
        for _, reg in ipairs(reg_list) do
            local _, _, key, value = line:find(reg)
            if key and value then
                table.insert(envs, key)
                table.insert(envs, value)
                break 
            end
        end
    end
    file:close()
end

---@param user_envs string[]|nil
local function parse_user_envs(user_envs)
    if user_envs then
        for _, env in ipairs(user_envs) do
            local _, _, key, value = env:find("^%s*(.-)%s*=%s*(.-)%s*$")
            if key and value then
                table.insert(envs, key)
                table.insert(envs, value)
            end
        end
    end
end

function reqfuncs.launch(req)
	workdir = prune_path(req.arguments.workdir) or "."
    skynet = req.arguments.program
    config = req.arguments.config
	service = req.arguments.service
    open_debug = not req.arguments.noDebug
    parse_file_envs(workdir, req.arguments.envFile, req.arguments.fileEnvPrefix)
    parse_user_envs(req.arguments.envs)

    vscaux.send_response(req.command, req.seq)

    return true
end

function handle_request()
    while true do
        local req = vscaux.recv_request()
        if not req or not req.command then
            return false
        end
        local func = reqfuncs[req.command]
        if func and func(req) then
            break
        elseif not func then
            vscaux.send_error_response(req.command, req.seq, string.format("%s not yet implemented", req.command))
        end
    end
    return true
end

if handle_request() then
    return workdir, skynet, config, service, open_debug, cjson.encode(breakpoints), envs 
else
    error("launch error")
end
