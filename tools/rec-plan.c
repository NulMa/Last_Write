/* rec-plan — 제작 수기용 GIF 를 찍기 위한 입력 스크립트를 찾는다.
 *
 * 사람이 키를 눌러 녹화하는 대신, sim.c 를 헤드리스로 돌려서
 * "죽지 않고 가장 멀리 가는" 입력을 빔 탐색으로 찾는다.
 * 결과는 프레임마다 한 줄인 텍스트로 내고, floppy.exe --rec 가 그걸 재생하며 녹화한다.
 *
 *   rec-plan <level> <frames> <loadout-mask> [lead-frames] > script.txt
 *
 *   level         LEVELS[] 인덱스
 *   frames        몇 프레임을 계획할지 (60 = 1초)
 *   loadout-mask  장착할 능력 비트 (boot=1 dash=2 glide=4 grip=8 jump2=16)
 *   lead-frames   터미널 위에서 로드아웃을 갈아끼우며 서 있는 시간 (기본 60)
 *
 * 제출물이 아니다. 1.44MB 예산과 무관하다.
 */
#include "../src/sim.h"
#include "../src/levels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK   6
#define BEAM    400
#define NACT    9
#define MAXF    4000

typedef struct { int r, l, jh, jp, dp, tog; } Key;

typedef struct { int prev, act; } Hist;   /* 탐색 이력은 부모와 행동만 — World 를 들면 GB 단위가 된다 */
typedef struct { World w; int hist; float score; } Node;

static const Tune *T;

/* 행동 하나 = CHUNK 프레임 동안의 입력 패턴 */
static void act_keys(int a, int f, int jumpWasHeld, Key *k)
{
    memset(k, 0, sizeof(*k)); k->tog = -1;
    switch (a) {
    case 0: k->r = 1; break;                                         /* 걷기 */
    case 1: k->r = 1; k->jh = 1; k->jp = (f == 0); break;           /* 긴 점프 */
    case 2: k->r = 1; k->jh = (f < 3); k->jp = (f == 0); break;      /* 짧은 점프 */
    case 3: k->r = 1; k->dp = (f == 0); break;                       /* 대시 */
    case 4: k->r = 1; k->jh = 1; break;                              /* 점프 유지 · 활공 */
    case 5: break;                                                   /* 멈춤 */
    case 6: k->l = 1; break;                                         /* 뒤로 */
    case 7: k->jh = 1; k->jp = (f == 0); break;                      /* 제자리 점프 */
    case 8: k->r = 1; k->jh = 1; k->jp = (f == 0); k->dp = (f == 2); break; /* 점프 + 대시 */
    }
    (void)jumpWasHeld;
}

static void to_input(const Key *k, Input *in)
{
    memset(in, 0, sizeof(*in));
    in->left = k->l; in->right = k->r;
    in->jumpHeld = k->jh; in->jumpPressed = k->jp;
    in->dashPressed = k->dp; in->toggleAbility = k->tog;
    in->anyKey = 0;
}

static float score_of(const World *w)
{
    float s = w->p.x;
    if (w->state == ST_WON) s += 100000.0f;
    if (w->hasDisk) s += 400.0f;
    /* 파동과의 거리도 조금 본다 — 바짝 붙어 있으면 다음 수가 없다 */
    s += 0.05f * (w->p.x - w->formatX);
    return s;
}

static Node *cand;
static int cmp(const void *x, const void *y)
{
    float sa = cand[*(const int *)x].score, sb = cand[*(const int *)y].score;
    return (sa < sb) - (sa > sb);
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: rec-plan level frames mask [lead]\n"); return 1; }
    int lvl = atoi(argv[1]), frames = atoi(argv[2]);
    unsigned mask = (unsigned)strtoul(argv[3], 0, 0);
    int lead = (argc > 4) ? atoi(argv[4]) : 60;

    static Tune tune; tune_defaults(&tune); T = &tune;
    static World w0;
    sim_new_game(&w0, &tune);
    for (int i = 0; i < lvl && i < LEVEL_COUNT; i++) w0.unlocked |= LEVELS[i].reward;
    w0.unlocked |= mask;
    sim_load(&w0, &tune, lvl);

    static Key script[MAXF];
    int n = 0;

    /* 1) 터미널 위: 원하는 로드아웃이 될 때까지 하나씩 토글 */
    for (int f = 0; f < lead && n < MAXF; f++) {
        Key k; memset(&k, 0, sizeof(k)); k.tog = -1;
        /* 스폰 자리는 터미널 바로 왼쪽이다 — 올라설 때까지 걷는다 */
        if (!w0.atTerminal) k.r = 1;
        else if ((f % 12) == 0) {
            for (int a = 0; a < AB_COUNT; a++) {
                int want = (mask >> a) & 1;
                if (w0.p.has[a] != want && sim_ability_available(&w0, a)) { k.tog = a; break; }
            }
        }
        Input in; to_input(&k, &in);
        sim_step(&w0, &tune, &in, 1.0f / 60.0f);
        script[n++] = k;
    }
    for (int a = 0; a < AB_COUNT; a++)
        if (w0.p.has[a] != (int)((mask >> a) & 1))
            fprintf(stderr, "warn: ability %d not set as requested (has=%d)\n", a, w0.p.has[a]);

    /* 2) 빔 탐색 */
    int steps = (frames - lead) / CHUNK;
    Hist *hist = malloc(sizeof(Hist) * (size_t)(BEAM * NACT * (steps + 1) + 1));
    Node *beam = malloc(sizeof(Node) * BEAM);
    cand = malloc(sizeof(Node) * BEAM * NACT);
    int *order = malloc(sizeof(int) * BEAM * NACT);
    if (!hist || !beam || !cand || !order) { fprintf(stderr, "oom\n"); return 1; }
    int histN = 0;
    hist[histN].prev = -1; hist[histN].act = -1;
    beam[0].w = w0; beam[0].hist = histN++; beam[0].score = score_of(&w0);
    int beamN = 1;

    for (int s = 0; s < steps; s++) {
        int cn = 0;
        for (int b = 0; b < beamN; b++) {
            if (beam[b].w.state == ST_WON) { cand[cn++] = beam[b]; continue; }
            for (int a = 0; a < NACT; a++) {
                Node *nd = &cand[cn];
                nd->w = beam[b].w;
                int dead = 0;
                for (int f = 0; f < CHUNK; f++) {
                    Key k; act_keys(a, f, 0, &k);
                    Input in; to_input(&k, &in);
                    sim_step(&nd->w, &tune, &in, 1.0f / 60.0f);
                    if (nd->w.deathCause != DEATH_NONE) { dead = 1; break; }
                    if (nd->w.state == ST_WON) break;
                }
                if (dead) continue;
                hist[histN].prev = beam[b].hist; hist[histN].act = a;
                nd->hist = histN++;
                nd->score = score_of(&nd->w);
                cn++;
            }
        }
        if (cn == 0) { fprintf(stderr, "all branches died at step %d\n", s); break; }
        for (int i = 0; i < cn; i++) order[i] = i;
        qsort(order, (size_t)cn, sizeof(int), cmp);
        /* 같은 칸에 몰리지 않게 위치로 거른다 */
        static Node next[BEAM]; int nn = 0;
        for (int i = 0; i < cn && nn < BEAM; i++) {
            const Player *p = &cand[order[i]].w.p;
            int dup = 0;
            for (int j = 0; j < nn && !dup; j++) {
                const Player *q = &next[j].w.p;
                if ((int)(p->x / 3) == (int)(q->x / 3) && (int)(p->y / 3) == (int)(q->y / 3)
                    && (int)(p->vy / 40) == (int)(q->vy / 40) && p->airJumps == q->airJumps
                    && p->dashUsed == q->dashUsed) dup = 1;
            }
            if (!dup) next[nn++] = cand[order[i]];
        }
        memcpy(beam, next, sizeof(Node) * (size_t)nn); beamN = nn;
        if (beam[0].w.state == ST_WON) break;
    }
    const Node *bestN = &beam[0];

    /* 3) 거꾸로 따라가서 행동 열을 복원 */
    static int chain[MAXF]; int cl = 0;
    for (int i = bestN->hist; i >= 0 && hist[i].act >= 0; i = hist[i].prev) chain[cl++] = hist[i].act;
    for (int i = cl - 1; i >= 0 && n < MAXF; i--)
        for (int f = 0; f < CHUNK && n < MAXF; f++) act_keys(chain[i], f, 0, &script[n++]);

    fprintf(stderr, "level %d (%s %s): x=%.0f/%d  won=%d disk=%d frames=%d\n",
            lvl, LEVELS[lvl].id, LEVELS[lvl].file, bestN->w.p.x,
            LEVELS[lvl].width * TILE, bestN->w.state == ST_WON, bestN->w.hasDisk, n);

    for (int i = 0; i < n; i++)
        printf("%d %d %d %d %d %d\n", script[i].r, script[i].l, script[i].jh,
               script[i].jp, script[i].dp, script[i].tog);
    return 0;
}
