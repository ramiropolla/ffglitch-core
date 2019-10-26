#define init_put_bits            AV_JOIN(PB_PREFIX, init_put_bits)
#define put_bits_count           AV_JOIN(PB_PREFIX, put_bits_count)
#define rebase_put_bits          AV_JOIN(PB_PREFIX, rebase_put_bits)
#define put_bits_left            AV_JOIN(PB_PREFIX, put_bits_left)
#define flush_put_bits           AV_JOIN(PB_PREFIX, flush_put_bits)
#define flush_put_bits_le        AV_JOIN(PB_PREFIX, flush_put_bits_le)
#define put_bits                 AV_JOIN(PB_PREFIX, put_bits)
#define put_bits_le              AV_JOIN(PB_PREFIX, put_bits_le)
#define put_sbits                AV_JOIN(PB_PREFIX, put_sbits)
#define put_bits32               AV_JOIN(PB_PREFIX, put_bits32)
#define put_bits64               AV_JOIN(PB_PREFIX, put_bits64)
#define put_bits_ptr             AV_JOIN(PB_PREFIX, put_bits_ptr)
#define skip_put_bytes           AV_JOIN(PB_PREFIX, skip_put_bytes)
#define skip_put_bits            AV_JOIN(PB_PREFIX, skip_put_bits)
#define set_put_bits_buffer_size AV_JOIN(PB_PREFIX, set_put_bits_buffer_size)

void init_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size);
int put_bits_count(PutBitContext *s);
void rebase_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size);
int put_bits_left(PutBitContext* s);
void flush_put_bits(PutBitContext *s);
void flush_put_bits_le(PutBitContext *s);
void put_bits(PutBitContext *s, int n, unsigned int value);
void put_bits_le(PutBitContext *s, int n, unsigned int value);
void put_sbits(PutBitContext *pb, int n, int32_t value);
void put_bits32(PutBitContext *s, uint32_t value);
void put_bits64(PutBitContext *s, int n, uint64_t value);
uint8_t *put_bits_ptr(PutBitContext *s);
void skip_put_bytes(PutBitContext *s, int n);
void skip_put_bits(PutBitContext *s, int n);
void set_put_bits_buffer_size(PutBitContext *s, int size);
