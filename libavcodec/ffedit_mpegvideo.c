
/* This file is included by:
 * - ffedit_mpeg12dec.c
 * - ffedit_h263.c
 */

//---------------------------------------------------------------------
static void
ffe_mpegvideo_export_init(MpegEncContext *s)
{
    AVFrame *f = s->current_picture_ptr->f;
    memcpy(f->ffedit_sd, s->ffedit_sd, sizeof(f->ffedit_sd));

    /* set jctx */
    s->jctx = s->avctx->jctx;
    if ( s->jctx != NULL )
        for ( size_t i = 0; i < MAX_THREADS; i++ )
            if ( s->thread_context[i] != NULL )
                s->thread_context[i]->jctx = json_ctx_start_thread(s->jctx, i);
}
