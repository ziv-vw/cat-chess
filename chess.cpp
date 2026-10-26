// Chess engine — the original C++ project (Piece hierarchy, Coordinate, Board,
// Game, Player), repaired and completed so it compiles to WebAssembly.
//
// Kept from the original: the class design, and each piece's canMove() rule
// (Pawn/Knight/Bishop/Rook/Queen/King) — with the pawn/bishop/queen move bugs
// fixed. Rewritten: king-safety (the old checking()/endGame() were broken).
// Added, because the original never had them: castling, en passant, promotion.
// Converted: shared_ptr/vector/iostream -> raw pointers over a fixed board,
// since the standard library can't come along to wasm.

// ---- freestanding runtime (no libc / libc++) ----
typedef unsigned long usize;
extern "C" void* memset(void*p,int v,usize n){auto d=(unsigned char*)p;while(n--)*d++=(unsigned char)v;return p;}
extern "C" void* memcpy(void*d,const void*s,usize n){auto a=(unsigned char*)d;auto b=(const unsigned char*)s;while(n--)*a++=*b++;return d;}
extern "C" void __cxa_pure_virtual(){}
inline void* operator new(usize, void* p) noexcept { return p; }
void operator delete(void*) noexcept {}
void operator delete(void*, usize) noexcept {}

// a stack allocator for Piece objects; make/unmake restore the top, reset() clears it
static char pool[1u<<21];
static usize poolTop = 0;
static void* palloc(usize n){ n=(n+7)&~7ul; void* p=pool+poolTop; poolTop+=n; return p; }

static int sgn(int x){ return (x>0)-(x<0); }

enum { PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6 };
enum { C_WK=1, C_WQ=2, C_BK=4, C_BQ=8 };
enum { F_NORMAL=0, F_DOUBLE=1, F_EP=2, F_CASTLE=3, F_PROMO=4 };

class Game;
class Piece;

// --- Coordinate: a board square (the original class, with a raw Piece*) ---
// no in-class initializers: they'd make Game need C++ dynamic init, which a
// freestanding wasm never runs. setup()/start() set every field explicitly.
struct Coordinate { Piece* pc; int rank, file; };

// --- Player: just identity + a link back to the game (as in the original) ---
struct Player { int id; Game* game; };

// --- Board: 8x8 of Coordinates (was a vector<Coordinate> of shared_ptr) ---
struct Board {
    Coordinate cell[64];
    Coordinate& at(int sq){ return cell[sq]; }
    Coordinate& at(int r,int f){ return cell[r*8+f]; }
    void setup(){ for(int r=0;r<8;r++)for(int f=0;f<8;f++){ cell[r*8+f].rank=r; cell[r*8+f].file=f; cell[r*8+f].pc=nullptr; } }
};

// --- Piece: base class; each kind still implements its own canMove() ---
struct Piece {
    int rank, file;
    Player* owner;
    bool hasMoved;
    int color() const { return owner->id; }
    virtual int kind() const = 0;
    virtual int value() const = 0;
    // can this piece pseudo-move to square `to`? (occupancy + path; not king-safety)
    virtual bool canMove(int to) const = 0;
    virtual ~Piece(){}
    Board& board() const;                 // defined after Game
    bool targetOk(int to) const;          // empty, or an enemy piece
    bool pathClear(int to) const;         // straight/diagonal path is empty
};

// --- the six pieces (canMove kept from the original, bugs fixed) ---
struct Pawn : Piece {
    int kind() const override { return PAWN; }
    int value() const override { return 1; }
    bool canMove(int to) const override {
        Coordinate& t = (const_cast<Pawn*>(this))->board().at(to);
        int r1=t.rank, f1=t.file, dir = color()==0 ? 1 : -1, start = color()==0 ? 1 : 6;
        if(f1==file && r1==rank+dir && !t.pc) return true;                 // one forward
        if(f1==file && r1==rank+2*dir && rank==start && !t.pc              // two forward
           && !board().at(rank+dir,file).pc) return true;                 // (fix: path must be clear)
        if((f1==file+1||f1==file-1) && r1==rank+dir && t.pc && t.pc->color()!=color()) // diagonal capture
            return true;                                                   // (fix: black used rank+1)
        return false;
    }
};
struct Knight : Piece {
    int kind() const override { return KNIGHT; }
    int value() const override { return 3; }
    bool canMove(int to) const override {
        Coordinate& t=(const_cast<Knight*>(this))->board().at(to);
        int dr=t.rank-rank, df=t.file-file; dr=dr<0?-dr:dr; df=df<0?-df:df;
        return ((dr==1&&df==2)||(dr==2&&df==1)) && targetOk(to);
    }
};
struct Bishop : Piece {
    int kind() const override { return BISHOP; }
    int value() const override { return 3; }
    bool canMove(int to) const override {
        Coordinate& t=(const_cast<Bishop*>(this))->board().at(to);
        int dr=t.rank-rank, df=t.file-file;
        if(dr==0 || (dr!=df && dr!=-df)) return false;                     // must be a diagonal
        return targetOk(to) && pathClear(to);                              // (fix: old diagonals mis-looped)
    }
};
struct Rook : Piece {
    int kind() const override { return ROOK; }
    int value() const override { return 5; }
    bool canMove(int to) const override {
        Coordinate& t=(const_cast<Rook*>(this))->board().at(to);
        if(!((t.rank==rank)^(t.file==file))) return false;                 // same rank xor same file
        return targetOk(to) && pathClear(to);
    }
};
struct Queen : Piece {
    int kind() const override { return QUEEN; }
    int value() const override { return 9; }
    bool canMove(int to) const override {
        Coordinate& t=(const_cast<Queen*>(this))->board().at(to);
        int dr=t.rank-rank, df=t.file-file;
        bool straight = (dr==0)^(df==0);
        bool diag = (dr!=0) && (dr==df || dr==-df);
        if(!(straight||diag)) return false;
        return targetOk(to) && pathClear(to);
    }
};
struct King : Piece {
    int kind() const override { return KING; }
    int value() const override { return 0; }
    bool canMove(int to) const override {
        Coordinate& t=(const_cast<King*>(this))->board().at(to);
        int dr=t.rank-rank, df=t.file-file; dr=dr<0?-dr:dr; df=df<0?-df:df;
        return (dr<=1&&df<=1&&(dr||df)) && targetOk(to);                   // castling handled in move gen
    }
};

// --- a move + an undo record so make/unmake can roll back ---
struct Move { int from, to, flag, promo; };
struct Undo {
    Move m;
    Piece* moved; bool movedHadMoved;
    Piece* captured; int capSq;
    Piece* promoted;                 // the piece a pawn became (promotion)
    int prevCastle, prevEp; usize prevPool;
};

// --- Game: holds the board, side to move, castling rights, ep square ---
class Game {
public:
    Board b;
    Player white, black;
    int side, castle, ep;
    Undo stack[1024]; int sp;

    Player* player(int c){ return c==0?&white:&black; }

    template<class T> Piece* alloc(int r,int f,int c){
        T* p=(T*)palloc(sizeof(T)); new(p) T();
        p->rank=r; p->file=f; p->owner=player(c); p->hasMoved=false; return p;
    }
    Piece* make_kind(int kind,int r,int f,int c){
        switch(kind){
            case PAWN:   return alloc<Pawn>(r,f,c);
            case KNIGHT: return alloc<Knight>(r,f,c);
            case BISHOP: return alloc<Bishop>(r,f,c);
            case ROOK:   return alloc<Rook>(r,f,c);
            case QUEEN:  return alloc<Queen>(r,f,c);
            default:     return alloc<King>(r,f,c);
        }
    }

    void start(){
        white.id=0; white.game=this; black.id=1; black.game=this;  // set up here (no static init in wasm)
        poolTop=0; sp=0; b.setup();
        int back[8]={ROOK,KNIGHT,BISHOP,QUEEN,KING,BISHOP,KNIGHT,ROOK};
        for(int f=0;f<8;f++){
            b.at(0,f).pc=make_kind(back[f],0,f,0);
            b.at(1,f).pc=make_kind(PAWN,1,f,0);
            b.at(6,f).pc=make_kind(PAWN,6,f,1);
            b.at(7,f).pc=make_kind(back[f],7,f,1);
        }
        side=0; castle=C_WK|C_WQ|C_BK|C_BQ; ep=-1;
    }

    int kingSq(int c){
        for(int s=0;s<64;s++){ Piece* p=b.cell[s].pc; if(p&&p->kind()==KING&&p->color()==c) return s; }
        return -1;
    }

    // --- is square `sq` attacked by color `by`? (the correct replacement for checking()) ---
    bool attacked(int sq,int by){
        int r=sq/8, f=sq%8;
        int pdir = by==0 ? 1 : -1;               // a `by` pawn sits one rank *behind* the square it attacks
        for(int df=-1;df<=1;df+=2){ int pr=r-pdir, pf=f+df;
            if(pr>=0&&pr<8&&pf>=0&&pf<8){ Piece* p=b.at(pr,pf).pc; if(p&&p->kind()==PAWN&&p->color()==by) return true; } }
        static const int kn[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
        for(auto&o:kn){ int rr=r+o[0],ff=f+o[1]; if(rr>=0&&rr<8&&ff>=0&&ff<8){ Piece* p=b.at(rr,ff).pc; if(p&&p->kind()==KNIGHT&&p->color()==by) return true; } }
        for(int dr=-1;dr<=1;dr++)for(int df=-1;df<=1;df++){ if(!dr&&!df)continue; int rr=r+dr,ff=f+df;
            if(rr>=0&&rr<8&&ff>=0&&ff<8){ Piece* p=b.at(rr,ff).pc; if(p&&p->kind()==KING&&p->color()==by) return true; } }
        static const int diag[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
        for(auto&d:diag){ int rr=r+d[0],ff=f+d[1];
            while(rr>=0&&rr<8&&ff>=0&&ff<8){ Piece* p=b.at(rr,ff).pc; if(p){ if(p->color()==by&&(p->kind()==BISHOP||p->kind()==QUEEN))return true; break;} rr+=d[0];ff+=d[1]; } }
        static const int orth[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
        for(auto&d:orth){ int rr=r+d[0],ff=f+d[1];
            while(rr>=0&&rr<8&&ff>=0&&ff<8){ Piece* p=b.at(rr,ff).pc; if(p){ if(p->color()==by&&(p->kind()==ROOK||p->kind()==QUEEN))return true; break;} rr+=d[0];ff+=d[1]; } }
        return false;
    }
    bool inCheck(int c){ return attacked(kingSq(c), c^1); }

    // --- pseudo-legal move generation (uses each piece's canMove) ---
    int gen(Move* out){
        int n=0, c=side;
        for(int s=0;s<64;s++){
            Piece* p=b.cell[s].pc; if(!p||p->color()!=c) continue;
            int kind=p->kind(), r=s/8, f=s%8;
            if(kind==PAWN){
                int dir=c==0?1:-1, last=c==0?7:0;
                for(int to=0;to<64;to++){
                    if(!p->canMove(to)) continue;
                    int tr=to/8;
                    if(tr==last){ out[n++]={s,to,F_PROMO,QUEEN}; out[n++]={s,to,F_PROMO,ROOK};
                                  out[n++]={s,to,F_PROMO,BISHOP}; out[n++]={s,to,F_PROMO,KNIGHT}; }
                    else if(tr==r+2*dir) out[n++]={s,to,F_DOUBLE,0};
                    else out[n++]={s,to,F_NORMAL,0};
                }
                if(ep>=0){ int er=ep/8, ef=ep%8;                           // en passant (canMove can't see it)
                    if(er==r+dir && (ef==f+1||ef==f-1) && !b.at(ep).pc) out[n++]={s,ep,F_EP,0}; }
            } else if(kind==KING){
                for(int to=0;to<64;to++) if(p->canMove(to)) out[n++]={s,to,F_NORMAL,0};
                int opp=c^1, kBit=c==0?C_WK:C_BK, qBit=c==0?C_WQ:C_BQ;
                if((castle&kBit)&&!b.at(r,5).pc&&!b.at(r,6).pc
                   && !attacked(r*8+4,opp)&&!attacked(r*8+5,opp)&&!attacked(r*8+6,opp)) out[n++]={s,r*8+6,F_CASTLE,0};
                if((castle&qBit)&&!b.at(r,3).pc&&!b.at(r,2).pc&&!b.at(r,1).pc
                   && !attacked(r*8+4,opp)&&!attacked(r*8+3,opp)&&!attacked(r*8+2,opp)) out[n++]={s,r*8+2,F_CASTLE,0};
            } else {
                for(int to=0;to<64;to++) if(p->canMove(to)) out[n++]={s,to,F_NORMAL,0};
            }
        }
        return n;
    }

    void makeMove(Move m){
        Undo& u=stack[sp++];
        u.m=m; u.prevCastle=castle; u.prevEp=ep; u.prevPool=poolTop; u.promoted=nullptr;
        int c=side; Piece* mv=b.at(m.from).pc; u.moved=mv; u.movedHadMoved=mv->hasMoved;
        int capSq = m.flag==F_EP ? (m.from/8)*8 + (m.to%8) : m.to;
        u.captured=b.at(capSq).pc; u.capSq=capSq; b.at(capSq).pc=nullptr;

        b.at(m.from).pc=nullptr;
        mv->rank=m.to/8; mv->file=m.to%8; mv->hasMoved=true;
        if(m.flag==F_PROMO){ Piece* np=make_kind(m.promo, m.to/8, m.to%8, c); np->hasMoved=true; b.at(m.to).pc=np; u.promoted=np; }
        else b.at(m.to).pc=mv;

        if(m.flag==F_CASTLE){ int r=m.from/8;
            if(m.to%8==6){ Piece* rk=b.at(r,7).pc; b.at(r,5).pc=rk; b.at(r,7).pc=nullptr; rk->file=5; rk->hasMoved=true; }
            else         { Piece* rk=b.at(r,0).pc; b.at(r,3).pc=rk; b.at(r,0).pc=nullptr; rk->file=3; rk->hasMoved=true; } }

        if(mv->kind()==KING) castle &= c==0 ? ~(C_WK|C_WQ) : ~(C_BK|C_BQ);
        auto clr=[&](int sq){ if(sq==0)castle&=~C_WQ; else if(sq==7)castle&=~C_WK; else if(sq==56)castle&=~C_BQ; else if(sq==63)castle&=~C_BK; };
        clr(m.from); clr(m.to);
        ep = m.flag==F_DOUBLE ? ( (m.from/8 + (c==0?1:-1))*8 + m.from%8 ) : -1;
        side^=1;
    }
    void unmakeMove(){
        Undo& u=stack[--sp]; Move m=u.m; side^=1; int c=side;
        castle=u.prevCastle; ep=u.prevEp;
        if(m.flag==F_CASTLE){ int r=m.from/8;
            if(m.to%8==6){ Piece* rk=b.at(r,5).pc; b.at(r,7).pc=rk; b.at(r,5).pc=nullptr; rk->file=7; rk->hasMoved=false; }
            else         { Piece* rk=b.at(r,3).pc; b.at(r,0).pc=rk; b.at(r,3).pc=nullptr; rk->file=0; rk->hasMoved=false; } }
        b.at(m.to).pc=nullptr;
        u.moved->rank=m.from/8; u.moved->file=m.from%8; u.moved->hasMoved=u.movedHadMoved;
        b.at(m.from).pc=u.moved;
        b.at(u.capSq).pc=u.captured;
        poolTop=u.prevPool;                 // frees a promoted piece if one was allocated
    }

    int legal(Move* out){
        Move ps[218]; int n=gen(ps), k=0;
        for(int i=0;i<n;i++){ makeMove(ps[i]); if(!inCheck(side^1)) out[k++]=ps[i]; unmakeMove(); }
        return k;
    }

    unsigned long long perft(int depth){
        Move mv[218]; int n=legal(mv);
        if(depth<=1) return (unsigned long long)n;
        unsigned long long nodes=0;
        for(int i=0;i<n;i++){ makeMove(mv[i]); nodes+=perft(depth-1); unmakeMove(); }
        return nodes;
    }
};

// now that Game is complete, wire up the Piece helpers that need it
Board& Piece::board() const { return owner->game->b; }
bool Piece::targetOk(int to) const { Piece* t=owner->game->b.at(to).pc; return !t || t->color()!=color(); }
bool Piece::pathClear(int to) const {
    Board& bd=owner->game->b; int tr=to/8, tf=to%8;
    int dr=sgn(tr-rank), df=sgn(tf-file), r=rank+dr, f=file+df;
    while(r!=tr||f!=tf){ if(bd.at(r,f).pc) return false; r+=dr; f+=df; }
    return true;
}

// =============================== exported API ===============================
static Game G;
static unsigned rngS = 0x2545F491u;
static unsigned rnd(){ rngS^=rngS<<13; rngS^=rngS>>17; rngS^=rngS<<5; return rngS; }

extern "C" {

__attribute__((export_name("reset")))   void reset(){ G.start(); }
__attribute__((export_name("seed")))     void seed(unsigned s){ if(s){ rngS=s; } }
__attribute__((export_name("side")))     int side(){ return G.side; }
__attribute__((export_name("pieceAt")))  int pieceAt(int s){ Piece* p=G.b.cell[s].pc; return p ? (p->kind() | (p->color()<<3)) : 0; }

__attribute__((export_name("status")))
int status(){ Move mv[218]; int n=G.legal(mv); bool chk=G.inCheck(G.side);
    if(n==0) return chk?2:3; return chk?1:0; }

static unsigned char destBuf[40]; static int destN;
__attribute__((export_name("movesFrom")))
int movesFrom(int from){ Move mv[218]; int n=G.legal(mv); destN=0;
    for(int i=0;i<n;i++) if(mv[i].from==from){ int to=mv[i].to; bool dup=false;
        for(int k=0;k<destN;k++) if(destBuf[k]==to){dup=true;break;}
        if(!dup&&destN<40) destBuf[destN++]=(unsigned char)to; }
    return destN; }
__attribute__((export_name("destAt"))) int destAt(int i){ return (i>=0&&i<destN)?destBuf[i]:-1; }

__attribute__((export_name("isPromotion")))
int isPromotion(int from,int to){ Move mv[218]; int n=G.legal(mv);
    for(int i=0;i<n;i++) if(mv[i].from==from&&mv[i].to==to&&mv[i].flag==F_PROMO) return 1; return 0; }

__attribute__((export_name("move")))
int move(int from,int to,int promoType){ Move mv[218]; int n=G.legal(mv);
    for(int i=0;i<n;i++){ if(mv[i].from!=from||mv[i].to!=to) continue;
        if(mv[i].flag==F_PROMO){ int want=promoType?promoType:QUEEN; if(mv[i].promo!=want) continue; }
        G.makeMove(mv[i]); return 1; }
    return 0; }

__attribute__((export_name("aiMove")))
int aiMove(){ Move mv[218]; int n=G.legal(mv); if(n==0) return -1;
    int best=-1000000, bi=0;
    for(int i=0;i<n;i++){ int sc=0; Piece* vic=G.b.at(mv[i].to).pc;
        if(vic) sc += 10*vic->value();
        if(mv[i].flag==F_PROMO) sc += 8*mv[i].promo;
        G.makeMove(mv[i]); Move r[218]; int rn=G.legal(r); if(rn==0&&G.inCheck(G.side)) sc+=100000; G.unmakeMove();
        sc += (int)(rnd()%5);
        if(sc>best){ best=sc; bi=i; } }
    int from=mv[bi].from, to=mv[bi].to, promo=(mv[bi].flag==F_PROMO)?mv[bi].promo:0;
    G.makeMove(mv[bi]);
    return (promo<<16)|(from<<8)|to; }

__attribute__((export_name("perft")))
int perft(int depth){ return (int)G.perft(depth); }

} // extern "C"
