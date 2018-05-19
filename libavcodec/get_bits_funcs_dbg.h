#define get_bits_count        AV_JOIN3(dbg_, GB_PREFIX, get_bits_count)
#define get_xbits             AV_JOIN3(dbg_, GB_PREFIX, get_xbits)
#define get_xbits_le          AV_JOIN3(dbg_, GB_PREFIX, get_xbits_le)
#define get_sbits             AV_JOIN3(dbg_, GB_PREFIX, get_sbits)
#define get_bits              AV_JOIN3(dbg_, GB_PREFIX, get_bits)
#define get_bitsz             AV_JOIN3(dbg_, GB_PREFIX, get_bitsz)
#define get_bits_le           AV_JOIN3(dbg_, GB_PREFIX, get_bits_le)
#define show_bits             AV_JOIN3(dbg_, GB_PREFIX, show_bits)
#define skip_bits             AV_JOIN3(dbg_, GB_PREFIX, skip_bits)
#define get_bits1             AV_JOIN3(dbg_, GB_PREFIX, get_bits1)
#define show_bits1            AV_JOIN3(dbg_, GB_PREFIX, show_bits1)
#define skip_bits1            AV_JOIN3(dbg_, GB_PREFIX, skip_bits1)
#define get_bits_long         AV_JOIN3(dbg_, GB_PREFIX, get_bits_long)
#define skip_bits_long        AV_JOIN3(dbg_, GB_PREFIX, skip_bits_long)
#define get_bits64            AV_JOIN3(dbg_, GB_PREFIX, get_bits64)
#define get_sbits_long        AV_JOIN3(dbg_, GB_PREFIX, get_sbits_long)
#define show_bits_long        AV_JOIN3(dbg_, GB_PREFIX, show_bits_long)
#define check_marker          AV_JOIN3(dbg_, GB_PREFIX, check_marker)
#define init_get_bits         AV_JOIN3(dbg_, GB_PREFIX, init_get_bits)
#define init_get_bits8        AV_JOIN3(dbg_, GB_PREFIX, init_get_bits8)
#define init_get_bits8_le     AV_JOIN3(dbg_, GB_PREFIX, init_get_bits8_le)
#define align_get_bits        AV_JOIN3(dbg_, GB_PREFIX, align_get_bits)
#define get_vlc2              AV_JOIN3(dbg_, GB_PREFIX, get_vlc2)
#define get_rl_vlc2           AV_JOIN3(dbg_, GB_PREFIX, get_rl_vlc2)
#define get_cfhd_rl_vlc       AV_JOIN3(dbg_, GB_PREFIX, get_cfhd_rl_vlc)
#define decode012             AV_JOIN3(dbg_, GB_PREFIX, decode012)
#define decode210             AV_JOIN3(dbg_, GB_PREFIX, decode210)
#define get_bits_left         AV_JOIN3(dbg_, GB_PREFIX, get_bits_left)
#define skip_1stop_8data_bits AV_JOIN3(dbg_, GB_PREFIX, skip_1stop_8data_bits)

int get_bits_count(const GetBitContext *s, const char *file, int line, const char *func);
void skip_bits_long(GetBitContext *s, int n, const char *file, int line, const char *func);
int get_xbits(GetBitContext *s, int n, const char *file, int line, const char *func);
int get_xbits_le(GetBitContext *s, int n, const char *file, int line, const char *func);
int get_sbits(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int get_bits(GetBitContext *s, int n, const char *file, int line, const char *func);
int get_bitsz(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int get_bits_le(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int show_bits(GetBitContext *s, int n, const char *file, int line, const char *func);
void skip_bits(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int get_bits1(GetBitContext *s, const char *file, int line, const char *func);
unsigned int show_bits1(GetBitContext *s, const char *file, int line, const char *func);
void skip_bits1(GetBitContext *s, const char *file, int line, const char *func);
unsigned int get_bits_long(GetBitContext *s, int n, const char *file, int line, const char *func);
uint64_t get_bits64(GetBitContext *s, int n, const char *file, int line, const char *func);
int get_sbits_long(GetBitContext *s, int n, const char *file, int line, const char *func);
unsigned int show_bits_long(GetBitContext *s, int n, const char *file, int line, const char *func);
int check_marker(void *logctx, GetBitContext *s, const char *msg, const char *file, int line, const char *func);
int init_get_bits(GetBitContext *s, const uint8_t *buffer, int bit_size, const char *file, int line, const char *func);
int init_get_bits8(GetBitContext *s, const uint8_t *buffer, int byte_size, const char *file, int line, const char *func);
int init_get_bits8_le(GetBitContext *s, const uint8_t *buffer, int byte_size, const char *file, int line, const char *func);
const uint8_t *align_get_bits(GetBitContext *s, const char *file, int line, const char *func);
int get_vlc2(GetBitContext *s, VLC_TYPE (*table)[2], int bits, int max_depth, const char *file, int line, const char *func);
void get_rl_vlc2(
        int *plevel,
        int *prun,
        GetBitContext *s,
        RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update,
        const char *file,
        int line,
        const char *func);
void get_cfhd_rl_vlc(
        int *plevel,
        int *prun,
        GetBitContext *s,
        CFHD_RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update,
        const char *file,
        int line,
        const char *func);
int decode012(GetBitContext *gb, const char *file, int line, const char *func);
int decode210(GetBitContext *gb, const char *file, int line, const char *func);
int get_bits_left(GetBitContext *gb, const char *file, int line, const char *func);
int skip_1stop_8data_bits(GetBitContext *gb, const char *file, int line, const char *func);
