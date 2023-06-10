local aio = require("aio.aio")

--- @class gus
--- @field new fun(size: integer): lightuserdata
--- @field free fun(board: lightuserdata)
--- @field place fun(board: lightuserdata, x: integer, y: integer): integer
--- @field encode fun(board: lightuserdata): string
--- @field decode fun(text: string): lightuserdata
gus = gus or {}

local salt = os.getenv("SALT") or "GusAI"

aio:http_post("/go", function (self, query, headers, body)
    local params = aio:parse_query(body)
    local size = tonumber(params.size)
    local x = tonumber(params.x)
    local y = tonumber(params.y)
    local board = nil
    
    if size > 19 then
        self:http_response("400 Bad request", "application/json", { error = "board size is too big" })
        return
    elseif not params.session  then
        self:http_response("400 Bad request", "application/json", { error = "session is missing" })
        return
    end

    if params.session == "new" and size ~= nil then
        board = gus.new(size)
    else
        if codec.hex_encode(crypto.hmac_sha256(params.session, salt)) ~= (params.signature or "xxx") then
            self:http_response("400 Bad request", "application/json", { error = "invalid signature" })
            return
        end
        board = gus.decode(params.session)
    end
    if not board then
        self:http_response("500 Internal server error", "application/json", {error = "failed to allocate board"})
        return
    end
    local status = 0
    if x ~= nil and y ~= nil then
        status = gus.place(board, x, y)
    end 
    local encoded = gus.encode(board)
    self:http_response("200 OK", "application/json", {
        session = encoded,
        signature = codec.hex_encode(crypto.hmac_sha256(encoded, salt)),
        status = status
    })
    gus.free(board)
end)