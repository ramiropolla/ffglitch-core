
/* This file is included by:
 * - ffedit_mpeg12.c
 */

//---------------------------------------------------------------------
static void
ffe_mpegvideo_jctx_init(MpegEncContext *s)
{
    /* set jctx */
    if ( s->avctx->ffedit_export != 0 )
    {
        /* create one jctx for each exported frame */
        AVFrame *f = s->current_picture_ptr->f;
        f->jctx = av_mallocz(sizeof(json_ctx_t));
        json_ctx_start(f->jctx, 1);
        s->jctx = f->jctx;
    }
    if ( s->jctx != NULL )
    {
        for ( size_t i = 0; i < MAX_THREADS; i++ )
            if ( s->thread_context[i] != NULL )
                s->thread_context[i]->jctx = json_ctx_start_thread(s->jctx, 1, i);
    }
}
