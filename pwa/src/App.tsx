import { useEffect, useMemo, useState } from "react";

// 0=empty, 1=light man, 2=light king, 3=dark man, 4=dark king
type Piece = 0 | 1 | 2 | 3 | 4;
type Player = 0 | 1 | 2; // 0=none, 1=light, 2=dark
type Board = Piece[][];
type Variant = "english" | "international";
type Difficulty = "easy" | "medium" | "hard";
type Mode = "light" | "dark" | "two" | "demo";

interface DraughtsMove { fx: number; fy: number; tx: number; ty: number; cx: number; cy: number; isCapture: boolean; }

interface GameState {
  board: Board;
  turn: 1 | 2;
  winner: Player;
  size: number;
  variant: Variant;
  redCount: number;   // light count
  blackCount: number; // dark count
  mustContinue: boolean;
  forcedX: number;
  forcedY: number;
  moves: string[];
  history: { board: Board; turn: 1 | 2; winner: Player; mustContinue: boolean; forcedX: number; forcedY: number }[];
}

interface PersistedState { game: GameState; savedGame: GameState | null; difficulty: Difficulty; mode: Mode; }

const STORAGE_KEY = "exact-draughts-pwa-v1";
const DIRS: [number, number][] = [[-1,-1],[1,-1],[-1,1],[1,1]];

function pieceOwner(p: Piece): Player { if (p===1||p===2) return 1; if (p===3||p===4) return 2; return 0; }
function isKing(p: Piece): boolean { return p===2||p===4; }
function belongsTo(p: Piece, pl: Player): boolean { return pieceOwner(p)===pl; }
function otherPlayer(pl: 1|2): 1|2 { return pl===1?2:1; }
function isDarkSquare(x: number, y: number): boolean { return ((x+y)&1)===1; }

function directionAllowed(p: Piece, dy: number): boolean {
  if (isKing(p)) return true;
  if (p===1) return dy < 0;
  if (p===3) return dy > 0;
  return false;
}
function captureDirAllowed(variant: Variant, p: Piece, dy: number): boolean {
  if (variant==="international") return true;
  return directionAllowed(p, dy);
}
function inBounds(size: number, x: number, y: number): boolean { return x>=0&&x<size&&y>=0&&y<size; }
function cloneBoard(b: Board): Board { return b.map(r=>[...r] as Piece[]); }

function newGame(variant: Variant = "english"): GameState {
  const size = variant==="international"?10:8;
  const rows = variant==="international"?4:3;
  const board: Board = Array.from({length:size}, ()=>Array(size).fill(0));
  for (let y=0;y<rows;y++) for (let x=0;x<size;x++) if (isDarkSquare(x,y)) board[y][x]=3;
  for (let y=size-rows;y<size;y++) for (let x=0;x<size;x++) if (isDarkSquare(x,y)) board[y][x]=1;
  return {
    board, turn:1, winner:0, size, variant,
    redCount: variant==="international"?20:12, blackCount: variant==="international"?20:12,
    mustContinue:false, forcedX:-1, forcedY:-1, moves:[],
    history: [{board:cloneBoard(board),turn:1,winner:0,mustContinue:false,forcedX:-1,forcedY:-1}]
  };
}

function updateCounts(board: Board, size: number): {redCount:number;blackCount:number} {
  let r=0,b=0;
  for(let y=0;y<size;y++) for(let x=0;x<size;x++){
    if(belongsTo(board[y][x],1))r++; else if(belongsTo(board[y][x],2))b++;
  }
  return {redCount:r,blackCount:b};
}

function collectCapturesForPiece(g: GameState, x: number, y: number): DraughtsMove[] {
  const piece = g.board[y][x];
  const player = pieceOwner(piece);
  const opp = otherPlayer(player as 1|2);
  const moves: DraughtsMove[] = [];
  if (piece===0) return moves;
  for (const [dx,dy] of DIRS) {
    if (!captureDirAllowed(g.variant, piece, dy)) continue;
    if (g.variant==="international" && isKing(piece)) {
      let sx=x+dx, sy=y+dy, cx=-1, cy=-1;
      while (inBounds(g.size,sx,sy)) {
        const seen = g.board[sy][sx];
        if (seen===0) { if (cx!==-1) moves.push({fx:x,fy:y,tx:sx,ty:sy,cx,cy,isCapture:true}); }
        else if (belongsTo(seen,player as 1|2)) break;
        else { if (cx!==-1) break; cx=sx; cy=sy; }
        sx+=dx; sy+=dy;
      }
    } else {
      const mx=x+dx,my=y+dy,tx=x+dx*2,ty=y+dy*2;
      if (!inBounds(g.size,tx,ty)||g.board[ty][tx]!==0) continue;
      if (!belongsTo(g.board[my][mx],opp)) continue;
      moves.push({fx:x,fy:y,tx,ty,cx:mx,cy:my,isCapture:true});
    }
  }
  return moves;
}

function hasAnyCapture(g: GameState, player: Player): boolean {
  for (let y=0;y<g.size;y++) for (let x=0;x<g.size;x++)
    if (belongsTo(g.board[y][x],player) && collectCapturesForPiece(g,x,y).length>0) return true;
  return false;
}

function collectMovesForPiece(g: GameState, x: number, y: number): DraughtsMove[] {
  const piece = g.board[y][x];
  if (!belongsTo(piece, g.turn)) return [];
  if (g.mustContinue && (x!==g.forcedX||y!==g.forcedY)) return [];
  const captures = collectCapturesForPiece(g, x, y);
  if (captures.length>0||g.mustContinue) return captures;
  if (hasAnyCapture(g, g.turn)) return [];
  const moves: DraughtsMove[] = [];
  for (const [dx,dy] of DIRS) {
    if (!directionAllowed(piece, dy)) continue;
    const tx=x+dx, ty=y+dy;
    if (!inBounds(g.size,tx,ty)||g.board[ty][tx]!==0) continue;
    moves.push({fx:x,fy:y,tx,ty,cx:-1,cy:-1,isCapture:false});
    if (g.variant==="international" && isKing(piece)) {
      let tx2=tx+dx, ty2=ty+dy;
      while (inBounds(g.size,tx2,ty2)&&g.board[ty2][tx2]===0) {
        moves.push({fx:x,fy:y,tx:tx2,ty:ty2,cx:-1,cy:-1,isCapture:false});
        tx2+=dx; ty2+=dy;
      }
    }
  }
  return moves;
}

function collectAllLegalMoves(g: GameState): DraughtsMove[] {
  if (g.winner!==0) return [];
  const moves: DraughtsMove[] = [];
  for (let y=0;y<g.size;y++) for (let x=0;x<g.size;x++)
    if (belongsTo(g.board[y][x],g.turn)) moves.push(...collectMovesForPiece(g,x,y));
  return moves;
}

function isLegalMove(g: GameState, m: DraughtsMove): boolean {
  const avail = collectMovesForPiece(g, m.fx, m.fy);
  return avail.some(a => a.tx===m.tx && a.ty===m.ty);
}

function promoteIfNeeded(board: Board, x: number, y: number, size: number) {
  if (board[y][x]===1 && y===0) board[y][x]=2;
  else if (board[y][x]===3 && y===size-1) board[y][x]=4;
}

type MoveResult = "invalid"|"running"|"continue"|"over";

function applyMove(g: GameState, m: DraughtsMove): {next: GameState; result: MoveResult} {
  if (g.winner!==0 || !isLegalMove(g,m)) return {next:g, result:"invalid"};
  const board = cloneBoard(g.board);
  const piece = board[m.fy][m.fx];
  board[m.fy][m.fx]=0; board[m.ty][m.tx]=piece;
  if (m.isCapture) board[m.cy][m.cx]=0;
  promoteIfNeeded(board,m.tx,m.ty,g.size);
  const {redCount,blackCount}=updateCounts(board,g.size);
  const label=`${"abcdefghij"[m.fx]}${g.size-m.fy}-${"abcdefghij"[m.tx]}${g.size-m.ty}${m.isCapture?"x":""}`;
  let mustContinue=false, forcedX=-1, forcedY=-1, result: MoveResult="running";
  let winner: Player=0, nextTurn: 1|2=g.turn;
  if (m.isCapture) {
    const tmpG: GameState={...g,board,turn:g.turn,mustContinue:false,forcedX:-1,forcedY:-1,winner:0,redCount,blackCount};
    const followups = collectCapturesForPiece(tmpG,m.tx,m.ty);
    if (followups.length>0) { mustContinue=true; forcedX=m.tx; forcedY=m.ty; result="continue"; }
  }
  if (!mustContinue) {
    if (redCount===0) { winner=2; result="over"; }
    else if (blackCount===0) { winner=1; result="over"; }
    else {
      nextTurn=otherPlayer(g.turn);
      const tmpG2: GameState={...g,board,turn:nextTurn,winner:0,redCount,blackCount,mustContinue:false,forcedX:-1,forcedY:-1};
      if (collectAllLegalMoves(tmpG2).length===0) { winner=g.turn; result="over"; }
    }
  }
  const next: GameState = {
    board, turn:nextTurn, winner, size:g.size, variant:g.variant,
    redCount, blackCount, mustContinue, forcedX, forcedY,
    moves:[...g.moves, label],
    history:[...g.history, {board:cloneBoard(board),turn:nextTurn,winner,mustContinue,forcedX,forcedY}]
  };
  return {next, result};
}

function undoMove(g: GameState): GameState {
  if (g.history.length<=1) return g;
  const history = g.history.slice(0,-1);
  const prev = history[history.length-1];
  const {redCount,blackCount}=updateCounts(prev.board,g.size);
  return {
    board:cloneBoard(prev.board), turn:prev.turn, winner:prev.winner,
    size:g.size, variant:g.variant, redCount, blackCount,
    mustContinue:prev.mustContinue, forcedX:prev.forcedX, forcedY:prev.forcedY,
    moves:g.moves.slice(0,-1), history
  };
}

function pieceValue(p: Piece): number { if (p===2||p===4) return 500; if (p===1||p===3) return 300; return 0; }

function evaluatePosition(g: GameState, player: 1|2): number {
  if (g.winner===player) return 100000;
  if (g.winner===otherPlayer(player)) return -100000;
  let score=0;
  for (let y=0;y<g.size;y++) for (let x=0;x<g.size;x++) {
    const p=g.board[y][x]; const owner=pieceOwner(p);
    if (!owner) continue;
    let val=pieceValue(p);
    if (!isKing(p)) val+=p===1?(g.size-1-y)*18:y*18;
    if (x>0&&x<g.size-1) val+=8;
    score += owner===player ? val : -val;
  }
  return score;
}

function searchPosition(g: GameState, player: 1|2, depth: number, alpha: number, beta: number): number {
  if (depth<=0||g.winner!==0) return evaluatePosition(g,player);
  const moves=collectAllLegalMoves(g);
  if (!moves.length) return evaluatePosition(g,player);
  if (g.turn===player) {
    let best=-Infinity;
    for (const m of moves) {
      const {next}=applyMove(g,m);
      const s=searchPosition(next,player,depth-1,alpha,beta);
      if (s>best) best=s; if (best>alpha) alpha=best; if (alpha>=beta) break;
    }
    return best;
  } else {
    let best=Infinity;
    for (const m of moves) {
      const {next}=applyMove(g,m);
      const s=searchPosition(next,player,depth-1,alpha,beta);
      if (s<best) best=s; if (best<beta) beta=best; if (alpha>=beta) break;
    }
    return best;
  }
}

function aiPickMove(g: GameState, difficulty: Difficulty): DraughtsMove|null {
  const moves=collectAllLegalMoves(g);
  if (!moves.length) return null;
  const depth=difficulty==="easy"?2:difficulty==="medium"?4:g.variant==="international"?5:6;
  const player=g.turn;
  let best=-Infinity, bestMove=moves[0];
  for (const m of moves) {
    const {next}=applyMove(g,m);
    const s=searchPosition(next,player,depth-1,-1000000,1000000);
    if (s>best) { best=s; bestMove=m; }
  }
  return bestMove;
}

function playerName(p: 1|2): string { return p===1?"Light":"Dark"; }

function gameStatus(g: GameState, thinking: boolean, mode: Mode): string {
  if (g.winner!==0) return `${playerName(g.winner)} wins!`;
  if (g.mustContinue) return `${playerName(g.turn)}: continue capture.`;
  if (thinking) return "Opponent thinking…";
  if (mode==="demo") return "AI demo running.";
  return `${playerName(g.turn)} to move. L:${g.redCount} D:${g.blackCount}`;
}

function loadState(): PersistedState {
  const fallback: PersistedState = { game:newGame(), savedGame:null, difficulty:"medium", mode:"light" };
  try {
    const r=localStorage.getItem(STORAGE_KEY);
    if (!r) return fallback;
    const parsed = JSON.parse(r) as Partial<PersistedState>;
    // migrate old "red" mode → "light"
    const rawMode = parsed.mode as string;
    const mode: Mode = rawMode === "red" ? "light" : (rawMode as Mode) || fallback.mode;
    return { ...fallback, ...parsed, mode };
  } catch { return fallback; }
}

export default function App() {
  const initial = useMemo(loadState, []);
  const [game, setGame] = useState<GameState>(initial.game);
  const [savedGame, setSavedGame] = useState<GameState|null>(initial.savedGame);
  const [difficulty, setDifficulty] = useState<Difficulty>(initial.difficulty);
  const [mode, setMode] = useState<Mode>(initial.mode);
  const [thinking, setThinking] = useState(false);
  const [selected, setSelected] = useState<[number,number]|null>(null);
  const [message, setMessage] = useState("");
  const [page, setPage] = useState<"game"|"about">("game");
  const [showHistory, setShowHistory] = useState(true);
  const [reviewIdx, setReviewIdx] = useState<number | null>(null);

  const totalMoves = game.history.length - 1;
  const isReviewing = reviewIdx !== null;
  const displayIdx = reviewIdx ?? totalMoves;
  const displaySnap = isReviewing
    ? game.history[Math.min(reviewIdx!, game.history.length - 1)]
    : { board: game.board, turn: game.turn, winner: game.winner, mustContinue: game.mustContinue, forcedX: game.forcedX, forcedY: game.forcedY };
  const displayBoard = displaySnap.board;

  const legalMoves = useMemo(()=>collectAllLegalMoves(game), [game]);
  const selectedMoves = useMemo(()=>{
    if (!selected || isReviewing) return [];
    return collectMovesForPiece(game, selected[0], selected[1]);
  }, [game, selected, isReviewing]);
  const targets = useMemo(()=>new Set(selectedMoves.map(m=>`${m.tx},${m.ty}`)), [selectedMoves]);
  const selectables = useMemo(()=>{
    if (isReviewing) return new Set<string>();
    const s=new Set<string>();
    for (const m of legalMoves) s.add(`${m.fx},${m.fy}`);
    return s;
  }, [legalMoves, isReviewing]);

  const isHumanTurn = game.winner===0 && !isReviewing && (
    mode==="two" || (mode==="light" && game.turn===1) || (mode==="dark" && game.turn===2)
  );

  function goReview(idx: number | null) { setReviewIdx(idx); if (idx !== null) setSelected(null); }

  useEffect(()=>{
    localStorage.setItem(STORAGE_KEY,JSON.stringify({game,savedGame,difficulty,mode}));
  },[game,savedGame,difficulty,mode]);

  useEffect(()=>{
    if (game.winner!==0) { setThinking(false); return; }
    const shouldAI = mode==="demo" ||
      (mode==="light" && game.turn===2) ||
      (mode==="dark" && game.turn===1);
    if (!shouldAI) return;
    setThinking(true);
    const id=window.setTimeout(()=>{
      const m=aiPickMove(game,difficulty);
      if (!m) { setThinking(false); return; }
      const {next}=applyMove(game,m);
      setGame(next); setSelected(null);
      setMessage(`${playerName(game.turn)}: ${"abcdefghij"[m.fx]}${game.size-m.fy}→${"abcdefghij"[m.tx]}${game.size-m.ty}`);
      setThinking(false);
    }, mode==="demo"?500:650);
    return ()=>clearTimeout(id);
  },[game,mode,difficulty]);

  function tapSquare(x: number, y: number) {
    if (!isHumanTurn||thinking||game.winner!==0) return;
    if (targets.has(`${x},${y}`) && selected) {
      const move=selectedMoves.find(m=>m.tx===x&&m.ty===y)!;
      const {next}=applyMove(game,move);
      setGame(next); setSelected(null); setReviewIdx(null);
      setMessage(`${playerName(game.turn)}: ${"abcdefghij"[move.fx]}${game.size-move.fy}→${"abcdefghij"[move.tx]}${game.size-move.ty}`);
      return;
    }
    if (selectables.has(`${x},${y}`)) { setSelected([x,y]); setMessage(`Selected ${"abcdefghij"[x]}${game.size-y}`); return; }
    setSelected(null);
  }

  function handleNew(v?: Variant) {
    const variant=v||game.variant;
    setGame(newGame(variant)); setSelected(null); setMessage("New game."); setThinking(false); setReviewIdx(null);
  }
  function handleUndo() {
    let g=undoMove(game);
    if (mode!=="two" && g.history.length>1) {
      const humanPlayer: Player = mode==="light" ? 1 : 2;
      if (g.turn !== humanPlayer) g=undoMove(g);
    }
    setGame(g); setSelected(null); setMessage("Undone."); setThinking(false); setReviewIdx(null);
  }
  function handleSave() { setSavedGame(game); setMessage("Saved."); }
  function handleLoad() {
    if (!savedGame) { setMessage("No saved game."); return; }
    setGame(savedGame); setSelected(null); setThinking(false); setMessage("Loaded."); setReviewIdx(null);
  }

  const size=game.size;

  return (
    <main className="app">
      <header className="hero">
        <h1>Exact Draughts</h1>
        <p>{message||gameStatus(game,thinking,mode)}</p>
      </header>

      <section className="toolbar" aria-label="Game controls">
        <button onClick={()=>handleNew()}>New</button>
        <button onClick={handleUndo} disabled={game.history.length<=1||thinking}>Undo</button>
        <button onClick={handleSave}>Save</button>
        <button onClick={handleLoad}>Load</button>
        <button onClick={()=>handleNew(game.variant==="english"?"international":"english")}>
          {game.variant==="english"?"→Intl":"→Eng"}
        </button>
        <button onClick={()=>setShowHistory(v=>!v)} aria-pressed={showHistory}>
          {showHistory ? "Hide Moves" : "Show Moves"}
        </button>
        <button onClick={()=>setPage(p=>p==="game"?"about":"game")}>{page==="game"?"About":"Game"}</button>
      </section>

      <section className="settings" aria-label="Settings">
        <label>Mode
          <select value={mode} onChange={e=>{setMode(e.target.value as Mode);setThinking(false);}}>
            <option value="light">Play Light</option>
            <option value="dark">Play Dark</option>
            <option value="two">2 Player</option>
            <option value="demo">AI Demo</option>
          </select>
        </label>
        <label>Level
          <select value={difficulty} onChange={e=>setDifficulty(e.target.value as Difficulty)}>
            <option value="easy">Easy</option>
            <option value="medium">Medium</option>
            <option value="hard">Hard</option>
          </select>
        </label>
        <label>Variant
          <select value={game.variant} onChange={e=>handleNew(e.target.value as Variant)}>
            <option value="english">English (8×8)</option>
            <option value="international">International (10×10)</option>
          </select>
        </label>
      </section>

      {page==="about" ? (
        <section className="about-page">
          <h2>About Exact Draughts</h2>
          <p>An installable browser port of Exact Draughts. Rules engine ported from the native Kindle draughts implementation. Supports English (8×8) and International (10×10) variants.</p>
          <p>Light pieces vs Dark pieces. Light moves up, Dark moves down. Captures are mandatory. Kings can move and capture in all directions.</p>
          <p>Attribution: Exact Draughts lineage from the GNOME Games KUAL porting project. License: GPL-3.0-or-later.</p>
          <button onClick={()=>{localStorage.removeItem(STORAGE_KEY);handleNew();}}>Clear Browser Save</button>
        </section>
      ) : (
        <section className={["play-area", showHistory ? "" : "history-hidden"].join(" ")}>
          <div className="board-wrap">
            <div className={`board size-${size}`} aria-label="Draughts board">
              {Array.from({length:size},(_,y)=>
                Array.from({length:size},(_,x)=>{
                  const dark=isDarkSquare(x,y);
                  const piece=displayBoard[y][x];
                  const isSel=!isReviewing && selected?.[0]===x&&selected?.[1]===y;
                  const isTarget=!isReviewing && targets.has(`${x},${y}`);
                  const isSelectable=dark&&selectables.has(`${x},${y}`)&&isHumanTurn&&!thinking;
                  const owner=pieceOwner(piece);
                  return (
                    <div
                      key={`${y}-${x}`}
                      className={[
                        "square",
                        dark?"dark":"light",
                        isSelectable&&!isSel?"clickable":"",
                        isSel?"selected":"",
                        isTarget?"target-move":""
                      ].join(" ")}
                      onClick={()=>dark?tapSquare(x,y):undefined}
                    >
                      {piece!==0 && (
                        <div className={`piece-wrap ${owner===1?"piece-red":"piece-black"}`}>
                          {isKing(piece)&&<div className="king-marker">K</div>}
                        </div>
                      )}
                    </div>
                  );
                })
              )}
            </div>
          </div>

          {showHistory && (
          <aside className="history">
            <h2>Moves</h2>
            <div className="review-nav">
              <button onClick={() => goReview(0)} disabled={displayIdx === 0} title="Start">◀◀</button>
              <button onClick={() => goReview(Math.max(0, displayIdx - 1))} disabled={displayIdx === 0} title="Previous">◀</button>
              <span className="review-label">{isReviewing ? `${displayIdx}/${totalMoves}` : "Live"}</span>
              <button onClick={() => displayIdx < totalMoves ? goReview(displayIdx + 1) : goReview(null)} disabled={displayIdx >= totalMoves} title="Next">▶</button>
              <button onClick={() => goReview(null)} disabled={!isReviewing} title="Live">▶▶</button>
            </div>
            <ol>
              {game.moves.map((m,i)=>(
                <li
                  key={i}
                  className={displayIdx === i + 1 ? "active" : ""}
                  onClick={() => goReview(i + 1)}
                >{m}</li>
              ))}
            </ol>
          </aside>
          )}
        </section>
      )}

      <footer className="notes">
        <p>Captures are mandatory. Kings marked K. State auto-saved. Save/Load for manual restore.</p>
      </footer>
    </main>
  );
}
