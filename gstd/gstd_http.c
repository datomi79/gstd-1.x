/*
 * GStreamer Daemon - Gst Launch under steroids
 * Copyright (c) 2015-2020 Ridgerun, LLC (http://www.ridgerun.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <libsoup/soup.h>

#include "gstd_ipc.h"
#include "gstd_http.h"
#include "gstd_parser.h"
#include "gstd_element.h"
#include "gstd_pipeline_bus.h"
#include "gstd_event_handler.h"

/* Gstd HTTP debugging category */
GST_DEBUG_CATEGORY_STATIC (gstd_http_debug);
#define GST_CAT_DEFAULT gstd_http_debug

#define GSTD_DEBUG_DEFAULT_LEVEL GST_LEVEL_INFO

typedef enum _GstdHttpVerbs
{
  GET,
  POST,
  PUT,
  DELETE,
} GstdHttpVerbs;

struct _GstdHttp
{
  GstdIpc parent;
  guint port;
  gchar *address;
  SoupServer *server;
};

struct _GstdHttpClass
{
  GstdIpcClass parent_class;
};

G_DEFINE_TYPE (GstdHttp, gstd_http, GSTD_TYPE_IPC);

/* VTable */

static void gstd_http_finalize (GObject *);
static GstdReturnCode gstd_http_start (GstdIpc * base, GstdSession * session);
static GstdReturnCode gstd_http_stop (GstdIpc * base);
static gboolean gstd_http_init_get_option_group (GstdIpc * base,
    GOptionGroup ** group);
static SoupStatus get_status_code (GstdReturnCode ret);
static GstdReturnCode do_get (SoupServer * server, SoupMessage * msg,
    char **output, GstdSession * session);
static GstdReturnCode do_post (SoupServer * server, SoupMessage * msg,
    char *name, char *description, char **output, GstdSession * session);
static GstdReturnCode do_put (SoupServer * server, SoupMessage * msg,
    char *name, char **output, GstdSession * session);
static GstdReturnCode do_delete (SoupServer * server, SoupMessage * msg,
    char *name, char **output, GstdSession * session);
static void do_request (SoupServer * server, SoupMessage * msg,
    GHashTable * query, GstdSession * session, GstdHttpVerbs request_verb);
static void server_callback (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * context,
    gpointer data);

static void
gstd_http_class_init (GstdHttpClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstdIpcClass *gstdipc_class = GSTD_IPC_CLASS (klass);
  guint debug_color;
  gstdipc_class->get_option_group =
      GST_DEBUG_FUNCPTR (gstd_http_init_get_option_group);
  gstdipc_class->start = GST_DEBUG_FUNCPTR (gstd_http_start);
  object_class->finalize = gstd_http_finalize;
  gstdipc_class->stop = GST_DEBUG_FUNCPTR (gstd_http_stop);

  /* Initialize debug category with nice colors */
  debug_color = GST_DEBUG_FG_BLACK | GST_DEBUG_BOLD | GST_DEBUG_BG_WHITE;
  GST_DEBUG_CATEGORY_INIT (gstd_http_debug, "gstdhttp", debug_color,
      "Gstd HTTP category");
}

static void
gstd_http_init (GstdHttp * self)
{
  GST_INFO_OBJECT (self, "Initializing gstd Http");
  self->port = GSTD_HTTP_DEFAULT_PORT;
  self->address = g_strdup (GSTD_HTTP_DEFAULT_ADDRESS);
  self->server =NULL;
}

static void
gstd_http_finalize (GObject * object)
{
  GstdHttp *self = GSTD_HTTP (object);

  GST_INFO_OBJECT (object, "Deinitializing gstd HTTP");

  if (self->address) {
    g_free (self->address);
  }
  self->address = NULL;

  G_OBJECT_CLASS (gstd_http_parent_class)->finalize (object);
}

static SoupStatus
get_status_code (GstdReturnCode ret)
{
  SoupStatus status = SOUP_STATUS_OK;

  if (ret == GSTD_EOK) {
    status = SOUP_STATUS_OK;
  } else if (ret == GSTD_BAD_COMMAND || ret == GSTD_NO_RESOURCE) {
    status = SOUP_STATUS_NOT_FOUND;
  } else if (ret == GSTD_EXISTING_RESOURCE) {
    status = SOUP_STATUS_CONFLICT;
  } else if (ret == GSTD_BAD_VALUE) {
    status = SOUP_STATUS_NO_CONTENT;
  } else {
    status = SOUP_STATUS_BAD_REQUEST;
  }

  return status;
}

static GstdReturnCode
do_get (SoupServer * server, SoupMessage * msg, char **output,
    GstdSession * session)
{
  gchar *message = NULL;
  SoupURI *address = NULL;
  GstdReturnCode ret = GSTD_EOK;

  g_return_val_if_fail (server, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (msg, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (session, GSTD_NULL_ARGUMENT);

  address = soup_message_get_uri (msg);

  message = g_strdup_printf ("read %s", soup_uri_get_path (address));
  ret = gstd_parser_parse_cmd (session, message, output);
  g_free (message);

  return ret;
}

static GstdReturnCode
do_post (SoupServer * server, SoupMessage * msg, char *name,
    char *description, char **output, GstdSession * session)
{
  gchar *message = NULL;
  SoupURI *address = NULL;
  GstdReturnCode ret = GSTD_EOK;

  g_return_val_if_fail (server, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (msg, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (session, GSTD_NULL_ARGUMENT);

  address = soup_message_get_uri (msg);

  if (!name) {
    ret = GSTD_BAD_VALUE;
    GST_ERROR_OBJECT (session,
        "Wrong query param provided, \"name\" doesn't exist");
    goto out;
  }
  if (!description) {
      ret = GSTD_BAD_VALUE;
      GST_ERROR_OBJECT (session,
          "Wrong query param provided, \"description\" doesn't exist");
      goto out;
  }

  message = g_strdup_printf
      ("create %s %s %s", soup_uri_get_path (address), name, description);
  ret = gstd_parser_parse_cmd (session, message, output);
  g_free (message);

  out:
    return ret;
}

static GstdReturnCode
do_put (SoupServer * server, SoupMessage * msg, char *name, char **output,
    GstdSession * session)
{
  gchar *message = NULL;
  SoupURI *address = NULL;
  GstdReturnCode ret = GSTD_EOK;

  g_return_val_if_fail (server, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (msg, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (session, GSTD_NULL_ARGUMENT);

  address = soup_message_get_uri (msg);

  if (!name) {
    ret = GSTD_BAD_VALUE;
    GST_ERROR_OBJECT (session,
        "Wrong query param provided, \"name\" doesn't exist");
    goto out;
  }

  message = g_strdup_printf ("update %s %s", soup_uri_get_path (address), name);
  ret = gstd_parser_parse_cmd (session, message, output);
  g_free (message);

  out:
    return ret;
}

static GstdReturnCode
do_delete (SoupServer * server, SoupMessage * msg, char *name,
    char **output, GstdSession * session)
{
  gchar *message = NULL;
  SoupURI *address = NULL;
  GstdReturnCode ret = GSTD_EOK;

  g_return_val_if_fail (server, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (msg, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (session, GSTD_NULL_ARGUMENT);

  address = soup_message_get_uri (msg);

  if (!name) {
    ret = GSTD_BAD_VALUE;
    GST_ERROR_OBJECT (session,
        "Wrong query param provided, \"name\" doesn't exist");
    goto out;
  }

  message = g_strdup_printf ("delete %s %s", soup_uri_get_path (address), name);
  ret = gstd_parser_parse_cmd (session, message, output);
  g_free (message);

  out:
    return ret;
}

static void
do_request (SoupServer * server, SoupMessage * msg, GHashTable * query,
    GstdSession * session, GstdHttpVerbs request_verb)
{
  gchar *response = NULL;
  gchar *name = NULL;
  gchar *description_pipe = NULL;
  SoupURI *address = NULL;
  GstdReturnCode ret = GSTD_EOK;
  gchar *output = NULL;
  const gchar *description = NULL;
  SoupStatus status = SOUP_STATUS_OK;
  const gchar *query_text = NULL;

  g_return_if_fail (server);
  g_return_if_fail (msg);
  g_return_if_fail (session);

  address = soup_message_get_uri (msg);

  query_text = soup_uri_get_query (address);

  if (!(msg->method == SOUP_METHOD_GET)) {
    if (!query_text) {
      ret = GSTD_BAD_VALUE;
      GST_ERROR_OBJECT (session, "No query params provided");
      goto out;
    }
    name = g_hash_table_lookup (query, "name");
    description_pipe = g_hash_table_lookup (query, "description");
  }


  if (msg->method == SOUP_METHOD_GET) {
    ret = do_get (server, msg, &output, session);
  } else if (msg->method == SOUP_METHOD_POST) {
    ret = do_post (server, msg, name, description_pipe, &output, session);
  } else if (msg->method == SOUP_METHOD_PUT) {
    ret = do_put (server, msg, name, &output, session);
  } else if (msg->method == SOUP_METHOD_DELETE) {
    ret = do_delete (server, msg, name, &output, session);
  } else {
    soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);
  }

  description = gstd_return_code_to_string (ret);
  response =
      g_strdup_printf
      ("{\n  \"code\" : %d,\n  \"description\" : \"%s\",\n  \"response\" : %s\n}",
      ret, description, output ? output : "null");
  g_free (output);
  soup_message_set_response (msg, "application/json", SOUP_MEMORY_COPY,
      response, strlen (response));
  g_free (response);

out:
  status = get_status_code (ret);
  soup_message_set_status (msg, status);
  return;
}

static void
server_callback (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query,
    SoupClientContext * context, gpointer data)
{
  GstdSession *session = NULL;

  g_return_if_fail (server);
  g_return_if_fail (msg);
  g_return_if_fail (data);

  session = GSTD_SESSION (data);
  g_return_if_fail (session);


  soup_message_headers_append (msg->response_headers,
      "Access-Control-Allow-Origin", "*");
  soup_message_headers_append (msg->response_headers,
      "Access-Control-Allow-Headers", "origin,range,content-type");
  soup_message_headers_append (msg->response_headers,
      "Access-Control-Allow-Methods", "PUT, GET, POST, DELETE");
  do_request (server, msg, query, session, GET);

}

static GstdReturnCode
gstd_http_start (GstdIpc * base, GstdSession * session)
{
  GError *error = NULL;
  GSocketAddress *sa;

  g_return_val_if_fail (base, GSTD_NULL_ARGUMENT);
  g_return_val_if_fail (session, GSTD_NULL_ARGUMENT);

  GstdHttp *self = GSTD_HTTP (base);
  guint16 port = self->port;
  gchar *address = self->address;

  gstd_http_stop (base);

  GST_DEBUG_OBJECT (self, "Initializing HTTP server");
  self->server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "Gstd-1.0", NULL);
  if (!self->server) {
    goto noconnection;
  }

  sa = g_inet_socket_address_new_from_string (address, port);

  soup_server_listen (self->server, sa, 0, &error);
  if (error) {
    goto noconnection;
  }

  soup_server_add_handler (self->server, NULL, server_callback, session, NULL);

  return GSTD_EOK;

noconnection:
  {
    GST_ERROR_OBJECT (self, "%s", error->message);
    g_printerr ("%s\n", error->message);
    g_error_free (error);
    g_object_unref (self->server);
    return GSTD_NO_CONNECTION;
  }
}

static gboolean
gstd_http_init_get_option_group (GstdIpc * base, GOptionGroup ** group)
{
  g_return_val_if_fail (base, FALSE);
  g_return_val_if_fail (group, FALSE);

  GstdHttp *self = GSTD_HTTP (base);

  GOptionEntry http_args[] = {
    {"enable-http-protocol", 't', 0, G_OPTION_ARG_NONE, &base->enabled,
        "Enable attach the server through given HTTP ports ", NULL}
    ,
    {"http-address", 'a', 0, G_OPTION_ARG_STRING, &self->address,
          "Attach to the server through a given address (default 127.0.0.1)",
        "http-address"}
    ,
    {"http-port", 'p', 0, G_OPTION_ARG_INT, &self->port,
          "Attach to the server through a given port (default 5001)",
        "http-port"}
    ,
    {NULL}
  };
  GST_DEBUG_OBJECT (self, "HTTP init group callback ");
  *group = g_option_group_new ("gstd-http", ("HTTP Options"),
      ("Show HTTP Options"), NULL, NULL);

  g_option_group_add_entries (*group, http_args);
  return TRUE;
}

static GstdReturnCode
gstd_http_stop (GstdIpc * base)
{
  g_return_val_if_fail (base, GSTD_NULL_ARGUMENT);

  GstdHttp *self = GSTD_HTTP (base);
  GstdSession *session = base->session;

  GST_DEBUG_OBJECT (self, "Entering HTTP server stop ");
  GST_INFO_OBJECT (session, "Closing HTTP server connection for %s",
      GSTD_OBJECT_NAME (session));
  if(self->server){
    g_object_unref (self->server);
  }

  return GSTD_EOK;
}
