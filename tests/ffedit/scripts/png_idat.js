const PNG_FILTER_VALUE_NONE  = 0;
const PNG_FILTER_VALUE_SUB   = 1;
const PNG_FILTER_VALUE_UP    = 2;
const PNG_FILTER_VALUE_AVG   = 3;
const PNG_FILTER_VALUE_PAETH = 4;
export function setup(args)
{
  args.features = [ "idat" ];
}
export function glitch_frame(frame, stream)
{
  const rows = frame.idat?.rows;
  if ( !rows )
    return;
  const length = rows.length;
  for ( let i = 0; i < length; i++ )
  {
    const row = rows[i];
    row[0] = PNG_FILTER_VALUE_PAETH;
  }
}
