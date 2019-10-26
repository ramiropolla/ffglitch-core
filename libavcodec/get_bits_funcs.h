#define get_bits_count        AV_JOIN(GB_PREFIX, get_bits_count)
#define get_xbits             AV_JOIN(GB_PREFIX, get_xbits)
#define get_xbits_le          AV_JOIN(GB_PREFIX, get_xbits_le)
#define get_sbits             AV_JOIN(GB_PREFIX, get_sbits)
#define get_bits              AV_JOIN(GB_PREFIX, get_bits)
#define get_bitsz             AV_JOIN(GB_PREFIX, get_bitsz)
#define get_bits_le           AV_JOIN(GB_PREFIX, get_bits_le)
#define show_bits             AV_JOIN(GB_PREFIX, show_bits)
#define skip_bits             AV_JOIN(GB_PREFIX, skip_bits)
#define get_bits1             AV_JOIN(GB_PREFIX, get_bits1)
#define show_bits1            AV_JOIN(GB_PREFIX, show_bits1)
#define skip_bits1            AV_JOIN(GB_PREFIX, skip_bits1)
#define get_bits_long         AV_JOIN(GB_PREFIX, get_bits_long)
#define skip_bits_long        AV_JOIN(GB_PREFIX, skip_bits_long)
#define get_bits64            AV_JOIN(GB_PREFIX, get_bits64)
#define get_sbits_long        AV_JOIN(GB_PREFIX, get_sbits_long)
#define show_bits_long        AV_JOIN(GB_PREFIX, show_bits_long)
#define check_marker          AV_JOIN(GB_PREFIX, check_marker)
#define init_get_bits         AV_JOIN(GB_PREFIX, init_get_bits)
#define init_get_bits8        AV_JOIN(GB_PREFIX, init_get_bits8)
#define init_get_bits8_le     AV_JOIN(GB_PREFIX, init_get_bits8_le)
#define align_get_bits        AV_JOIN(GB_PREFIX, align_get_bits)
#define get_vlc2              AV_JOIN(GB_PREFIX, get_vlc2)
#define get_rl_vlc2           AV_JOIN(GB_PREFIX, get_rl_vlc2)
#define get_cfhd_rl_vlc       AV_JOIN(GB_PREFIX, get_cfhd_rl_vlc)
#define decode012             AV_JOIN(GB_PREFIX, decode012)
#define decode210             AV_JOIN(GB_PREFIX, decode210)
#define get_bits_left         AV_JOIN(GB_PREFIX, get_bits_left)
#define skip_1stop_8data_bits AV_JOIN(GB_PREFIX, skip_1stop_8data_bits)

int get_bits_count(const GetBitContext *s);
void skip_bits_long(GetBitContext *s, int n);
int get_xbits(GetBitContext *s, int n);
int get_xbits_le(GetBitContext *s, int n);
int get_sbits(GetBitContext *s, int n);
unsigned int get_bits(GetBitContext *s, int n);
int get_bitsz(GetBitContext *s, int n);
unsigned int get_bits_le(GetBitContext *s, int n);
unsigned int show_bits(GetBitContext *s, int n);
void skip_bits(GetBitContext *s, int n);
unsigned int get_bits1(GetBitContext *s);
unsigned int show_bits1(GetBitContext *s);
void skip_bits1(GetBitContext *s);
unsigned int get_bits_long(GetBitContext *s, int n);
uint64_t get_bits64(GetBitContext *s, int n);
int get_sbits_long(GetBitContext *s, int n);
unsigned int show_bits_long(GetBitContext *s, int n);
int check_marker(void *logctx, GetBitContext *s, const char *msg);
int init_get_bits(GetBitContext *s, const uint8_t *buffer, int bit_size);
int init_get_bits8(GetBitContext *s, const uint8_t *buffer, int byte_size);
int init_get_bits8_le(GetBitContext *s, const uint8_t *buffer, int byte_size);
const uint8_t *align_get_bits(GetBitContext *s);
int get_vlc2(GetBitContext *s, VLC_TYPE (*table)[2], int bits, int max_depth);
void get_rl_vlc2(
        int *plevel,
        int *prun,
        GetBitContext *s,
        RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update);
void get_cfhd_rl_vlc(
        int *plevel,
        int *prun,
        GetBitContext *s,
        CFHD_RL_VLC_ELEM *table,
        int bits,
        int max_depth,
        int need_update);
int decode012(GetBitContext *gb);
int decode210(GetBitContext *gb);
int get_bits_left(GetBitContext *gb);
int skip_1stop_8data_bits(GetBitContext *gb);
