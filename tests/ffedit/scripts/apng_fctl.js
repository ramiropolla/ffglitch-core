const APNG_DISPOSE_OP_NONE       = 0;
const APNG_DISPOSE_OP_BACKGROUND = 1;
const APNG_DISPOSE_OP_PREVIOUS   = 2;
const APNG_BLEND_OP_SOURCE = 0;
const APNG_BLEND_OP_OVER   = 1;
export function setup(args)
{
  args.features = [ "headers" ];
}
export function glitch_frame(frame, stream)
{
  const fctl = frame.headers.fcTL;
  if ( !fctl )
    return;
  fctl.dispose_op = APNG_DISPOSE_OP_NONE;
  fctl.blend_op   = APNG_BLEND_OP_OVER;
}
