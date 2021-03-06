/**
 * Autogenerated by Thrift Compiler (0.9.3)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef EDU_RICE_BOLD_SERVICE_BROADCAST_TYPES_H
#define EDU_RICE_BOLD_SERVICE_BROADCAST_TYPES_H

/* base includes */
#include <glib-object.h>
#include <thrift/c_glib/thrift_struct.h>
#include <thrift/c_glib/protocol/thrift_protocol.h>

/* custom thrift includes */

/* begin types */

enum _edu_rice_bold_servicePushReplyCmd {
  EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_PUSH = 0,
  EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_POSTPONE = 1,
  EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CMD_DENY = 2
};
typedef enum _edu_rice_bold_servicePushReplyCmd edu_rice_bold_servicePushReplyCmd;

/* return the name of the constant */
const char *
toString_PushReplyCmd(int value); 

typedef gchar * edu_rice_bold_serviceServerID;

typedef gchar * edu_rice_bold_serviceBcdID;

typedef gint32 edu_rice_bold_serviceBcdSize;

/* struct BcdInfo */
struct _edu_rice_bold_serviceBcdInfo
{ 
  ThriftStruct parent; 

  /* public */
  gchar * id;
  gboolean __isset_id;
  gint32 size;
  gboolean __isset_size;
};
typedef struct _edu_rice_bold_serviceBcdInfo edu_rice_bold_serviceBcdInfo;

struct _edu_rice_bold_serviceBcdInfoClass
{
  ThriftStructClass parent;
};
typedef struct _edu_rice_bold_serviceBcdInfoClass edu_rice_bold_serviceBcdInfoClass;

GType edu_rice_bold_service_bcd_info_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_INFO (edu_rice_bold_service_bcd_info_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_INFO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_INFO, edu_rice_bold_serviceBcdInfo))
#define EDU_RICE_BOLD_SERVICE_BCD_INFO_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE__TYPE_BCD_INFO, edu_rice_bold_serviceBcdInfoClass))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_INFO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_INFO))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_INFO_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_INFO))
#define EDU_RICE_BOLD_SERVICE_BCD_INFO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_INFO, edu_rice_bold_serviceBcdInfoClass))

/* struct PushReply */
struct _edu_rice_bold_servicePushReply
{ 
  ThriftStruct parent; 

  /* public */
  gint32 xid;
  gboolean __isset_xid;
  edu_rice_bold_servicePushReplyCmd cmd;
  gboolean __isset_cmd;
};
typedef struct _edu_rice_bold_servicePushReply edu_rice_bold_servicePushReply;

struct _edu_rice_bold_servicePushReplyClass
{
  ThriftStructClass parent;
};
typedef struct _edu_rice_bold_servicePushReplyClass edu_rice_bold_servicePushReplyClass;

GType edu_rice_bold_service_push_reply_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_PUSH_REPLY (edu_rice_bold_service_push_reply_get_type())
#define EDU_RICE_BOLD_SERVICE_PUSH_REPLY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_PUSH_REPLY, edu_rice_bold_servicePushReply))
#define EDU_RICE_BOLD_SERVICE_PUSH_REPLY_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE__TYPE_PUSH_REPLY, edu_rice_bold_servicePushReplyClass))
#define EDU_RICE_BOLD_SERVICE_IS_PUSH_REPLY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_PUSH_REPLY))
#define EDU_RICE_BOLD_SERVICE_IS_PUSH_REPLY_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_PUSH_REPLY))
#define EDU_RICE_BOLD_SERVICE_PUSH_REPLY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_PUSH_REPLY, edu_rice_bold_servicePushReplyClass))

/* constants */

/* struct BcdServicePushArgs */
struct _edu_rice_bold_serviceBcdServicePushArgs
{ 
  ThriftStruct parent; 

  /* public */
  gchar * master;
  gboolean __isset_master;
  GHashTable * slaves;
  gboolean __isset_slaves;
  edu_rice_bold_serviceBcdInfo * data;
  gboolean __isset_data;
};
typedef struct _edu_rice_bold_serviceBcdServicePushArgs edu_rice_bold_serviceBcdServicePushArgs;

struct _edu_rice_bold_serviceBcdServicePushArgsClass
{
  ThriftStructClass parent;
};
typedef struct _edu_rice_bold_serviceBcdServicePushArgsClass edu_rice_bold_serviceBcdServicePushArgsClass;

GType edu_rice_bold_service_bcd_service_push_args_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_ARGS (edu_rice_bold_service_bcd_service_push_args_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PUSH_ARGS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_ARGS, edu_rice_bold_serviceBcdServicePushArgs))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PUSH_ARGS_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE__TYPE_BCD_SERVICE_PUSH_ARGS, edu_rice_bold_serviceBcdServicePushArgsClass))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_PUSH_ARGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_ARGS))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_PUSH_ARGS_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_ARGS))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PUSH_ARGS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_ARGS, edu_rice_bold_serviceBcdServicePushArgsClass))

/* struct BcdServicePushResult */
struct _edu_rice_bold_serviceBcdServicePushResult
{ 
  ThriftStruct parent; 

  /* public */
  edu_rice_bold_servicePushReply * success;
  gboolean __isset_success;
};
typedef struct _edu_rice_bold_serviceBcdServicePushResult edu_rice_bold_serviceBcdServicePushResult;

struct _edu_rice_bold_serviceBcdServicePushResultClass
{
  ThriftStructClass parent;
};
typedef struct _edu_rice_bold_serviceBcdServicePushResultClass edu_rice_bold_serviceBcdServicePushResultClass;

GType edu_rice_bold_service_bcd_service_push_result_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_RESULT (edu_rice_bold_service_bcd_service_push_result_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PUSH_RESULT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_RESULT, edu_rice_bold_serviceBcdServicePushResult))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PUSH_RESULT_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE__TYPE_BCD_SERVICE_PUSH_RESULT, edu_rice_bold_serviceBcdServicePushResultClass))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_PUSH_RESULT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_RESULT))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_PUSH_RESULT_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_RESULT))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_PUSH_RESULT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_PUSH_RESULT, edu_rice_bold_serviceBcdServicePushResultClass))

/* struct BcdServiceUnpushArgs */
struct _edu_rice_bold_serviceBcdServiceUnpushArgs
{ 
  ThriftStruct parent; 

  /* public */
  gint32 xid;
  gboolean __isset_xid;
};
typedef struct _edu_rice_bold_serviceBcdServiceUnpushArgs edu_rice_bold_serviceBcdServiceUnpushArgs;

struct _edu_rice_bold_serviceBcdServiceUnpushArgsClass
{
  ThriftStructClass parent;
};
typedef struct _edu_rice_bold_serviceBcdServiceUnpushArgsClass edu_rice_bold_serviceBcdServiceUnpushArgsClass;

GType edu_rice_bold_service_bcd_service_unpush_args_get_type (void);
#define EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_UNPUSH_ARGS (edu_rice_bold_service_bcd_service_unpush_args_get_type())
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_UNPUSH_ARGS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_UNPUSH_ARGS, edu_rice_bold_serviceBcdServiceUnpushArgs))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_UNPUSH_ARGS_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), EDU_RICE_BOLD_SERVICE__TYPE_BCD_SERVICE_UNPUSH_ARGS, edu_rice_bold_serviceBcdServiceUnpushArgsClass))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_UNPUSH_ARGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_UNPUSH_ARGS))
#define EDU_RICE_BOLD_SERVICE_IS_BCD_SERVICE_UNPUSH_ARGS_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_UNPUSH_ARGS))
#define EDU_RICE_BOLD_SERVICE_BCD_SERVICE_UNPUSH_ARGS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EDU_RICE_BOLD_SERVICE_TYPE_BCD_SERVICE_UNPUSH_ARGS, edu_rice_bold_serviceBcdServiceUnpushArgsClass))

#endif /* EDU_RICE_BOLD_SERVICE_BROADCAST_TYPES_H */
