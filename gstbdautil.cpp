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

static std::string
mb_to_wc (const wchar_t * wc)
{
  char mb[512];
  size_t size;
  errno_t err = wcstombs_s (&size, mb, wc, sizeof (mb));

  if (err != 0) {
    return "";
  }

  return mb;
}

std::string
bda_err_to_str (HRESULT hr)
{
  char error_string[MAX_ERROR_TEXT_LEN];
  DWORD res = AMGetErrorTextA (hr, error_string, sizeof (error_string));

  if (res == 0) {
    return "Unknown error";
  }

  return trim_string (error_string);
}

std::string
bda_get_tuner_name (IMoniker * tuner_moniker)
{
  std::string name;

  IPropertyBagPtr property_bag;
  HRESULT res = tuner_moniker->BindToStorage (0, 0, IID_IPropertyBag,
      reinterpret_cast < void **>(&property_bag));
  if (FAILED (res)) {
    return name;
  }

  VARIANT var_name;
  VariantInit (&var_name);
  res = property_bag->Read (L"FriendlyName", &var_name, 0);

  if (FAILED (res)) {
    VariantClear (&var_name);
    return name;
  }

  name = mb_to_wc (var_name.bstrVal);
  VariantClear (&var_name);

  return name;
}
