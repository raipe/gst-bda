/* GStreamer
 * Copyright (C) 2015 Raimo Järvi <raimo.jarvi@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include "gstbdautil.h"
#include <windows.h>
#include <errors.h>

static const char *
trim_string (char *str)
{
  /* Trim leading whitespace. */
  while (*str && isspace (*str)) {
    str++;
  }

  /* Trim trailing whitespace. */
  for (int i = strlen (str) - 1; i >= 0; i--) {
    if (!isspace (str[i])) {
      break;
    }

    str[i] = 0;
  }

  return str;
}

std::string bda_err_to_str (HRESULT hr)
{
  char error_string[MAX_ERROR_TEXT_LEN];
  DWORD res = AMGetErrorTextA (hr, error_string, sizeof (error_string));

  if (res == 0) {
    return "Unknown error";
  }

  return trim_string (error_string);
}
