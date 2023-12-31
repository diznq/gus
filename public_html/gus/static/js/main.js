const BLACK = 0;
const WHITE = 1;
const EMPTY = 2;


const   NO_ERR = 0,
        ERR_PLACED = -1,
        ERR_OOB = -2,
        ERR_SUICIDE = -3,
        ERR_KO = -4,
        ERR_PASS = -5;

class Game {
    /**
     * Initialize game
     * @param {HTMLDivElement} parent 
     * @param {Number} size
     */
    constructor(parent, score, pass, size) {
        this.session = "new";
        this.signature = "";
        this.parent = parent;
        this.size = size;
        this.score = score;
        this.turn = BLACK;
        this.cells = [];
        this.state = [];
        this.parent.innerHTML = "";

        this.score.innerHTML = `<div class="ai-pass" id="ai_pass">AI passes</div><div class="black">Black: <span id="black_score">0</span></div><div class="white">White: <span id="white_score">6.5</span></div>`

        for(let y = 0; y < size; y++) {
            const row = document.createElement("div");
            row.className = "row";
            for(let x = 0; x < size; x++) {
                const cell = document.createElement("div");
                const inner = document.createElement("div");
                cell.className = "cell";
                inner.classList = "body";
                cell.appendChild(inner);
                cell.body = inner;
                row.appendChild(cell);
                cell.onclick = (event) => {
                    this.handleClick(cell, event, x, y)
                }
                this.cells.push(cell)
                this.state.push(EMPTY);
            }
            parent.appendChild(row);
        }
        this.refreshState(-1, -1);

        pass.onclick = () => {
            this.refreshState(-2, -2)
        }
    }

    /**
     * Handle board click
     * @param {HTMLDivElement} cell 
     * @param {MouseEvent} event 
     * @param {Number} x 
     * @param {Number} y 
     */
    handleClick(cell, event, x, y) {
        const state = this.$(x, y);
        this.refreshState(x, y, this.turn);
    }

    /**
     * Propagate group ID to neighouring cells of same color
     * @param {{[Number]: Number}} groups group lookup table
     * @param {{[Number]: Array<Number>}} liberties liberties lookup table
     * @param {Number} x current cell X
     * @param {Number} y current cell Y
     * @param {Number} group current group ID
     * @param {Number} color current color
     */
    propagate(groups, liberties, x, y, group, color) {
        const state = this.$(x, y)
        if(state == color && groups[y * this.size + x] == 0) {
            groups[y * this.size + x] = group;
            liberties[group] = liberties[group] || [];
            liberties[group].push(... this.getLiberties(x, y))
            this.cells[y * this.size + x].setAttribute("data-group", group)
            this.propagate(groups, liberties, x - 1, y, group, state)
            this.propagate(groups, liberties, x + 1, y, group, state)
            this.propagate(groups, liberties, x, y - 1, group, state)
            this.propagate(groups, liberties, x, y + 1, group, state)
        }
    }

    /**
     * Refresh liberties on current state
     * @returns number of stones to be removed
     */
    refreshLiberties() {
        let groups = []
        let liberties = {}
        let id = 1;
        let removed = [];
        for(let i = 0; i < this.size * this.size; i++) groups.push(0);
        for(let y = 0; y < this.size; y++) {
            for(let x = 0; x < this.size; x++) {
                let state = this.$(x, y)
                let group = groups[y * this.size + x];
                if(group > 0) continue;
                if(state != EMPTY) {
                    group = id++;
                    this.propagate(groups, liberties, x, y, group, state)
                } else {
                    groups[y * this.size + x] = 0;
                    this.cells[y * this.size + x].setAttribute("data-group", "-")
                }
            }
        }

        for(let i = 0; i < groups.length; i++) {
            const group = groups[i]
            if(group == 0) {
                this.cells[i].setAttribute("data-liberties", "")
            } else {
                liberties[group] = [... new Set(liberties[group])]
                this.cells[i].setAttribute("data-liberties", liberties[group].length)
                if(liberties[group].length == 0) {
                    removed.push(i)
                }
            }
        }

        return removed;
    }

    /**
     * Get list of surrounding liberties for cell (stone) inside group at given position
     * @param {Number} x current cell X
     * @param {Number} y current cell Y
     * @returns 
     */
    getLiberties(x, y) {
        let liberties = []
        const check = [ [x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1] ]
        for(let i = 0; i < check.length; i++) {
            let X = check[i][0]
            let Y = check[i][1]
            if(X < 0 || Y < 0 || X >= this.size || Y >= this.size) continue;
            let state = this.$(X, Y)
            if(state == EMPTY) {
                liberties.push(Y * this.size + X)
            }
        }
        return liberties;
    }

    /**
     * Refresh game state
     * @param {Number} x move to be performed 
     * @param {Number} y move to be performed
     */
    refreshState(x, y) {
        const xhr = new XMLHttpRequest();
        xhr.onload = () => {
            const response = xhr.response;
            this.session = response.session;
            this.signature = response.signature;
            const params = this.session.split(" ")
            const blackScore = parseFloat(params[3]) / 10
            const whiteScore = parseFloat(params[4]) / 10
            this.parent.querySelectorAll(".highlighted").forEach(a => a.className = a.className.replace("highlighted", ""))
            this.state = params[5].split("").map(a => {
                if(a == "+") return EMPTY;
                else if(a == "X") return BLACK;
                else return WHITE;
            })

            document.getElementById("white_score").textContent = whiteScore.toFixed(1);
            document.getElementById("black_score").textContent = blackScore.toFixed(1);

            this.refreshLiberties();
            
            for(let i = 0; i < this.state.length; i++) {
                const cell = this.state[i];
                if(cell == EMPTY) {
                    this.cells[i].className = "cell empty";
                } else if(cell == WHITE) {
                    this.cells[i].className = "cell white";
                } else if(cell == BLACK) {
                    this.cells[i].className = "cell black";
                }
                if(i == response.y * this.size + response.x) {
                    this.cells[i].className += " highlighted";
                }
                this.cells[i].body.textContent = (this.cells[i].getAttribute("data-liberties") || "");
            }

            document.getElementById("ai_pass").className = "ai-pass";

            if(response.status < 0) {
                switch(response.status) {
                    case ERR_SUICIDE:
                        alert("Suicide is forbidden!");
                        break;
                    case ERR_KO:
                        alert("Move leads to Ko!")
                        break;
                    case ERR_PASS:
                        document.getElementById("ai_pass").className = "ai-pass ai-pass-true";
                        break;
                }
            }

            return true;
        }

        xhr.responseType = "json";
        xhr.open("POST", "/gus/go");
        xhr.setRequestHeader("Content-type", "applicaton/www-form-urlencoded")
        xhr.send("x=" + x + "&y=" + y + "&session=" + encodeURIComponent(this.session) + "&signature=" + this.signature + "&size=" + this.size);
    }

    /**
     * Get or set field at given position
     * @param {Number} x 
     * @param {Number} y 
     * @param {?Number} value
     * @returns 
     */
    $(x, y, value) {
        if(x < 0 || x >= this.size || y < 0 || y >= this.size) return EMPTY;
        if(typeof(value) != "undefined") {
            this.state[y * this.size + x] = value;
            return value;
        }
        return this.state[y * this.size + x];
    }
}