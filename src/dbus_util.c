#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * Call a method on a remote object
 */
void dbus_query(char* param)
{
    DBusMessage* msg;
    DBusMessageIter args;
    DBusConnection* conn;
    DBusError err;
    DBusPendingCall* pending;
    int ret;

    printf("Calling remote method with %s\n", param);

    // initialiset the errors
    dbus_error_init(&err);

    // connect to the system bus and check for errors
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }
    if (NULL == conn) {
        exit(1);
    }

    // request our name on the bus
    ret = dbus_bus_request_name(conn, "com.example.SampleService", 0, &err);
    if (dbus_error_is_set(&err)) {
        fprintf(stderr, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    // create a new method call and check for errors
    msg = dbus_message_new_method_call("com.example.SampleService", // target for the method call
            "/SomeObject", // object to call on
            "com.example.SampleInterface", // interface to call on
            "HelloWorld"); // method name
    if (NULL == msg) {
        fprintf(stderr, "Message Null\n");
        exit(1);
    }

    // append arguments
    dbus_message_iter_init_append(msg, &args);
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param)) {
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }

    // send message and get a handle for a reply
    if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
        fprintf(stderr, "Out Of Memory!\n");
        exit(1);
    }
    if (NULL == pending) {
        fprintf(stderr, "Pending Call Null\n");
        exit(1);
    }
    dbus_connection_flush(conn);

    printf("Request Sent\n");

    // free message
    dbus_message_unref(msg);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if (NULL == msg) {
        fprintf(stderr, "Reply Null\n");
        exit(1);
    }
    // free the pending message handle
    dbus_pending_call_unref(pending);

    // read the parameters
    if (!dbus_message_iter_init(msg, &args))
        fprintf(stderr, "Message has no arguments!\n");
    else {
        int ctype;

        ctype = dbus_message_iter_get_arg_type(&args);
        if (ctype == DBUS_TYPE_ARRAY) {
            DBusMessageIter dict;
            dbus_message_iter_recurse(&args, &dict);

            while ((ctype = dbus_message_iter_get_arg_type(&dict)) !=
                    DBUS_TYPE_INVALID) {
                DBusMessageIter entry;
                const char *key;

                if (ctype == DBUS_TYPE_STRING) {
                    dbus_message_iter_get_basic(&dict, &key);
                    fprintf(stderr, "ret_str - %s\n", key);
                }

                //dbus_message_iter_next(&entry);
                dbus_message_iter_next(&dict);
            }
        }
    }

    // free reply and close connection
    dbus_message_unref(msg);
    dbus_connection_unref(conn);
}

