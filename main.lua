local aio = require("aio.aio")

MAX_BOARD = MAX_BOARD or 13

--- @class gus
--- @field new fun(size: integer): lightuserdata
--- @field free fun(board: lightuserdata)
--- @field place fun(board: lightuserdata, x: integer, y: integer, predict: boolean, pass: boolean): integer, integer, integer
--- @field encode fun(board: lightuserdata): string
--- @field decode fun(text: string): lightuserdata
gus = gus or {}

local salt = os.getenv("SALT") or "GusAI"

aio:set_max_cache_size(100000)

aio:http_post("/gus/go", function (self, query, headers, body)
    local params = aio:parse_query(body)
    local size = tonumber(params.size)
    local x = tonumber(params.x)
    local y = tonumber(params.y)
    local board = nil
    local predict = false
    local pass = false
    
    if size == nil or size > MAX_BOARD then
        self:http_response("400 Bad request", "application/json", { error = "board size is too big" })
        return
    elseif not params.session  then
        self:http_response("400 Bad request", "application/json", { error = "session is missing" })
        return
    elseif not x or not y then
        self:http_response("400 Bad request", "application/json", { error = "invalid move" })
        return
    end

    local key = string.format("%s:%s:%d:%d", params.session, params.signature, params.x, params.y)

    local result = aio:cached("go", key, function ()
        if params.session == "new" and size ~= nil then
            board = gus.new(size)
        else
            if codec.hex_encode(crypto.hmac_sha256(params.session, salt)) ~= (params.signature or "xxx") then
                return { error = "invalid signature", http_status = "400 Bad request" }
            end
            board = gus.decode(params.session)
            predict = true
        end
        if not board then
            return nil
        end
        if x == -2 then pass = true end
        local status = 0
        if x ~= nil and y ~= nil then
            status, x, y = gus.place(board, x, y, predict, pass)
        end 
        local encoded = gus.encode(board)
        local response = {
            http_status = "200 OK",
            session = encoded,
            signature = codec.hex_encode(crypto.hmac_sha256(encoded, salt)),
            status = status,
            x = x,
            y = y
        }
        gus.free(board)
        return response
    end)

    if not result then
        self:http_response("500 Internal server error", "application/json", {error = "oom"})
        return
    end
    local status = result.http_status
    self:http_response(status, "application/json", result)
end)