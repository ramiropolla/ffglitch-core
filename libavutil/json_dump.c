/* Crappy JSON dumper
 * Copyright (c) 2018-2019 Ramiro Polla
 * MIT License
 */

#include "json.h"

//---------------------------------------------------------------------
static void output_lf(FILE *fp, json_t *jso, int level)
{
    if ( (jso->flags & JSON_PFLAGS_NO_LF) == 0 )
    {
        fprintf(fp, "\n");
        fprintf(fp, "%*s", level * 2, "");
    }
    else if ( (jso->flags & JSON_PFLAGS_NO_SPACE) == 0 )
    {
        fprintf(fp, " ");
    }
}

static void json_print_element(FILE *fp, json_t *jso, int level)
{
    if ( jso == NULL )
    {
        fprintf(fp, "null");
        return;
    }
    switch ( JSON_TYPE(jso->flags) )
    {
    case JSON_TYPE_OBJECT:
        fprintf(fp, "{");
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( i != 0 )
                fprintf(fp, ",");
            output_lf(fp, jso, level+1);
            fprintf(fp, "\"%s\"", jso->obj->names[i]);
            fprintf(fp, ":");
            json_print_element(fp, jso->obj->values[i], level+1);
        }
        output_lf(fp, jso, level);
        fprintf(fp, "}");
        break;
    case JSON_TYPE_ARRAY:
        fprintf(fp, "[");
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( i != 0 )
                fprintf(fp, ",");
            output_lf(fp, jso, level+1);
            json_print_element(fp, jso->array[i], level+1);
        }
        output_lf(fp, jso, level);
        fprintf(fp, "]");
        break;
    case JSON_TYPE_ARRAY_OF_INTS:
        fprintf(fp, "[");
        for ( size_t i = 0; i < JSON_LEN(jso->flags); i++ )
        {
            if ( i != 0 )
                fprintf(fp, ",");
            output_lf(fp, jso, level+1);
            if ( jso->array_of_ints[i] == JSON_NULL )
                fprintf(fp, "null");
            else
                fprintf(fp, "%" PRId64, jso->array_of_ints[i]);
        }
        output_lf(fp, jso, level);
        fprintf(fp, "]");
        break;
    case JSON_TYPE_STRING:
        fprintf(fp, "\"%s\"", jso->str);
        break;
    case JSON_TYPE_NUMBER:
        if ( jso->val == JSON_NULL )
            fprintf(fp, "null");
        else
            fprintf(fp, "%" PRId64, jso->val);
        break;
    case JSON_TYPE_BOOL:
        fprintf(fp, (jso->val == 0) ? "false" : "true");
        break;
    }
}

void json_fputs(FILE *fp, json_t *jso)
{
    json_print_element(fp, jso, 0);
    fprintf(fp, "\n");
}
