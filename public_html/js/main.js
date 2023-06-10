const BLACK = 1;
const WHITE = 2;
const KO = 4;
const EMPTY = 8;
const KO_BLACK = BLACK | KO;
const KO_WHITE = WHITE | KO;

class Game {
    /**
     * Initialize game
     * @param {HTMLDivElement} parent 
     * @param {Number} size
     */
    constructor(parent, size) {
        this.parent = parent;
        this.size = size;
        this.komi = 6.5;
        this.turn = BLACK;
        this.cells = [];
        this.state = [];
        this.parent.innerHTML = "";
        for(let y = 0; y < size; y++) {
            const row = document.createElement("div");
            row.className = "row";
            for(let x = 0; x < size; x++) {
                const cell = document.createElement("div");
                cell.className = "cell";
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
        if(state & EMPTY) {
            this.refreshState(x, y, this.turn);
        } else if(state & KO) {
            // handle ko
        }
    }

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
                if((state & (EMPTY | KO)) == 0) {
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
                this.cells[i].setAttribute("data-liberties", "-")
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

    getLiberties(x, y) {
        let liberties = []
        const check = [ [x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1] ]
        for(let i = 0; i < check.length; i++) {
            let X = check[i][0]
            let Y = check[i][1]
            if(X < 0 || Y < 0 || X >= this.size || Y >= this.size) continue;
            let state = this.$(X, Y)
            if(state & (KO | EMPTY)) {
                liberties.push(Y * this.size + X)
            }
        }
        return liberties;
    }

    refreshState(x, y, turn) {
        if(typeof(turn) != "undefined") {
            this.$(x, y, turn)
            this.turn = turn == BLACK ? WHITE : BLACK;
        }
        let removed = this.refreshLiberties()
        let suicide = false;

        if(removed.length > 0 && x >= 0 && y >= 0) {
            if(removed.length == 1 && removed[0] == (y * this.size + x)) {
                suicide = true;
            } else {
                removed = removed.filter(a => a != (y * this.size + x));
            }
            
            if(suicide) {
                // suicide is not allowed, revert back
                this.state[y * this.size + x] = EMPTY;
                this.turn = this.turn == BLACK ? WHITE : BLACK;
                return false;
            }
        }

        for(let i = 0; i < removed.length; i++) this.state[removed[i]] = EMPTY;
        removed = this.refreshLiberties()
        for(let i = 0; i < removed.length; i++) this.state[removed[i]] = EMPTY;
        
        for(let i = 0; i < this.state.length; i++) {
            const cell = this.state[i];
            if(cell & (EMPTY | KO)) {
                this.cells[i].className = "cell empty";
            } else if(cell & WHITE) {
                this.cells[i].className = "cell white";
            } else if(cell & BLACK) {
                this.cells[i].className = "cell black";
            }
            this.cells[i].textContent = (this.cells[i].getAttribute("data-liberties") || "-");
        }

        return true;
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