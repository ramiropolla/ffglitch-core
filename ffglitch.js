
import * as std from "std";

function usage(msg)
{
    if ( msg )
        print(msg);
    print("usage: ./qjs " + scriptArgs[0] + " [-p] -i <input.json> -s <script.js> -o <output.json>");
    return 1;
}

//---------------------------------------------------------------------
// no lf, no space
const pflags_def = [ [ false, false ] ];

function get_pflags(jso)
{
    if ( jso === "fcode" || jso === "bcode" )
    {
        return [
            [ true, false ]
        ];
    }
    else if ( jso === "forward" || jso === "backward" )
    {
        return [
            [ false, false ],
            [ true, false ],
            [ true, true ]
        ];
    }
    return pflags_def;
}

function print_lf(o_file, level)
{
    o_file.puts('\n');
    o_file.printf("%*s", level * 2, "");
}

function print_space(o_file)
{
    o_file.puts(' ');
}

function output_lf(o_file, pflags, level)
{
    const json_pflags_no_lf = pflags[0];
    const json_pflags_no_space = pflags[1];
    if ( !json_pflags_no_lf )
        print_lf(o_file, level);
    else if ( !json_pflags_no_space )
        print_space(o_file);
}

function json_print_element(o_file, jso, pflags, level)
{
    if ( typeof jso === "object" )
    {
        if ( jso === null )
        {
            o_file.puts("null");
        }
        else if ( Array.isArray(jso) )
        {
            const pflags0 = pflags[0];
            const pflags1 = pflags.slice(1);
            o_file.puts('[');
            for ( let i = 0; i < jso.length; i++ )
            {
                if ( i != 0 )
                    o_file.puts(',');
                output_lf(o_file, pflags0, level+1);
                json_print_element(o_file, jso[i], pflags1, level+1);
            }
            output_lf(o_file, pflags0, level);
            o_file.puts(']');
        }
        else
        {
            let i = 0;
            o_file.puts('{');
            for ( const key in jso )
            {
                if ( i++ != 0 )
                    o_file.puts(',');
                print_lf(o_file, level+1);
                o_file.puts(JSON.stringify(key));
                o_file.puts(':');
                json_print_element(o_file, jso[key], get_pflags(key), level+1);
            }
            print_lf(o_file, level);
            o_file.puts('}');
        }
    }
    else
    {
        o_file.puts(JSON.stringify(jso));
    }
}

function json_fputs(o_file, jso)
{
    json_print_element(o_file, jso, pflags_def, 0);
    o_file.puts('\n');
}
///////////////////////////////////////////////////////////////////////

// global variables for the script
var json_stream = null;
var json_frame = null;

function run_script(s_path, i_path, o_path, do_pretty)
{
    // load input JSON
    print("[+] loading input JSON file '" + i_path + "'")
    let i_file = std.open(i_path, "r");
    if ( !i_file )
        return usage("could not open input file '" + i_path + "'");
    let str = i_file.readAsString();
    let json_root = JSON.parse(str);
    if ( !json_root )
        return usage("error parsing input file '" + i_path + "'");
    i_file.close();

    // will error out on exception
    print("[+] running script '" + s_path + "' on JSON data")
    std.loadScript(s_path);

    // for each stream (normally there is only one stream of interest)
    let streams = json_root["streams"];
    for ( let i = 0; i < streams.length; i++ )
    {
        let stream = streams[i];
        json_stream = stream;
        // for each frame in the stream
        let frames = stream["frames"];
        for ( let j = 0; j < frames.length; j++ )
        {
            let frame = frames[j];
            json_frame = frame;
            glitch_frame(frame);
        }
    }

    // open input JSON
    print("[+] writing " + (do_pretty ? "prettified " : "") + "output JSON file '" + o_path + "'")
    let o_file = std.open(o_path, "w+");
    if ( !o_file )
        return usage("could not open output file '" + o_path + "'");
    if ( do_pretty )
        json_fputs(o_file, json_root);
    else
        o_file.puts(JSON.stringify(json_root));
    o_file.close();

    return 0;
}

function main(argc, argv)
{
    let i_path = null;
    let o_path = null;
    let s_path = null;
    let do_pretty = false;
    for ( let i = 1; i < argc; )
    {
        let opt = argv[i++];
        if ( opt == "-i" )
        {
            if ( i == argc )
                return usage("parameter missing for option '-i'");
            i_path = argv[i++];
        }
        else if ( opt == "-o" )
        {
            if ( i == argc )
                return usage("parameter missing for option '-o'");
            o_path = argv[i++];
        }
        else if ( opt == "-s" )
        {
            if ( i == argc )
                return usage("parameter missing for option '-s'");
            s_path = argv[i++];
        }
        else if ( opt == "-p" )
        {
            do_pretty = true;
        }
        else
        {
            return usage("unknown option '" + opt + "'");
        }
    }
    if ( !s_path || !i_path || !o_path )
        return usage();
    return run_script(s_path, i_path, o_path, do_pretty);
}

main(scriptArgs.length, scriptArgs, this);
