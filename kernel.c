/*
 * kernel.c — Tic-Tac-Toe OS
 * ------------------------------------------------------------
 * A minimal x86 kernel whose entire purpose is to play tic-tac-toe.
 * No libc, no underlying OS — this code runs directly on the CPU in
 * 32-bit protected mode, put there by GRUB via the Multiboot
 * header in boot.S. It talks to hardware directly:
 *
 *   - VGA text-mode buffer (0xB8000) for the screen
 *   - PS/2 keyboard controller (I/O ports 0x60/0x64) for input
 *
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

/* ================== Low-level I/O ================== */

static inline u8 inb(u16 port)
{
    u8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(u16 port, u8 val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ================== VGA text-mode driver ================== */

#define VGA_ADDR ((volatile u16 *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

enum
{
    VGA_BLACK = 0,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LGREY,
    VGA_DGREY,
    VGA_LBLUE,
    VGA_LGREEN,
    VGA_LCYAN,
    VGA_LRED,
    VGA_LMAGENTA,
    VGA_YELLOW,
    VGA_WHITE
};

static inline u8 vga_color(u8 fg, u8 bg) { return (u8)(fg | (bg << 4)); }
static inline u16 vga_entry(char c, u8 color) { return (u16)c | ((u16)color << 8); }

static void vga_clear(u8 color)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_ADDR[i] = vga_entry(' ', color);
}

static void vga_putc(int x, int y, char c, u8 color)
{
    if (x < 0 || x >= VGA_COLS || y < 0 || y >= VGA_ROWS)
        return;
    VGA_ADDR[y * VGA_COLS + x] = vga_entry(c, color);
}

static void vga_puts(int x, int y, const char *s, u8 color)
{
    for (int i = 0; s[i] != '\0'; i++)
        vga_putc(x + i, y, s[i], color);
}

/* clears one row, then writes text into it (handy for status lines) */
static void vga_line(int y, const char *s, u8 color, u8 bg_color)
{
    for (int x = 0; x < VGA_COLS; x++)
        VGA_ADDR[y * VGA_COLS + x] = vga_entry(' ', bg_color);
    vga_puts(2, y, s, color);
}

/* CP437 box-drawing characters, used for the grid */
#define CH_HORIZ 0xC4
#define CH_VERT 0xB3
#define CH_CROSS 0xC5

/* ================== PS/2 keyboard driver (polling) ================== */

/* Set-1 scancodes we care about. Anything with bit 0x80 set is a
 * "key released" event, which we ignore — we only act on key-down. */
#define SC_1 0x02
#define SC_9 0x0A
#define SC_R 0x13
#define SC_ESC 0x01

static u32 entropy = 2463534242u; /* xorshift32 seed */

/* Blocks until a key is pressed (make code), returns its scancode.
 * Spins on the PS/2 status port; each idle spin perturbs our PRNG
 * state, so the exact timing of human keypresses feeds randomness
 * into EASY-mode move selection (no RTC/timer driver needed). */
static u8 kb_wait_key(void)
{
    for (;;)
    {
        while (!(inb(0x64) & 1))
        {
            entropy ^= entropy << 13;
            entropy ^= entropy >> 17;
            entropy ^= entropy << 5;
        }
        u8 sc = inb(0x60);
        if (sc & 0x80)
            continue; /* key release, ignore */
        return sc;
    }
}

static u32 rng_next(void)
{
    entropy ^= entropy << 13;
    entropy ^= entropy >> 17;
    entropy ^= entropy << 5;
    return entropy;
}

/* ================== Game kernel: rules + minimax AI ================== */

#define EMPTY 0
#define PLAYER_X 1
#define PLAYER_O 2
#define DRAW 3
#define NONE 0
#define HUMAN PLAYER_X
#define AI PLAYER_O

static int board[9];

static const int WIN_LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

static int check_winner(void)
{
    for (int i = 0; i < 8; i++)
    {
        int a = WIN_LINES[i][0], b = WIN_LINES[i][1], c = WIN_LINES[i][2];
        if (board[a] != EMPTY && board[a] == board[b] && board[b] == board[c])
            return board[a];
    }
    for (int i = 0; i < 9; i++)
        if (board[i] == EMPTY)
            return NONE;
    return DRAW;
}

static void reset_board(void)
{
    for (int i = 0; i < 9; i++)
        board[i] = EMPTY;
}

static int opponent_of(int p) { return p == PLAYER_X ? PLAYER_O : PLAYER_X; }

static int minimax(int ai, int maximizing, int depth, int max_depth)
{
    int result = check_winner();
    if (result == ai)
        return 10 - depth;
    if (result == opponent_of(ai))
        return depth - 10;
    if (result == DRAW)
        return 0;
    if (max_depth >= 0 && depth >= max_depth)
        return 0;

    int current = maximizing ? ai : opponent_of(ai);
    int best = maximizing ? -1000 : 1000;
    for (int i = 0; i < 9; i++)
    {
        if (board[i] != EMPTY)
            continue;
        board[i] = current;
        int score = minimax(ai, !maximizing, depth + 1, max_depth);
        board[i] = EMPTY;
        if (maximizing)
        {
            if (score > best)
                best = score;
        }
        else
        {
            if (score < best)
                best = score;
        }
    }
    return best;
}

static int best_move(int ai, int max_depth)
{
    int move = -1, best = -1000;
    for (int i = 0; i < 9; i++)
    {
        if (board[i] != EMPTY)
            continue;
        board[i] = ai;
        int score = minimax(ai, 0, 1, max_depth);
        board[i] = EMPTY;
        if (score > best)
        {
            best = score;
            move = i;
        }
    }
    return move;
}

static int random_empty_cell(void)
{
    int empties[9], n = 0;
    for (int i = 0; i < 9; i++)
        if (board[i] == EMPTY)
            empties[n++] = i;
    if (n == 0)
        return -1;
    return empties[rng_next() % (u32)n];
}

/* difficulty: 0=EASY 1=MEDIUM 2=HARD */
static int ai_move(int player, int difficulty)
{
    if (check_winner() != NONE)
        return -1;
    int pos;
    switch (difficulty)
    {
    case 0:
        pos = (rng_next() % 100 < 70) ? random_empty_cell() : best_move(player, 2);
        break;
    case 1:
        pos = best_move(player, 4);
        break;
    default:
        pos = best_move(player, -1);
        break;
    }
    if (pos < 0)
        pos = random_empty_cell();
    if (pos >= 0)
        board[pos] = player;
    return pos;
}

/* ================== Rendering ================== */

#define BOARD_X 30
#define BOARD_Y 6
/* cell width 4, cell height 2 -> grid spans 3 cells */

static void draw_frame(const char *difficulty_name)
{
    u8 bg = vga_color(VGA_LGREY, VGA_BLUE);
    vga_line(0, " TIC-TAC-TOE OS", vga_color(VGA_WHITE, VGA_BLUE), bg);
    char line[40] = "kernel: minimax  difficulty: ";
    int len = 0;
    while (line[len])
        len++;
    for (int i = 0; difficulty_name[i]; i++)
        line[len++] = difficulty_name[i];
    line[len] = '\0';
    vga_line(1, line, vga_color(VGA_LCYAN, VGA_BLACK), vga_color(VGA_LGREY, VGA_BLACK));
}

static void draw_board(void)
{
    u8 grid_color = vga_color(VGA_DGREY, VGA_BLACK);
    /* horizontal separators */
    for (int row = 0; row < 2; row++)
    {
        int y = BOARD_Y + row * 2 + 1;
        for (int x = 0; x < 11; x++)
        {
            char c = (x == 3 || x == 7) ? CH_CROSS : CH_HORIZ;
            vga_putc(BOARD_X + x, y, c, grid_color);
        }
    }
    /* vertical separators */
    for (int col = 0; col < 2; col++)
    {
        int x = BOARD_X + col * 4 + 3;
        for (int y = 0; y < 5; y++)
        {
            if (y == 1 || y == 3)
                continue; /* crossing handled above */
            vga_putc(x, BOARD_Y + y, CH_VERT, grid_color);
        }
    }
    /* marks */
    for (int i = 0; i < 9; i++)
    {
        int r = i / 3, c = i % 3;
        int cx = BOARD_X + c * 4 + 1;
        int cy = BOARD_Y + r * 2;
        char ch = board[i] == PLAYER_X ? 'X' : board[i] == PLAYER_O ? 'O'
                                                                    : ('1' + i);
        u8 color = board[i] == PLAYER_X   ? vga_color(VGA_LGREEN, VGA_BLACK)
                   : board[i] == PLAYER_O ? vga_color(VGA_LRED, VGA_BLACK)
                                          : vga_color(VGA_DGREY, VGA_BLACK);
        vga_putc(cx, cy, ch, color);
    }
}

/* ================== Kernel entry ================== */

void kmain(void)
{
    __asm__ volatile("cli"); /* disable interrupts (we don't have an IDT yet) */
    vga_clear(vga_color(VGA_LGREY, VGA_BLACK));

    /* --- boot / difficulty screen --- */
    vga_puts(2, 2, "TIC-TAC-TOE", vga_color(VGA_WHITE, VGA_BLACK));
    vga_puts(2, 6, "select opponent difficulty:", vga_color(VGA_LGREY, VGA_BLACK));
    vga_puts(4, 8, "1  EASY    -- mostly random moves", vga_color(VGA_LGREEN, VGA_BLACK));
    vga_puts(4, 9, "2  MEDIUM  -- looks 4 moves ahead", vga_color(VGA_YELLOW, VGA_BLACK));
    vga_puts(4, 10, "3  HARD    -- perfect play, cannot be beaten", vga_color(VGA_LRED, VGA_BLACK));
    vga_puts(2, 13, "press 1, 2, or 3", vga_color(VGA_DGREY, VGA_BLACK));

    int difficulty;
    const char *diff_name;
    for (;;)
    {
        u8 sc = kb_wait_key();
        if (sc == SC_1)
        {
            difficulty = 0;
            diff_name = "EASY";
            break;
        }
        if (sc == SC_1 + 1)
        {
            difficulty = 1;
            diff_name = "MEDIUM";
            break;
        }
        if (sc == SC_1 + 2)
        {
            difficulty = 2;
            diff_name = "HARD";
            break;
        }
    }

    for (;;)
    { /* one iteration per game; loops forever so you can replay */
        reset_board();
        vga_clear(vga_color(VGA_LGREY, VGA_BLACK));
        draw_frame(diff_name);

        vga_puts(2, 4, "you are X. press 1-9 to place a mark:", vga_color(VGA_LGREY, VGA_BLACK));
        vga_puts(2, 20, " 1 | 2 | 3      (keys map to cells in", vga_color(VGA_DGREY, VGA_BLACK));
        vga_puts(2, 21, " 4 | 5 | 6       reading order, left-to-right,", vga_color(VGA_DGREY, VGA_BLACK));
        vga_puts(2, 22, " 7 | 8 | 9       top-to-bottom)", vga_color(VGA_DGREY, VGA_BLACK));

        int turn = HUMAN;
        int result = NONE;

        while (result == NONE)
        {
            draw_board();
            vga_line(17, turn == HUMAN ? "your move" : "kernel is thinking...",
                     vga_color(turn == HUMAN ? VGA_LGREEN : VGA_LRED, VGA_BLACK),
                     vga_color(VGA_LGREY, VGA_BLACK));

            if (turn == HUMAN)
            {
                for (;;)
                {
                    u8 sc = kb_wait_key();
                    if (sc >= SC_1 && sc <= SC_9)
                    {
                        int pos = sc - SC_1;
                        if (board[pos] == EMPTY)
                        {
                            board[pos] = HUMAN;
                            break;
                        }
                    }
                }
            }
            else
            {
                ai_move(AI, difficulty);
            }

            result = check_winner();
            turn = opponent_of(turn);
        }

        draw_board();
        if (result == DRAW)
            vga_line(17, "draw. press R to play again, ESC to halt", vga_color(VGA_YELLOW, VGA_BLACK), vga_color(VGA_LGREY, VGA_BLACK));
        else if (result == HUMAN)
            vga_line(17, "you win! press R to play again, ESC to halt", vga_color(VGA_LGREEN, VGA_BLACK), vga_color(VGA_LGREY, VGA_BLACK));
        else
            vga_line(17, "kernel wins. press R to play again, ESC to halt", vga_color(VGA_LRED, VGA_BLACK), vga_color(VGA_LGREY, VGA_BLACK));

        for (;;)
        {
            u8 sc = kb_wait_key();
            if (sc == SC_R)
                break; /* replay: fall out to outer loop */
            if (sc == SC_ESC)
            {
                vga_clear(vga_color(VGA_LGREY, VGA_BLACK));
                vga_puts(30, 12, "system halted.", vga_color(VGA_WHITE, VGA_BLACK));
                __asm__ volatile("cli");
                for (;;)
                    __asm__ volatile("hlt");
            }
        }
    }
}