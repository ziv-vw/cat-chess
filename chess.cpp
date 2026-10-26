// Chess engine, built to wasm. A Piece base class with a subclass per type,
// each generating its own moves, over a 0x88 board. The board is a value type
// so make/unmake is just a copy, and king safety is a square-attacked test.
// Freestanding (no STL/libc), so the wasm has no imports.

typedef unsigned long size_t;
extern "C" void* memset(void*p,int v,size_t n){auto d=(unsigned char*)p;while(n--)*d++=(unsigned char)v;return p;}
extern "C" void* memcpy(void*d,const void*s,size_t n){auto a=(unsigned char*)d;auto b=(const unsigned char*)s;while(n--)*a++=*b++;return d;}
extern "C" void __cxa_pure_virtual(){}
inline void* operator new(size_t, void* p) noexcept { return p; }
void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}

enum { PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6 };
#define TYPE(p)  ((p) & 7)
#define COLOR(p) ((p) >> 3)
#define CODE(t,c) ((t) | ((c) << 3))
#define OFF(sq)  ((sq) & 0x88)
#define RANK(sq) ((sq) >> 4)
#define FILE(sq) ((sq) & 7)
#define SQ(r,f)  (((r) << 4) | (f))
enum { C_WK=1, C_WQ=2, C_BK=4, C_BQ=8 };
enum { F_NORMAL=0, F_DOUBLE=1, F_EP=2, F_CASTLE=3, F_PROMO=4 };

struct Move { unsigned char from, to, promo, flag; };
struct MoveList {
    Move m[256]; int n = 0;
    void add(int from,int to,int promo,int flag){ m[n].from=from;m[n].to=to;m[n].promo=promo;m[n].flag=flag;n++; }
};

struct Board {
    signed char b[128];
    unsigned char side, castle;
    signed char ep;
};

// move offsets (0x88)
static const int KN[8] = { 33, 31, 18, 14, -14, -18, -31, -33 };
static const int KG[8] = { 16, -16, 1, -1, 15, 17, -15, -17 };
static const int BD[4] = { 15, 17, -15, -17 };
static const int RD[4] = { 16, -16, 1, -1 };

// is `sq` attacked by a piece of color `by`?
static bool attacked(const Board& B, int sq, int by){
    if (by == 0){
        if (!OFF(sq-15) && B.b[sq-15]==CODE(PAWN,0)) return true;
        if (!OFF(sq-17) && B.b[sq-17]==CODE(PAWN,0)) return true;
    } else {
        if (!OFF(sq+15) && B.b[sq+15]==CODE(PAWN,1)) return true;
        if (!OFF(sq+17) && B.b[sq+17]==CODE(PAWN,1)) return true;
    }
    for (int i=0;i<8;i++){ int s=sq+KN[i];
        if (!OFF(s) && B.b[s] && TYPE(B.b[s])==KNIGHT && COLOR(B.b[s])==by) return true; }
    for (int i=0;i<8;i++){ int s=sq+KG[i];
        if (!OFF(s) && B.b[s] && TYPE(B.b[s])==KING && COLOR(B.b[s])==by) return true; }
    for (int i=0;i<4;i++){ int s=sq+BD[i];
        while(!OFF(s)){ if(B.b[s]){ int t=TYPE(B.b[s]); if(COLOR(B.b[s])==by&&(t==BISHOP||t==QUEEN)) return true; break;} s+=BD[i]; } }
    for (int i=0;i<4;i++){ int s=sq+RD[i];
        while(!OFF(s)){ if(B.b[s]){ int t=TYPE(B.b[s]); if(COLOR(B.b[s])==by&&(t==ROOK||t==QUEEN)) return true; break;} s+=RD[i]; } }
    return false;
}

// each piece type knows how to generate its own pseudo-legal moves
struct Piece {
    virtual int value() const = 0;
    virtual void genMoves(const Board& B, int from, MoveList& ml) const = 0;
    virtual ~Piece(){}
};

static void slide(const Board& B, int from, int c, const int* dir, int nd, MoveList& ml){
    for (int i=0;i<nd;i++){ int to=from+dir[i];
        while(!OFF(to)){
            if(!B.b[to]) ml.add(from,to,0,F_NORMAL);
            else { if(COLOR(B.b[to])!=c) ml.add(from,to,0,F_NORMAL); break; }
            to+=dir[i];
        } }
}

struct Pawn : Piece {
    int value() const override { return 1; }
    void genMoves(const Board& B, int from, MoveList& ml) const override {
        int c=COLOR(B.b[from]);
        int fwd = c==0?16:-16, start = c==0?1:6, last = c==0?7:0;
        int one=from+fwd;
        if(!OFF(one)&&!B.b[one]){
            if(RANK(one)==last){ ml.add(from,one,QUEEN,F_PROMO); ml.add(from,one,ROOK,F_PROMO);
                                 ml.add(from,one,BISHOP,F_PROMO); ml.add(from,one,KNIGHT,F_PROMO); }
            else ml.add(from,one,0,F_NORMAL);
            int two=from+2*fwd;
            if(RANK(from)==start && !B.b[two]) ml.add(from,two,0,F_DOUBLE);
        }
        for(int d=-1;d<=1;d+=2){ int cap=from+fwd+d;
            if(OFF(cap)) continue;
            if(B.b[cap] && COLOR(B.b[cap])!=c){
                if(RANK(cap)==last){ ml.add(from,cap,QUEEN,F_PROMO); ml.add(from,cap,ROOK,F_PROMO);
                                     ml.add(from,cap,BISHOP,F_PROMO); ml.add(from,cap,KNIGHT,F_PROMO); }
                else ml.add(from,cap,0,F_NORMAL);
            } else if(cap==B.ep) ml.add(from,cap,0,F_EP);
        }
    }
};
struct Knight : Piece {
    int value() const override { return 3; }
    void genMoves(const Board& B, int from, MoveList& ml) const override {
        int c=COLOR(B.b[from]);
        for(int i=0;i<8;i++){ int to=from+KN[i];
            if(!OFF(to)&&(!B.b[to]||COLOR(B.b[to])!=c)) ml.add(from,to,0,F_NORMAL); }
    }
};
struct Bishop : Piece {
    int value() const override { return 3; }
    void genMoves(const Board& B, int from, MoveList& ml) const override { slide(B,from,COLOR(B.b[from]),BD,4,ml); }
};
struct Rook : Piece {
    int value() const override { return 5; }
    void genMoves(const Board& B, int from, MoveList& ml) const override { slide(B,from,COLOR(B.b[from]),RD,4,ml); }
};
struct Queen : Piece {
    int value() const override { return 9; }
    void genMoves(const Board& B, int from, MoveList& ml) const override { slide(B,from,COLOR(B.b[from]),KG,8,ml); }
};
struct King : Piece {
    int value() const override { return 0; }
    void genMoves(const Board& B, int from, MoveList& ml) const override {
        int c=COLOR(B.b[from]);
        for(int i=0;i<8;i++){ int to=from+KG[i];
            if(!OFF(to)&&(!B.b[to]||COLOR(B.b[to])!=c)) ml.add(from,to,0,F_NORMAL); }
        int r=RANK(from), opp=c^1;
        int kBit=c==0?C_WK:C_BK, qBit=c==0?C_WQ:C_BQ;
        if((B.castle&kBit)&&!B.b[SQ(r,5)]&&!B.b[SQ(r,6)]
           && !attacked(B,SQ(r,4),opp)&&!attacked(B,SQ(r,5),opp)&&!attacked(B,SQ(r,6),opp))
            ml.add(from,SQ(r,6),0,F_CASTLE);
        if((B.castle&qBit)&&!B.b[SQ(r,3)]&&!B.b[SQ(r,2)]&&!B.b[SQ(r,1)]
           && !attacked(B,SQ(r,4),opp)&&!attacked(B,SQ(r,3),opp)&&!attacked(B,SQ(r,2),opp))
            ml.add(from,SQ(r,2),0,F_CASTLE);
    }
};

// singletons, one per piece type (placement-new'd — no static init needed)
alignas(Pawn)   static char bufP[sizeof(Pawn)];
alignas(Knight) static char bufN[sizeof(Knight)];
alignas(Bishop) static char bufB[sizeof(Bishop)];
alignas(Rook)   static char bufR[sizeof(Rook)];
alignas(Queen)  static char bufQ[sizeof(Queen)];
alignas(King)   static char bufK[sizeof(King)];
static Piece* PIECES[7];
static void initPieces(){
    PIECES[PAWN]  =new(bufP) Pawn();
    PIECES[KNIGHT]=new(bufN) Knight();
    PIECES[BISHOP]=new(bufB) Bishop();
    PIECES[ROOK]  =new(bufR) Rook();
    PIECES[QUEEN] =new(bufQ) Queen();
    PIECES[KING]  =new(bufK) King();
}

static void makeMove(Board& B, Move m){
    int from=m.from, to=m.to, c=B.side, pc=B.b[from];
    if(m.flag==F_EP) B.b[c==0?to-16:to+16]=0;
    if(m.flag==F_CASTLE){ int r=RANK(from);
        if(FILE(to)==6){ B.b[SQ(r,5)]=B.b[SQ(r,7)]; B.b[SQ(r,7)]=0; }
        else           { B.b[SQ(r,3)]=B.b[SQ(r,0)]; B.b[SQ(r,0)]=0; } }
    B.b[to] = (m.flag==F_PROMO) ? CODE(m.promo,c) : pc;
    B.b[from]=0;
    if(TYPE(pc)==KING) B.castle &= c==0 ? ~(C_WK|C_WQ) : ~(C_BK|C_BQ);
    if(from==SQ(0,0)||to==SQ(0,0)) B.castle &= ~C_WQ;
    if(from==SQ(0,7)||to==SQ(0,7)) B.castle &= ~C_WK;
    if(from==SQ(7,0)||to==SQ(7,0)) B.castle &= ~C_BQ;
    if(from==SQ(7,7)||to==SQ(7,7)) B.castle &= ~C_BK;
    B.ep = (m.flag==F_DOUBLE) ? (signed char)(c==0?from+16:from-16) : -1;
    B.side ^= 1;
}
static int kingSq(const Board& B, int c){
    for(int s=0;s<128;s++) if(!OFF(s)&&B.b[s]&&TYPE(B.b[s])==KING&&COLOR(B.b[s])==c) return s;
    return -1;
}
static bool inCheck(const Board& B, int c){ return attacked(B, kingSq(B,c), c^1); }

static void genAll(const Board& B, MoveList& ml){
    for(int s=0;s<128;s++){
        if(OFF(s)||!B.b[s]||COLOR(B.b[s])!=B.side) continue;
        PIECES[TYPE(B.b[s])]->genMoves(B, s, ml);
    }
}
static int legal(const Board& B, Move* out){
    MoveList ml; genAll(B, ml); int k=0;
    for(int i=0;i<ml.n;i++){ Board t=B; makeMove(t, ml.m[i]);
        if(!inCheck(t, B.side)) out[k++]=ml.m[i]; }
    return k;
}

// the live game
static Board G;
static unsigned int rng = 0x2545F491u;
static unsigned int rnd(){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }

static void setStart(Board& B){
    for(int i=0;i<128;i++) B.b[i]=0;
    int back[8]={ROOK,KNIGHT,BISHOP,QUEEN,KING,BISHOP,KNIGHT,ROOK};
    for(int f=0;f<8;f++){
        B.b[SQ(0,f)]=CODE(back[f],0); B.b[SQ(1,f)]=CODE(PAWN,0);
        B.b[SQ(6,f)]=CODE(PAWN,1);    B.b[SQ(7,f)]=CODE(back[f],1);
    }
    B.side=0; B.castle=C_WK|C_WQ|C_BK|C_BQ; B.ep=-1;
}

static unsigned long long perftRec(Board& B, int depth){
    Move mv[256]; int n=legal(B, mv);
    if(depth<=1) return (unsigned long long)n;
    unsigned long long nodes=0;
    for(int i=0;i<n;i++){ Board t=B; makeMove(t, mv[i]); nodes+=perftRec(t, depth-1); }
    return nodes;
}

// exported API — the functions the page calls
extern "C" {

__attribute__((export_name("reset")))     void reset(){ initPieces(); setStart(G); }
__attribute__((export_name("seed")))      void seed(unsigned int s){ if(s) rng=s; }
__attribute__((export_name("side")))      int side(){ return G.side; }
__attribute__((export_name("pieceAt")))   int pieceAt(int s){ int sq=SQ(s>>3,s&7); return OFF(sq)?0:G.b[sq]; }

__attribute__((export_name("status")))
int status(){ Move mv[256]; int n=legal(G, mv); bool chk=inCheck(G,G.side);
    if(n==0) return chk?2:3; return chk?1:0; }

static unsigned char destBuf[32]; static int destN;
__attribute__((export_name("movesFrom")))
int movesFrom(int s){ int from=SQ(s>>3,s&7); Move mv[256]; int n=legal(G,mv); destN=0;
    for(int i=0;i<n;i++) if(mv[i].from==from){ int to=mv[i].to; destBuf[destN++]=(RANK(to)<<3)|FILE(to); if(destN>=32) break; }
    return destN; }
__attribute__((export_name("destAt")))  int destAt(int i){ return (i>=0&&i<destN)?destBuf[i]:-1; }

__attribute__((export_name("isPromotion")))
int isPromotion(int fs,int ts){ int from=SQ(fs>>3,fs&7), to=SQ(ts>>3,ts&7);
    Move mv[256]; int n=legal(G,mv);
    for(int i=0;i<n;i++) if(mv[i].from==from&&mv[i].to==to&&mv[i].flag==F_PROMO) return 1;
    return 0; }

__attribute__((export_name("move")))
int move(int fs,int ts,int promoType){ int from=SQ(fs>>3,fs&7), to=SQ(ts>>3,ts&7);
    Move mv[256]; int n=legal(G,mv);
    for(int i=0;i<n;i++){ if(mv[i].from!=from||mv[i].to!=to) continue;
        if(mv[i].flag==F_PROMO){ int want=promoType?promoType:QUEEN; if(mv[i].promo!=want) continue; }
        makeMove(G, mv[i]); return 1; }
    return 0; }

__attribute__((export_name("aiMove")))
int aiMove(){ Move mv[256]; int n=legal(G,mv); if(n==0) return -1;
    int best=-1000000, bi=0;
    for(int i=0;i<n;i++){ int sc=0; int victim=G.b[mv[i].to];
        if(victim) sc += 10*PIECES[TYPE(victim)]->value();
        if(mv[i].flag==F_PROMO) sc += 8*PIECES[mv[i].promo]->value();
        Board t=G; makeMove(t, mv[i]); Move r[256]; int rn=legal(t,r);
        if(rn==0 && inCheck(t,t.side)) sc += 100000;
        sc += (int)(rnd()%5);
        if(sc>best){ best=sc; bi=i; } }
    int from=mv[bi].from, to=mv[bi].to;
    int promo=(mv[bi].flag==F_PROMO)?mv[bi].promo:0;
    makeMove(G, mv[bi]);
    int f=(RANK(from)<<3)|FILE(from), tt=(RANK(to)<<3)|FILE(to);
    return (promo<<16)|(f<<8)|tt; }

__attribute__((export_name("perft")))
int perft(int depth){ Board t=G; return (int)perftRec(t, depth); }

} // extern "C"
