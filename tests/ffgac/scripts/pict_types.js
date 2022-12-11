let frame_num = 0;
export function pict_type_func(args)
{
  if ( (frame_num++ & 3) )
    return "P";
  return "I";
}
