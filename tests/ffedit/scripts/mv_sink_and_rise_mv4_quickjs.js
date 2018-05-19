export function glitch_frame(frame)
{
    // bail out if we have no motion vectors
    let mvs = frame["mv"];
    if ( !mvs )
        return;
    // bail out if we have no forward motion vectors
    let fwd_mvs = mvs["forward"];
    if ( !fwd_mvs )
        return;

    // clear horizontal element of all motion vectors
    const ilen = fwd_mvs.length;
    for ( let i = 0; i < ilen; i++ )
    {
        let row = fwd_mvs[i];
        const jlen = row.length;
        for ( let j = 0; j < jlen; j++ )
        {
            let mv = row[j];
            if ( mv instanceof Array )
            {
                const klen = mv.length;
                for ( let k = 0; k < klen; k++ )
                    mv[k][0] = 0;
            }
            else if ( mv !== null )
            {
                mv[0] = 0;
            }
        }
    }
}
