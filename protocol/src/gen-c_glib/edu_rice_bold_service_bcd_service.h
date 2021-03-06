/**
 * Autogenerated by Thrift Compiler (0.9.3)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef EDU_RICE_BOLD_SERVICE_BCD_SERVICE_H
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_H

#include <thrift/c_glib/processor/thrift_dispatch_processor.h>

#include "edu_rice_bold_service_broadcast_types.h"

/* BcdService service interface */
typedef struct _edu_rice_bold_serviceBcdServiceIf edu_rice_bold_serviceBcdServiceIf;  /* dummy object */

struct _edu_rice_bold_serviceBcdServiceIfInterface
{
  GTypeInterface parent;

  gboolean (*push) (edu_rice_bold_serviceBcdServiceIf *iface, edu_rice_bold_servicePushReply ** _return, const edu_rice_bold_serviceServerID master, const GHashTable * slaves, const edu_rice_bold_serviceBcdInfo * data, GError **error);
  gboolean (*unpush) (edu_rice_bold_serviceBcdServiceIf *iface, const gint32 xid, GError **error);
};
typedef struct _edu_rice_bold_serviceBcdServiceIfInterface edu_rice_bold_serviceBcdServiceIfInterface;

GType edu_rice_bold_service_bcd_service_if_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_IF (edu_rice_bold_service_bcd_service_if_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_IF(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_IF, edu_rice_bold_serviceBcdServiceIf))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_IF(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_IF))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_IF_GET_INTERFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_IF, edu_rice_bold_serviceBcdServiceIfInterface))

gboolean edu_rice_bold_service_bcd_service_if_push (edu_rice_bold_serviceBcdServiceIf *iface, edu_rice_bold_servicePushReply ** _return, const edu_rice_bold_serviceServerID master, const GHashTable * slaves, const edu_rice_bold_serviceBcdInfo * data, GError **error);
gboolean edu_rice_bold_service_bcd_service_if_unpush (edu_rice_bold_serviceBcdServiceIf *iface, const gint32 xid, GError **error);

/* BcdService service client */
struct _edu_rice_bold_serviceBcdServiceClient
{
  GObject parent;

  ThriftProtocol *input_protocol;
  ThriftProtocol *output_protocol;
};
typedef struct _edu_rice_bold_serviceBcdServiceClient edu_rice_bold_serviceBcdServiceClient;

struct _edu_rice_bold_serviceBcdServiceClientClass
{
  GObjectClass parent;
};
typedef struct _edu_rice_bold_serviceBcdServiceClientClass edu_rice_bold_serviceBcdServiceClientClass;

GType edu_rice_bold_service_bcd_service_client_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT (edu_rice_bold_service_bcd_service_client_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT, edu_rice_bold_serviceBcdServiceClient))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_CLIENT_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT, edu_rice_bold_serviceBcdServiceClientClass))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_IS_CLIENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_IS_CLIENT_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_CLIENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_CLIENT, edu_rice_bold_serviceBcdServiceClientClass))

gboolean edu_rice_bold_service_bcd_service_client_push (edu_rice_bold_serviceBcdServiceIf * iface, edu_rice_bold_servicePushReply ** _return, const edu_rice_bold_serviceServerID master, const GHashTable * slaves, const edu_rice_bold_serviceBcdInfo * data, GError ** error);
gboolean edu_rice_bold_service_bcd_service_client_send_push (edu_rice_bold_serviceBcdServiceIf * iface, const edu_rice_bold_serviceServerID master, const GHashTable * slaves, const edu_rice_bold_serviceBcdInfo * data, GError ** error);
gboolean edu_rice_bold_service_bcd_service_client_recv_push (edu_rice_bold_serviceBcdServiceIf * iface, edu_rice_bold_servicePushReply ** _return, GError ** error);
gboolean edu_rice_bold_service_bcd_service_client_unpush (edu_rice_bold_serviceBcdServiceIf * iface, const gint32 xid, GError ** error);
gboolean edu_rice_bold_service_bcd_service_client_send_unpush (edu_rice_bold_serviceBcdServiceIf * iface, const gint32 xid, GError ** error);
void bcd_service_client_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
void bcd_service_client_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);

/* BcdService handler (abstract base class) */
struct _edu_rice_bold_serviceBcdServiceHandler
{
  GObject parent;
};
typedef struct _edu_rice_bold_serviceBcdServiceHandler edu_rice_bold_serviceBcdServiceHandler;

struct _edu_rice_bold_serviceBcdServiceHandlerClass
{
  GObjectClass parent;

  gboolean (*push) (edu_rice_bold_serviceBcdServiceIf *iface, edu_rice_bold_servicePushReply ** _return, const edu_rice_bold_serviceServerID master, const GHashTable * slaves, const edu_rice_bold_serviceBcdInfo * data, GError **error);
  gboolean (*unpush) (edu_rice_bold_serviceBcdServiceIf *iface, const gint32 xid, GError **error);
};
typedef struct _edu_rice_bold_serviceBcdServiceHandlerClass edu_rice_bold_serviceBcdServiceHandlerClass;

GType edu_rice_bold_service_bcd_service_handler_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_HANDLER (edu_rice_bold_service_bcd_service_handler_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_HANDLER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_HANDLER, edu_rice_bold_serviceBcdServiceHandler))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_HANDLER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_HANDLER))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_HANDLER_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_HANDLER, edu_rice_bold_serviceBcdServiceHandlerClass))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_HANDLER_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_HANDLER))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_HANDLER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_HANDLER, edu_rice_bold_serviceBcdServiceHandlerClass))

gboolean edu_rice_bold_service_bcd_service_handler_push (edu_rice_bold_serviceBcdServiceIf *iface, edu_rice_bold_servicePushReply ** _return, const edu_rice_bold_serviceServerID master, const GHashTable * slaves, const edu_rice_bold_serviceBcdInfo * data, GError **error);
gboolean edu_rice_bold_service_bcd_service_handler_unpush (edu_rice_bold_serviceBcdServiceIf *iface, const gint32 xid, GError **error);

/* BcdService processor */
struct _edu_rice_bold_serviceBcdServiceProcessor
{
  ThriftDispatchProcessor parent;

  /* protected */
  edu_rice_bold_serviceBcdServiceHandler *handler;
  GHashTable *process_map;
};
typedef struct _edu_rice_bold_serviceBcdServiceProcessor edu_rice_bold_serviceBcdServiceProcessor;

struct _edu_rice_bold_serviceBcdServiceProcessorClass
{
  ThriftDispatchProcessorClass parent;

  /* protected */
  gboolean (*dispatch_call) (ThriftDispatchProcessor *processor,
                             ThriftProtocol *in,
                             ThriftProtocol *out,
                             gchar *fname,
                             gint32 seqid,
                             GError **error);
};
typedef struct _edu_rice_bold_serviceBcdServiceProcessorClass edu_rice_bold_serviceBcdServiceProcessorClass;

GType edu_rice_bold_service_bcd_service_processor_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PROCESSOR (edu_rice_bold_service_bcd_service_processor_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PROCESSOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PROCESSOR, edu_rice_bold_serviceBcdServiceProcessor))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_PROCESSOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PROCESSOR))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PROCESSOR_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PROCESSOR, edu_rice_bold_serviceBcdServiceProcessorClass))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_PROCESSOR_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PROCESSOR))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PROCESSOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PROCESSOR, edu_rice_bold_serviceBcdServiceProcessorClass))

#endif /* EDU_RICE_BOLD_SERVICE_BCD_SERVICE_H */
