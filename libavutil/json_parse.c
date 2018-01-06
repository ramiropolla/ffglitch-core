/* Crappy JSON parser
 * Copyright (c) 2018-2019 Ramiro Polla
 * MIT License
 */

#include "json.h"

#include <stdlib.h>
#include <string.h>

//---------------------------------------------------------------------
static char *json_parse_element(json_ctx_t *jctx, json_t *jso, char *buf);

static void *set_error(json_ctx_t *jctx, const char *str)
{
    if ( jctx->error == NULL )
        jctx->error = str;
    return NULL;
}

static char *json_skip_whitespace(char *buf)
{
    while ( *buf == ' '
         || *buf == '\t'
         || *buf == '\r'
         || *buf == '\n' )
    {
        buf++;
    }
    return buf;
}

static char *parse_string(json_ctx_t *jctx, char **pout, char *buf)
{
    char *out = buf;
    *pout = out;
    while ( *buf != '"' )
    {
        if ( *buf == '\0' )
        {
            jctx->error = "EOL";
            return NULL;
        }
        else if ( *buf == '\\' )
        {
            buf++;
            switch ( *buf )
            {
                case '"':
                case '\\':
                case '/': *out++ = *buf; break;
                case 'b': *out++ = '\b'; break;
                case 'f': *out++ = '\f'; break;
                case 'n': *out++ = '\n'; break;
                case 'r': *out++ = '\r'; break;
                case 't': *out++ = '\t'; break;
                case 'u':
                    // Copy unicode as-is.
                    *out++ = '\\';
                    *out++ = *buf++;
                    break;
                default:
                    return set_error(jctx, "Wrong char after backslash");
            }
            buf++;
        }
        else
        {
            *out++ = *buf++;
        }
    }
    *out = '\0';
    buf++;
    return buf;
}

static char *json_parse_string(json_ctx_t *jctx, json_t *jso, char *buf)
{
    jso->flags = JSON_TYPE_STRING;
    buf = parse_string(jctx, &jso->str, buf);
    if ( buf != NULL )
        jso->str = json_allocator_strdup(jctx, jso->str, strlen(jso->str)+1);
    return buf;
}

static char *parse_number(int64_t *pout, char *buf)
{
    uint64_t uval = 0;
    int is_negative = (*buf == '-');
    if ( is_negative )
        buf++;

    while ( *buf >= '0' && *buf <= '9' )
        uval = (uval * 10) + *buf++ - '0';

    *pout = is_negative ? -uval : uval;

    return buf;
}

static char *json_parse_number(json_ctx_t *jctx, json_t *jso, char *buf)
{
    jso->flags = JSON_TYPE_NUMBER;
    return parse_number(&jso->val, buf);
}

static char *json_parse_null(json_ctx_t *jctx, json_t *jso, char *buf)
{
    jso->flags = JSON_TYPE_NUMBER;
    jso->val = JSON_NULL;
    return buf + 4;
}

static char *json_parse_object(json_ctx_t *jctx, json_t *jso, char *buf)
{
    char **names = NULL;
    json_t **values = NULL;
    size_t alloc = 0;
    size_t len = 0;
    buf = json_skip_whitespace(buf);
    if ( *buf == '}' )
    {
        // Empty object.
        jso->flags = JSON_TYPE_OBJECT | len;
        jso->obj = json_allocator_get(jctx, sizeof(json_obj_t));
        jso->obj->names = NULL;
        jso->obj->values = NULL;
        buf++;
        return buf;
    }
    while ( 42 )
    {
        json_t *jval = alloc_json_t(jctx);
        size_t cur_i = len++;
        char *name;

        buf = json_skip_whitespace(buf);
        if ( *buf++ != '"' )
            return set_error(jctx, "object (key is not string)");
        buf = parse_string(jctx, &name, buf);
        if ( buf == NULL )
            return set_error(jctx, "object (error parsing key)");
        buf = json_skip_whitespace(buf);
        if ( *buf++ != ':' )
            return set_error(jctx, "object (missing : between key and value)");
        buf = json_skip_whitespace(buf);
        buf = json_parse_element(jctx, jval, buf);
        if ( buf == NULL )
            return set_error(jctx, "object (error parsing value)");

        if ( len > alloc )
        {
            alloc += OBJECT_CHUNK;
            names = realloc(names, alloc * sizeof(char *));
            values = realloc(values, alloc * sizeof(json_t *));
        }
        names[cur_i] = json_allocator_strdup(jctx, name, strlen(name)+1);
        values[cur_i] = jval;

        buf = json_skip_whitespace(buf);
        if ( *buf == '}' )
        {
            buf++;
            break;
        }
        else if ( *buf == ',' )
        {
            buf++;
            continue;
        }
        else
        {
            return set_error(jctx, "object (bad token after key:value)");
        }
    }
    jso->flags = JSON_TYPE_OBJECT | len;
    jso->obj = json_allocator_get(jctx, sizeof(json_obj_t));
    jso->obj->names = json_allocator_dup(jctx, names, len * sizeof(char *));
    jso->obj->values = json_allocator_dup(jctx, values, len * sizeof(json_t *));
    free(names);
    free(values);
    return buf;
}

static char *json_parse_array(json_ctx_t *jctx, json_t *jso, char *buf)
{
    int64_t *array_of_ints = NULL;
    json_t **array = NULL;
    size_t alloc = 0;
    size_t len = 0;
    buf = json_skip_whitespace(buf);
    if ( *buf == ']' )
    {
        // Empty array.
        jso->flags = JSON_TYPE_ARRAY | len;
        jso->array = NULL;
        buf++;
        return buf;
    }
    while ( 42 )
    {
        json_t jval;
        size_t cur_i = len++;

        buf = json_skip_whitespace(buf);
        buf = json_parse_element(jctx, &jval, buf);
        if ( buf == NULL )
            return set_error(jctx, "array (error parsing element)");

        // If nth element is not a number and we were building an array
        // of ints, convert array back to normal.
        if ( array_of_ints != NULL
          && array == NULL
          && JSON_TYPE(jval.flags) != JSON_TYPE_NUMBER )
        {
            json_t jnum;
            jnum.flags = JSON_TYPE_NUMBER;
            array = malloc(alloc * sizeof(json_t *));
            for ( size_t i = 0; i < cur_i; i++ )
            {
                jnum.val = array_of_ints[i];
                array[i] = json_allocator_dup(jctx, &jnum, sizeof(json_t));
            }
            free(array_of_ints);
            array_of_ints = NULL;
        }

        // Expand array if needed.
        if ( len > alloc )
        {
            alloc += ARRAY_CHUNK;
            // If first element is a number, start building an array of ints.
            if ( array_of_ints != NULL || (cur_i == 0 && JSON_TYPE(jval.flags) == JSON_TYPE_NUMBER) )
                array_of_ints = realloc(array_of_ints, alloc * sizeof(int64_t));
            else
                array = realloc(array, alloc * sizeof(json_t *));
        }

        if ( array_of_ints != NULL )
            array_of_ints[cur_i] = jval.val;
        else
            array[cur_i] = json_allocator_dup(jctx, &jval, sizeof(json_t));

        buf = json_skip_whitespace(buf);
        if ( *buf == ']' )
        {
            buf++;
            break;
        }
        else if ( *buf == ',' )
        {
            buf++;
            continue;
        }
        else
        {
            return set_error(jctx, "array (bad token after element)");
        }
    }
    if ( array_of_ints != NULL )
    {
        jso->array_of_ints = json_allocator_dup(jctx, array_of_ints, len * sizeof(int64_t));
        free(array_of_ints);
        jso->flags = JSON_TYPE_ARRAY_OF_INTS;
    }
    else
    {
        jso->array = json_allocator_dup(jctx, array, len * sizeof(json_t *));
        free(array);
        jso->flags = JSON_TYPE_ARRAY;
    }
    jso->flags |= len;
    return buf;
}

static char *json_parse_element(json_ctx_t *jctx, json_t *jso, char *buf)
{
    buf = json_skip_whitespace(buf);
    if ( *buf == '{' )
        return json_parse_object(jctx, jso, buf+1);
    else if ( *buf == '[' )
        return json_parse_array(jctx, jso, buf+1);
    else if ( *buf == '"' )
        return json_parse_string(jctx, jso, buf+1);
    else if ( *buf == '-' || (*buf >= '0' && *buf <= '9') )
        return json_parse_number(jctx, jso, buf);
    else if ( buf[0] == 'n' && buf[1] == 'u' && buf[2] == 'l' && buf[3] == 'l' )
        return json_parse_null(jctx, jso, buf);
    return NULL;
}

json_t *json_parse(json_ctx_t *jctx, char *buf)
{
    json_t *jso = alloc_json_t(jctx);
    buf = json_parse_element(jctx, jso, buf);
    if ( buf == NULL )
        return NULL;
    buf = json_skip_whitespace(buf);
    if ( *buf != '\0' )
        return NULL;
    return jso;
}

const char *json_parse_error(json_ctx_t *jctx)
{
    return jctx->error;
}
