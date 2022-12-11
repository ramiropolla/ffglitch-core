let global_mb_types;

const CANDIDATE_MB_TYPE_INTRA   = (1 <<  0);
const CANDIDATE_MB_TYPE_INTER   = (1 <<  1);
const CANDIDATE_MB_TYPE_INTER4V = (1 <<  2);
const CANDIDATE_MB_TYPE_INTER_I = (1 <<  8);

let frame_num = 0;
export function mb_type_func(args)
{
  const mb_types = args.mb_types;
  const mb_height = mb_types.length;
  const mb_width = mb_types[0].length;

  /* start with an array of all P frames */
  if ( global_mb_types === undefined )
  {
    global_mb_types = new Array(mb_height);
    for ( let mb_y = 0; mb_y < mb_height; mb_y++ )
    {
      let mb_row = new Int32Array(mb_width);
      mb_row.fill(CANDIDATE_MB_TYPE_INTER);
      global_mb_types[mb_y] = mb_row;
    }
  }

  /* copy our global mb_types to args */
  for ( let mb_y = 0; mb_y < mb_height; mb_y++ )
    for ( let mb_x = 0; mb_x < mb_width; mb_x++ )
      mb_types[mb_y][mb_x] = global_mb_types[mb_y][mb_x];

  /* make some mbs I */
  for ( let mb_y = 0; mb_y < mb_height && mb_y < frame_num; mb_y++ )
    for ( let mb_x = 0; mb_x < mb_width && mb_x < frame_num; mb_x++ )
      global_mb_types[mb_y][mb_x] = CANDIDATE_MB_TYPE_INTRA;

  frame_num++;
}
