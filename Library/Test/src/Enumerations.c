#include "Enumerations.h"

/* Enumeration string lists */

/* ANCHOR ENUMERATIONS */

char *HTLinkResults[] =
{
  "HT_LINK_INVALID",
  "HT_LINK_NONE",
  "HT_LINK_ERROR",
  "HT_LINK_OK",
  NULL
};

/* REQUEST ENUMERATIONS */

char *reload_names[] =
{
"HT_ANY_VERSION",
"HT_MEM_REFRESH",
"HT_CACHE_REFRESH",
"HT_FORCE_RELOAD",
NULL
};

char *HTGnHd_names[] =
{
  "HT_G_DATE",
  "HT_G_FORWARDED",
  "HT_G_MESSAGE_ID",
  "HT_G_MIME",
  "HT_G_CONNECTION",
  "HT_G_NO_CACHE",
  NULL
};

char *HTRqHd_names[] =
{
  "HT_C_ACCEPT_TYPE",
  "HT_C_ACCEPT_CHAR",
  "HT_C_ACCEPT_ENC",
  "HT_C_ACCEPT_LAN",
  "HT_C_FROM",
  "HT_C_IMS",
  "HT_C_HOST",
  "HT_C_REFERER",
  "HT_C_USER_AGENT",
  NULL
};

char *HTRsHd_names[] =
{
  "HT_S_LOCATION",
  "HT_S_PROXY_AUTH",
  "HT_S_PUBLIC",
  "HT_S_RETRY_AFTER",
  "HT_S_SERVER",
  "HT_S_WWW_AUTH",
  NULL
};

char *HTEnHd_names[] =
{
  "HT_E_ALLOW",
  "HT_E_CONTENT_ENCODING",
  "HT_E_CONTENT_LANGUAGE",
  "HT_E_CONTENT_LENGTH",
  "HT_E_CTE",                 /* Content-Transfer-Encoding */
  "HT_E_CONTENT_TYPE",
  "HT_E_DERIVED_FROM",
  "HT_E_EXPIRES",
  "HT_E_LAST_MODIFIED",
  "HT_E_LINK",
  "HT_E_TITLE",
  "HT_E_URI",
  "HT_E_VERSION",
  NULL
};

char *HTSeverity_names[] = 
{
  "ERR_FATAL",
  "ERR_NON_FATAL",
  "ERR_WARN",
  "ERR_INFO",
  NULL
};

char *HTPriority_names[] =
{
  "HT_PRIORITY_INV",
  "HT_PRIORITY_OFF",
  "HT_PRIORITY_MIN",
  "HT_PRIORITY_MAX",
  NULL
};

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
      
/*      	   Enumerations and name functions		      */

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */

/**************************
**       HTANCHOR        **
**************************/

HTLinkResult HTLinkResult_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTLinkResults+1)))
      return HT_LINK_NONE;
    else if (!strcasecomp(name, *(HTLinkResults+2)))
      return HT_LINK_ERROR;
    else if (!strcasecomp(name, *(HTLinkResults+3)))
      return HT_LINK_OK;
  }
  return HT_LINK_INVALID;
}

char *HTLinkResult_name(HTLinkResult result) {
  if (result & HT_LINK_NONE)
    return *(HTLinkResults+1);
  else if (result == HT_LINK_ERROR)
    return *(HTLinkResults+2);
  else if (result == HT_LINK_OK)
    return *(HTLinkResults+3);
  else
    return *HTLinkResults;
}

/**************************
**      HTREQUEST        **
**************************/

/* RELOAD */

HTReload HTReload_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(reload_names+1)))
      return HT_MEM_REFRESH;
    else if (!strcasecomp(name, *(reload_names+2)))
      return HT_CACHE_REFRESH;
    else if (!strcasecomp(name, *(reload_names+3)))
      return HT_FORCE_RELOAD;
  }
  return HT_ANY_VERSION;
}

/* GNHD */

HTGnHd HTGnHd_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTGnHd_names+1)))
      return HT_G_FORWARDED;
    else if (!strcasecomp(name, *(HTGnHd_names+2)))
      return HT_G_MESSAGE_ID;
    else if (!strcasecomp(name, *(HTGnHd_names+3)))
      return HT_G_MIME;
    else if (!strcasecomp(name, *(HTGnHd_names+4)))
      return HT_G_CONNECTION;
    else if (!strcasecomp(name, *(HTGnHd_names+5)))
      return HT_G_NO_CACHE;
  }
  return HT_G_DATE;
}

/* RQHD */

HTRqHd HTRqHd_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTRqHd_names+1)))
      return HT_C_ACCEPT_CHAR;
    else if (!strcasecomp(name, *(HTRqHd_names+2)))
      return HT_C_ACCEPT_ENC;
    else if (!strcasecomp(name, *(HTRqHd_names+3)))
      return HT_C_ACCEPT_LAN;
    else if (!strcasecomp(name, *(HTRqHd_names+4)))
      return HT_C_FROM;
    else if (!strcasecomp(name, *(HTRqHd_names+5)))
      return HT_C_IMS;
    else if (!strcasecomp(name, *(HTRqHd_names+6)))
      return HT_C_HOST;
    else if (!strcasecomp(name, *(HTRqHd_names+7)))
      return HT_C_REFERER;
    else if (!strcasecomp(name, *(HTRqHd_names+8)))
      return HT_C_USER_AGENT;
  }
  return HT_C_ACCEPT_TYPE;
}

/* RSHD */

HTRsHd HTRsHd_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTRsHd_names+1)))
      return HT_S_PROXY_AUTH;
    else if (!strcasecomp(name, *(HTRsHd_names+2)))
      return HT_S_PUBLIC;
    else if (!strcasecomp(name, *(HTRsHd_names+3)))
      return HT_S_RETRY_AFTER;
    else if (!strcasecomp(name, *(HTRsHd_names+4)))
      return HT_S_SERVER;
    else if (!strcasecomp(name, *(HTRsHd_names+5)))
      return HT_S_WWW_AUTH;
  }
  return HT_S_LOCATION;
}

/* ENHD */

HTEnHd HTEnHd_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTEnHd_names+1)))
      return HT_E_CONTENT_ENCODING;
    else if (!strcasecomp(name, *(HTEnHd_names+2)))
      return HT_E_CONTENT_LANGUAGE;
    else if (!strcasecomp(name, *(HTEnHd_names+3)))
      return HT_E_CONTENT_LENGTH;
    else if (!strcasecomp(name, *(HTEnHd_names+4)))
      return HT_E_CTE;
    else if (!strcasecomp(name, *(HTEnHd_names+5)))
      return HT_E_CONTENT_TYPE;
    else if (!strcasecomp(name, *(HTEnHd_names+6)))
      return HT_E_DERIVED_FROM;
    else if (!strcasecomp(name, *(HTEnHd_names+7)))
      return HT_E_EXPIRES;
    else if (!strcasecomp(name, *(HTEnHd_names+8)))
      return HT_E_LAST_MODIFIED;
    else if (!strcasecomp(name, *(HTEnHd_names+9)))
      return HT_E_LINK;
    else if (!strcasecomp(name, *(HTEnHd_names+10)))
      return HT_E_TITLE;
    else if (!strcasecomp(name, *(HTEnHd_names+11)))
      return HT_E_URI;
    else if (!strcasecomp(name, *(HTEnHd_names+12)))
      return HT_E_VERSION;
  }
  return HT_E_ALLOW;
}

/* HTSEVERITY */

HTSeverity HTSeverity_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTSeverity_names+1)))
      return ERR_NON_FATAL;
    else if (!strcasecomp(name, *(HTSeverity_names+2)))
      return ERR_WARN;
    else if (!strcasecomp(name, *(HTSeverity_names+3)))
      return ERR_INFO;
  }
  return ERR_FATAL;
}

char *HTSeverity_name(HTSeverity severity) {
  if (severity & ERR_NON_FATAL)
    return *(HTSeverity_names+1);
  else if (severity == ERR_WARN)
    return *(HTSeverity_names+2);
  else if (severity == ERR_INFO)
    return *(HTSeverity_names+3);
  else
    return *HTSeverity_names;
}

/* HTPRIORITY */

HTPriority HTPriority_enum(char *name) {
  if (name) {
    if (!strcasecomp(name, *(HTPriority_names+1)))
      return HT_PRIORITY_OFF;
    else if (!strcasecomp(name, *(HTPriority_names+2)))
      return HT_PRIORITY_MIN;
    else if (!strcasecomp(name, *(HTPriority_names+3)))
      return HT_PRIORITY_MAX;
  }
  return HT_PRIORITY_INV;
}

char *HTPriority_name(HTPriority priority) {
  if (priority & HT_PRIORITY_OFF)
    return *(HTPriority_names+1);
  else if (priority == HT_PRIORITY_MIN)
    return *(HTPriority_names+2);
  else if (priority == HT_PRIORITY_MAX)
    return *(HTPriority_names+3);
  else
    return *HTPriority_names;
}