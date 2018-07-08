/*
 * $Id$
 *
 * Copyright (C) 2002, 2003 ETC s.r.o.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by Marcel Telka <marcel@telka.sk>, 2002, 2003.
 *
 */

#include <sysdep.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <urjtag/error.h>
#include <urjtag/log.h>
#include <urjtag/tap_register.h>

urj_tap_register_t *
urj_tap_register_alloc (int len)
{
    urj_tap_register_t *tr;

    if (len < 1)
    {
        urj_error_set (URJ_ERROR_INVALID, "len < 1");
        return NULL;
    }

    tr = malloc (sizeof (urj_tap_register_t));
    if (!tr)
    {
        urj_error_set (URJ_ERROR_OUT_OF_MEMORY, "malloc(%zd) fails",
                       sizeof (urj_tap_register_t));
        return NULL;
    }

    tr->data = malloc (len);
    if (!tr->data)
    {
        free (tr);
        urj_error_set (URJ_ERROR_OUT_OF_MEMORY, "malloc(%zd) fails",
                       (size_t) len);
        return NULL;
    }

    memset (tr->data, 0, len);

    tr->string = malloc (len + 1);
    if (!tr->string)
    {
        free (tr->data);
        free (tr);
        urj_error_set (URJ_ERROR_OUT_OF_MEMORY, "malloc(%zd) fails",
                       (size_t) (len + 1));
        return NULL;
    }

    tr->len = len;
    tr->string[len] = '\0';

    return tr;
}

urj_tap_register_t *
urj_tap_register_realloc (urj_tap_register_t *tr, int new_len)
{
    if (!tr)
        return urj_tap_register_alloc (new_len);

    if (new_len < 1)
    {
        urj_error_set (URJ_ERROR_INVALID, "new_len < 1");
        return NULL;
    }

    tr->data = realloc (tr->data, new_len);

    if (!tr->data)
    {
        urj_error_set (URJ_ERROR_OUT_OF_MEMORY, "realloc(%d) fails",
                       new_len);
        return NULL;
    }

    if (tr->len < new_len)
        memset (tr->data + tr->len, 0, (new_len - tr->len));

    tr->len = new_len;

    return tr;
}

urj_tap_register_t *
urj_tap_register_duplicate (const urj_tap_register_t *tr)
{
    if (!tr)
    {
        urj_error_set (URJ_ERROR_INVALID, "tr == NULL");
        return NULL;
    }

    return urj_tap_register_init (urj_tap_register_alloc (tr->len),
                                  urj_tap_register_get_string (tr));
}

void
urj_tap_register_free (urj_tap_register_t *tr)
{
    if (tr)
    {
        free (tr->data);
        free (tr->string);
    }
    free (tr);
}

urj_tap_register_t *
urj_tap_register_fill (urj_tap_register_t *tr, int val)
{
    if (tr)
        memset (tr->data, val & 1, tr->len);

    return tr;
}

int
urj_tap_register_set_string (urj_tap_register_t *tr, const char *str)
{
    if (!tr)
    {
        urj_error_set (URJ_ERROR_INVALID, "tr == NULL");
        return URJ_STATUS_FAIL;
    }

    if (strncmp (str, "0x", 2) == 0)
    {
        /* Hex values */
        uint64_t val;

        if (sscanf (str, "%"PRIX64, &val) != 1)
        {
            urj_error_set (URJ_ERROR_SYNTAX,
                           _("invalid hex string '%s'"),
                           str);
            return URJ_STATUS_FAIL;
        }
        return urj_tap_register_set_value (tr, val);
    }
    else
    {
        /* Bit string */
        unsigned int bit;

        if (strspn (str, "01") != strlen (str))
        {
            urj_error_set (URJ_ERROR_SYNTAX,
                           _("bit patterns should be 0s and 1s, not '%s'"),
                           str);
            return URJ_STATUS_FAIL;
        }
        else if (tr->len != strlen (str))
        {
            urj_error_set (URJ_ERROR_OUT_OF_BOUNDS,
                           _("register length %d mismatch: %zd"),
                           tr->len, strlen (str));
            return URJ_STATUS_FAIL;
        }

        for (bit = 0; str[bit]; ++bit)
            tr->data[tr->len - 1 - bit] = (str[bit] == '1');

        return URJ_STATUS_OK;
    }
}

int
urj_tap_register_set_string_bit_range (urj_tap_register_t *tr, const char *str, int msb, int lsb)
{
    int step = msb >= lsb ? 1 : -1;
    int len =  msb >= lsb ? msb - lsb + 1 : lsb - msb + 1;
    int sidx;

    if (!tr)
    {
        urj_error_set (URJ_ERROR_INVALID, "tr == NULL");
        return URJ_STATUS_FAIL;
    }

    if (msb > tr->len - 1 || lsb > tr->len - 1 || msb < 0 || lsb < 0)
    {
        urj_error_set (URJ_ERROR_OUT_OF_BOUNDS,
                       _("register %d:%d will not fit in %d bits"),
                       msb, lsb, tr->len);
        return URJ_STATUS_FAIL;
    }

    if (strncmp (str, "0x", 2) == 0)
    {
        /* Hex values */
        uint64_t val;

	//printf("in urj_tap_register_set_string_bit_range(%s) hexmode\n", str);
        if (sscanf (str, "%"PRIX64, &val) != 1)
        {
            urj_error_set (URJ_ERROR_SYNTAX,
                           _("invalid hex string '%s'"),
                           str);
            return URJ_STATUS_FAIL;
        }
        return urj_tap_register_set_value_bit_range (tr, val, msb, lsb);
    }
    else
    {
        /* Bit string */
        int bit;

	//printf("in urj_tap_register_set_string_bit_range(%s, %d, %d) binmode len=%d step=%d\n", str, msb, lsb, len, step);
        if (strspn (str, "01") != strlen (str))
        {
            urj_error_set (URJ_ERROR_SYNTAX,
                           _("bit patterns should be 0s and 1s, not '%s'"),
                           str);
            return URJ_STATUS_FAIL;
        }
        else if (len != strlen (str))
        {
            urj_error_set (URJ_ERROR_OUT_OF_BOUNDS,
                           _("register subfield length %d mismatch: %zd"),
                           len, strlen (str));
            return URJ_STATUS_FAIL;
        }

        for (sidx = 0, bit = msb; bit*step >= lsb*step; bit -= step, sidx++)
        {
            tr->data[bit] = (str[sidx] == '1');
        }

        return URJ_STATUS_OK;
    }
}

int
urj_tap_register_set_value_bit_range (urj_tap_register_t *tr, uint64_t val, int msb, int lsb)
{
    int bit;
    int step = msb >= lsb ? 1 : -1;

    if (!tr)
    {
        urj_error_set (URJ_ERROR_INVALID, "tr == NULL");
        return URJ_STATUS_FAIL;
    }

    if (msb > tr->len - 1 || lsb > tr->len - 1 || msb < 0 || lsb < 0)
    {
        urj_error_set (URJ_ERROR_OUT_OF_BOUNDS,
                       _("register %d:%d will not fit in %d bits"),
                       msb, lsb, tr->len);
        return URJ_STATUS_FAIL;
    }

    for (bit = lsb; bit * step <= msb * step; bit += step)
    {
        tr->data[bit] = !!(val & 1);
        val >>= 1;
    }

    return URJ_STATUS_OK;
}

int
urj_tap_register_set_value (urj_tap_register_t *tr, uint64_t val)
{
    return urj_tap_register_set_value_bit_range (tr, val, tr->len - 1, 0);
}

const char *
urj_tap_register_get_string_bit_range (const urj_tap_register_t *tr, int msb, int lsb)
{
    int bit;
    int string_idx;
    int step = msb >= lsb ? 1 : -1;

    if (!tr)
    {
        urj_error_set (URJ_ERROR_INVALID, "tr == NULL");
        return NULL;
    }

    if (msb > tr->len - 1 || lsb > tr->len - 1 || msb < 0 || lsb < 0) 
    {
        urj_error_set (URJ_ERROR_INVALID, "msb or lsb out of range");
        return NULL;
    }

    for (bit = msb, string_idx = 0; bit * step >= lsb * step; bit -= step, string_idx++)
    {
        tr->string[string_idx] = (tr->data[bit] & 1) ? '1' : '0';
    }
    tr->string[string_idx] = '\0';

    return tr->string;
}

const char *
urj_tap_register_get_string (const urj_tap_register_t *tr)
{
    int i;

    if (!tr)
    {
        urj_error_set (URJ_ERROR_INVALID, "tr == NULL");
        return NULL;
    }

    for (i = 0; i < tr->len; i++)
        tr->string[tr->len - 1 - i] = (tr->data[i] & 1) ? '1' : '0';

    return tr->string;
}

uint64_t
urj_tap_register_get_value_bit_range (const urj_tap_register_t *tr, int msb, int lsb)
{
    int bit;
    uint64_t l, b;
    int step = msb >= lsb ? 1 : -1;

    if (!tr)
        return 0;

    if (msb > tr->len - 1 || lsb > tr->len - 1 || msb < 0 || lsb < 0)
        return 0;

    l = 0;
    b = 1;
    for (bit = lsb; bit * step <= msb * step; bit += step)
    {
        if (tr->data[bit] & 1)
            l |= b;
        b <<= 1;
    }

    return l;
}

uint64_t
urj_tap_register_get_value (const urj_tap_register_t *tr)
{
    return urj_tap_register_get_value_bit_range (tr, tr->len - 1, 0);
}

int
urj_tap_register_all_bits_same_value (const urj_tap_register_t *tr)
{
    int i, value;
    if (!tr)
        return -1;
    if (tr->len < 0)
        return -1;

    /* Return -1 if any of the bits in the register
     * differs from the others; the value otherwise. */

    value = tr->data[0] & 1;

    for (i = 1; i < tr->len; i++)
    {
        if ((tr->data[i] & 1) != value)
            return -1;
    }
    return value;
}

urj_tap_register_t *
urj_tap_register_init (urj_tap_register_t *tr, const char *value)
{
    int i;

    const char *p;

    if (!value || !tr)
        return tr;

    p = strchr (value, '\0');

    for (i = 0; i < tr->len; i++)
    {
        if (p == value)
            tr->data[i] = 0;
        else
        {
            p--;
            tr->data[i] = (*p == '0') ? 0 : 1;
        }
    }

    return tr;
}

int
urj_tap_register_compare (const urj_tap_register_t *tr,
                          const urj_tap_register_t *tr2)
{
    int i;

    if (!tr && !tr2)
        return 0;

    if (!tr || !tr2)
        return 1;

    if (tr->len != tr2->len)
        return 1;

    for (i = 0; i < tr->len; i++)
        if (tr->data[i] != tr2->data[i])
            return 1;

    return 0;
}

int
urj_tap_register_match (const urj_tap_register_t *tr, const char *expr)
{
    int i;
    const char *s;

    if (!tr || !expr || (tr->len != strlen (expr)))
        return 0;

    s = urj_tap_register_get_string (tr);

    for (i = 0; i < tr->len; i++)
        if ((expr[i] != '?') && (expr[i] != s[i]))
            return 0;

    return 1;
}

urj_tap_register_t *
urj_tap_register_inc (urj_tap_register_t *tr)
{
    int i;

    if (!tr)
        return NULL;

    for (i = 0; i < tr->len; i++)
    {
        tr->data[i] ^= 1;

        if (tr->data[i] == 1)
            break;
    }

    return tr;
}

urj_tap_register_t *
urj_tap_register_dec (urj_tap_register_t *tr)
{
    int i;

    if (!tr)
        return NULL;

    for (i = 0; i < tr->len; i++)
    {
        tr->data[i] ^= 1;

        if (tr->data[i] == 0)
            break;
    }

    return tr;
}

urj_tap_register_t *
urj_tap_register_shift_right (urj_tap_register_t *tr, int shift)
{
    int i;

    if (!tr)
        return NULL;

    if (shift < 1)
        return tr;

    for (i = 0; i < tr->len; i++)
    {
        if (i + shift < tr->len)
            tr->data[i] = tr->data[i + shift];
        else
            tr->data[i] = 0;
    }

    return tr;
}

urj_tap_register_t *
urj_tap_register_shift_left (urj_tap_register_t *tr, int shift)
{
    int i;

    if (!tr)
        return NULL;

    if (shift < 1)
        return tr;

    for (i = tr->len - 1; i >= 0; i--)
    {
        if (i - shift >= 0)
            tr->data[i] = tr->data[i - shift];
        else
            tr->data[i] = 0;
    }

    return tr;
}
