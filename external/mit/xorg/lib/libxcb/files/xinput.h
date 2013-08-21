/*
 * This file generated automatically from xinput.xml by c_client.py.
 * Edit at your peril.
 */

/**
 * @defgroup XCB_Input_API XCB Input API
 * @brief Input XCB Protocol Implementation.
 * @{
 **/

#ifndef __XINPUT_H
#define __XINPUT_H

#include "xcb.h"
#include "xproto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XCB_INPUT_MAJOR_VERSION 1
#define XCB_INPUT_MINOR_VERSION 4
  
extern xcb_extension_t xcb_input_id;

typedef uint8_t xcb_input_key_code_t;

/**
 * @brief xcb_input_key_code_iterator_t
 **/
typedef struct xcb_input_key_code_iterator_t {
    xcb_input_key_code_t *data; /**<  */
    int                   rem; /**<  */
    int                   index; /**<  */
} xcb_input_key_code_iterator_t;

typedef uint32_t xcb_input_event_class_t;

/**
 * @brief xcb_input_event_class_iterator_t
 **/
typedef struct xcb_input_event_class_iterator_t {
    xcb_input_event_class_t *data; /**<  */
    int                      rem; /**<  */
    int                      index; /**<  */
} xcb_input_event_class_iterator_t;

typedef enum xcb_input_valuator_mode_t {
    XCB_INPUT_VALUATOR_MODE_RELATIVE = 0,
    XCB_INPUT_VALUATOR_MODE_ABSOLUTE = 1
} xcb_input_valuator_mode_t;

typedef enum xcb_input_propagate_mode_t {
    XCB_INPUT_PROPAGATE_MODE_ADD_TO_LIST = 0,
    XCB_INPUT_PROPAGATE_MODE_DELETE_FROM_LIST = 1
} xcb_input_propagate_mode_t;

/**
 * @brief xcb_input_get_extension_version_cookie_t
 **/
typedef struct xcb_input_get_extension_version_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_extension_version_cookie_t;

/** Opcode for xcb_input_get_extension_version. */
#define XCB_INPUT_GET_EXTENSION_VERSION 1

/**
 * @brief xcb_input_get_extension_version_request_t
 **/
typedef struct xcb_input_get_extension_version_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint16_t name_len; /**<  */
    uint8_t  pad0[2]; /**<  */
} xcb_input_get_extension_version_request_t;

/**
 * @brief xcb_input_get_extension_version_reply_t
 **/
typedef struct xcb_input_get_extension_version_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t server_major; /**<  */
    uint16_t server_minor; /**<  */
    uint8_t  present; /**<  */
    uint8_t  pad1[19]; /**<  */
} xcb_input_get_extension_version_reply_t;

typedef enum xcb_input_device_use_t {
    XCB_INPUT_DEVICE_USE_IS_X_POINTER = 0,
    XCB_INPUT_DEVICE_USE_IS_X_KEYBOARD = 1,
    XCB_INPUT_DEVICE_USE_IS_X_EXTENSION_DEVICE = 2,
    XCB_INPUT_DEVICE_USE_IS_X_EXTENSION_KEYBOARD = 3,
    XCB_INPUT_DEVICE_USE_IS_X_EXTENSION_POINTER = 4
} xcb_input_device_use_t;

/**
 * @brief xcb_input_device_info_t
 **/
typedef struct xcb_input_device_info_t {
    xcb_atom_t device_type; /**<  */
    uint8_t    device_id; /**<  */
    uint8_t    num_class_info; /**<  */
    uint8_t    device_use; /**<  */
    uint8_t    pad0; /**<  */
} xcb_input_device_info_t;

/**
 * @brief xcb_input_device_info_iterator_t
 **/
typedef struct xcb_input_device_info_iterator_t {
    xcb_input_device_info_t *data; /**<  */
    int                      rem; /**<  */
    int                      index; /**<  */
} xcb_input_device_info_iterator_t;

/**
 * @brief xcb_input_list_input_devices_cookie_t
 **/
typedef struct xcb_input_list_input_devices_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_list_input_devices_cookie_t;

/** Opcode for xcb_input_list_input_devices. */
#define XCB_INPUT_LIST_INPUT_DEVICES 2

/**
 * @brief xcb_input_list_input_devices_request_t
 **/
typedef struct xcb_input_list_input_devices_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
} xcb_input_list_input_devices_request_t;

/**
 * @brief xcb_input_list_input_devices_reply_t
 **/
typedef struct xcb_input_list_input_devices_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  devices_len; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_list_input_devices_reply_t;

typedef enum xcb_input_input_class_t {
    XCB_INPUT_INPUT_CLASS_KEY = 0,
    XCB_INPUT_INPUT_CLASS_BUTTON = 1,
    XCB_INPUT_INPUT_CLASS_VALUATOR = 2,
    XCB_INPUT_INPUT_CLASS_FEEDBACK = 3,
    XCB_INPUT_INPUT_CLASS_PROXIMITY = 4,
    XCB_INPUT_INPUT_CLASS_FOCUS = 5,
    XCB_INPUT_INPUT_CLASS_OTHER = 6
} xcb_input_input_class_t;

/**
 * @brief xcb_input_input_info_t
 **/
typedef struct xcb_input_input_info_t {
    uint8_t class_id; /**<  */
    uint8_t len; /**<  */
} xcb_input_input_info_t;

/**
 * @brief xcb_input_input_info_iterator_t
 **/
typedef struct xcb_input_input_info_iterator_t {
    xcb_input_input_info_t *data; /**<  */
    int                     rem; /**<  */
    int                     index; /**<  */
} xcb_input_input_info_iterator_t;

/**
 * @brief xcb_input_key_info_t
 **/
typedef struct xcb_input_key_info_t {
    uint8_t              class_id; /**<  */
    uint8_t              len; /**<  */
    xcb_input_key_code_t min_keycode; /**<  */
    xcb_input_key_code_t max_keycode; /**<  */
    uint16_t             num_keys; /**<  */
    uint8_t              pad0[2]; /**<  */
} xcb_input_key_info_t;

/**
 * @brief xcb_input_key_info_iterator_t
 **/
typedef struct xcb_input_key_info_iterator_t {
    xcb_input_key_info_t *data; /**<  */
    int                   rem; /**<  */
    int                   index; /**<  */
} xcb_input_key_info_iterator_t;

/**
 * @brief xcb_input_button_info_t
 **/
typedef struct xcb_input_button_info_t {
    uint8_t  class_id; /**<  */
    uint8_t  len; /**<  */
    uint16_t num_buttons; /**<  */
} xcb_input_button_info_t;

/**
 * @brief xcb_input_button_info_iterator_t
 **/
typedef struct xcb_input_button_info_iterator_t {
    xcb_input_button_info_t *data; /**<  */
    int                      rem; /**<  */
    int                      index; /**<  */
} xcb_input_button_info_iterator_t;

/**
 * @brief xcb_input_axis_info_t
 **/
typedef struct xcb_input_axis_info_t {
    uint32_t resolution; /**<  */
    int32_t  minimum; /**<  */
    int32_t  maximum; /**<  */
} xcb_input_axis_info_t;

/**
 * @brief xcb_input_axis_info_iterator_t
 **/
typedef struct xcb_input_axis_info_iterator_t {
    xcb_input_axis_info_t *data; /**<  */
    int                    rem; /**<  */
    int                    index; /**<  */
} xcb_input_axis_info_iterator_t;

/**
 * @brief xcb_input_valuator_info_t
 **/
typedef struct xcb_input_valuator_info_t {
    uint8_t  class_id; /**<  */
    uint8_t  len; /**<  */
    uint8_t  axes_len; /**<  */
    uint8_t  mode; /**<  */
    uint32_t motion_size; /**<  */
} xcb_input_valuator_info_t;

/**
 * @brief xcb_input_valuator_info_iterator_t
 **/
typedef struct xcb_input_valuator_info_iterator_t {
    xcb_input_valuator_info_t *data; /**<  */
    int                        rem; /**<  */
    int                        index; /**<  */
} xcb_input_valuator_info_iterator_t;

/**
 * @brief xcb_input_input_class_info_t
 **/
typedef struct xcb_input_input_class_info_t {
    uint8_t class_id; /**<  */
    uint8_t event_type_base; /**<  */
} xcb_input_input_class_info_t;

/**
 * @brief xcb_input_input_class_info_iterator_t
 **/
typedef struct xcb_input_input_class_info_iterator_t {
    xcb_input_input_class_info_t *data; /**<  */
    int                           rem; /**<  */
    int                           index; /**<  */
} xcb_input_input_class_info_iterator_t;

/**
 * @brief xcb_input_open_device_cookie_t
 **/
typedef struct xcb_input_open_device_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_open_device_cookie_t;

/** Opcode for xcb_input_open_device. */
#define XCB_INPUT_OPEN_DEVICE 3

/**
 * @brief xcb_input_open_device_request_t
 **/
typedef struct xcb_input_open_device_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_open_device_request_t;

/**
 * @brief xcb_input_open_device_reply_t
 **/
typedef struct xcb_input_open_device_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  num_classes; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_open_device_reply_t;

/** Opcode for xcb_input_close_device. */
#define XCB_INPUT_CLOSE_DEVICE 4

/**
 * @brief xcb_input_close_device_request_t
 **/
typedef struct xcb_input_close_device_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_close_device_request_t;

/**
 * @brief xcb_input_set_device_mode_cookie_t
 **/
typedef struct xcb_input_set_device_mode_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_set_device_mode_cookie_t;

/** Opcode for xcb_input_set_device_mode. */
#define XCB_INPUT_SET_DEVICE_MODE 5

/**
 * @brief xcb_input_set_device_mode_request_t
 **/
typedef struct xcb_input_set_device_mode_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  mode; /**<  */
    uint8_t  pad0[2]; /**<  */
} xcb_input_set_device_mode_request_t;

/**
 * @brief xcb_input_set_device_mode_reply_t
 **/
typedef struct xcb_input_set_device_mode_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_set_device_mode_reply_t;

/** Opcode for xcb_input_select_extension_event. */
#define XCB_INPUT_SELECT_EXTENSION_EVENT 6

/**
 * @brief xcb_input_select_extension_event_request_t
 **/
typedef struct xcb_input_select_extension_event_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t window; /**<  */
    uint16_t     num_classes; /**<  */
    uint8_t      pad0[2]; /**<  */
} xcb_input_select_extension_event_request_t;

/**
 * @brief xcb_input_get_selected_extension_events_cookie_t
 **/
typedef struct xcb_input_get_selected_extension_events_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_selected_extension_events_cookie_t;

/** Opcode for xcb_input_get_selected_extension_events. */
#define XCB_INPUT_GET_SELECTED_EXTENSION_EVENTS 7

/**
 * @brief xcb_input_get_selected_extension_events_request_t
 **/
typedef struct xcb_input_get_selected_extension_events_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t window; /**<  */
} xcb_input_get_selected_extension_events_request_t;

/**
 * @brief xcb_input_get_selected_extension_events_reply_t
 **/
typedef struct xcb_input_get_selected_extension_events_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t num_this_classes; /**<  */
    uint16_t num_all_classes; /**<  */
    uint8_t  pad1[20]; /**<  */
} xcb_input_get_selected_extension_events_reply_t;

/** Opcode for xcb_input_change_device_dont_propagate_list. */
#define XCB_INPUT_CHANGE_DEVICE_DONT_PROPAGATE_LIST 8

/**
 * @brief xcb_input_change_device_dont_propagate_list_request_t
 **/
typedef struct xcb_input_change_device_dont_propagate_list_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t window; /**<  */
    uint16_t     num_classes; /**<  */
    uint8_t      mode; /**<  */
    uint8_t      pad0; /**<  */
} xcb_input_change_device_dont_propagate_list_request_t;

/**
 * @brief xcb_input_get_device_dont_propagate_list_cookie_t
 **/
typedef struct xcb_input_get_device_dont_propagate_list_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_dont_propagate_list_cookie_t;

/** Opcode for xcb_input_get_device_dont_propagate_list. */
#define XCB_INPUT_GET_DEVICE_DONT_PROPAGATE_LIST 9

/**
 * @brief xcb_input_get_device_dont_propagate_list_request_t
 **/
typedef struct xcb_input_get_device_dont_propagate_list_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t window; /**<  */
} xcb_input_get_device_dont_propagate_list_request_t;

/**
 * @brief xcb_input_get_device_dont_propagate_list_reply_t
 **/
typedef struct xcb_input_get_device_dont_propagate_list_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t num_classes; /**<  */
    uint8_t  pad1[22]; /**<  */
} xcb_input_get_device_dont_propagate_list_reply_t;

/**
 * @brief xcb_input_get_device_motion_events_cookie_t
 **/
typedef struct xcb_input_get_device_motion_events_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_motion_events_cookie_t;

/** Opcode for xcb_input_get_device_motion_events. */
#define XCB_INPUT_GET_DEVICE_MOTION_EVENTS 10

/**
 * @brief xcb_input_get_device_motion_events_request_t
 **/
typedef struct xcb_input_get_device_motion_events_request_t {
    uint8_t         major_opcode; /**<  */
    uint8_t         minor_opcode; /**<  */
    uint16_t        length; /**<  */
    xcb_timestamp_t start; /**<  */
    xcb_timestamp_t stop; /**<  */
    uint8_t         device_id; /**<  */
} xcb_input_get_device_motion_events_request_t;

/**
 * @brief xcb_input_get_device_motion_events_reply_t
 **/
typedef struct xcb_input_get_device_motion_events_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t num_coords; /**<  */
    uint8_t  num_axes; /**<  */
    uint8_t  device_mode; /**<  */
    uint8_t  pad1[18]; /**<  */
} xcb_input_get_device_motion_events_reply_t;

/**
 * @brief xcb_input_device_time_coord_t
 **/
typedef struct xcb_input_device_time_coord_t {
    xcb_timestamp_t time; /**<  */
} xcb_input_device_time_coord_t;

/**
 * @brief xcb_input_device_time_coord_iterator_t
 **/
typedef struct xcb_input_device_time_coord_iterator_t {
    xcb_input_device_time_coord_t *data; /**<  */
    int                            rem; /**<  */
    int                            index; /**<  */
} xcb_input_device_time_coord_iterator_t;

/**
 * @brief xcb_input_change_keyboard_device_cookie_t
 **/
typedef struct xcb_input_change_keyboard_device_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_change_keyboard_device_cookie_t;

/** Opcode for xcb_input_change_keyboard_device. */
#define XCB_INPUT_CHANGE_KEYBOARD_DEVICE 11

/**
 * @brief xcb_input_change_keyboard_device_request_t
 **/
typedef struct xcb_input_change_keyboard_device_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_change_keyboard_device_request_t;

/**
 * @brief xcb_input_change_keyboard_device_reply_t
 **/
typedef struct xcb_input_change_keyboard_device_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_change_keyboard_device_reply_t;

/**
 * @brief xcb_input_change_pointer_device_cookie_t
 **/
typedef struct xcb_input_change_pointer_device_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_change_pointer_device_cookie_t;

/** Opcode for xcb_input_change_pointer_device. */
#define XCB_INPUT_CHANGE_POINTER_DEVICE 12

/**
 * @brief xcb_input_change_pointer_device_request_t
 **/
typedef struct xcb_input_change_pointer_device_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  x_axis; /**<  */
    uint8_t  y_axis; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0; /**<  */
} xcb_input_change_pointer_device_request_t;

/**
 * @brief xcb_input_change_pointer_device_reply_t
 **/
typedef struct xcb_input_change_pointer_device_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_change_pointer_device_reply_t;

/**
 * @brief xcb_input_grab_device_cookie_t
 **/
typedef struct xcb_input_grab_device_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_grab_device_cookie_t;

/** Opcode for xcb_input_grab_device. */
#define XCB_INPUT_GRAB_DEVICE 13

/**
 * @brief xcb_input_grab_device_request_t
 **/
typedef struct xcb_input_grab_device_request_t {
    uint8_t         major_opcode; /**<  */
    uint8_t         minor_opcode; /**<  */
    uint16_t        length; /**<  */
    xcb_window_t    grab_window; /**<  */
    xcb_timestamp_t time; /**<  */
    uint16_t        num_classes; /**<  */
    uint8_t         this_device_mode; /**<  */
    uint8_t         other_device_mode; /**<  */
    uint8_t         owner_events; /**<  */
    uint8_t         device_id; /**<  */
    uint8_t         pad0[2]; /**<  */
} xcb_input_grab_device_request_t;

/**
 * @brief xcb_input_grab_device_reply_t
 **/
typedef struct xcb_input_grab_device_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_grab_device_reply_t;

/** Opcode for xcb_input_ungrab_device. */
#define XCB_INPUT_UNGRAB_DEVICE 14

/**
 * @brief xcb_input_ungrab_device_request_t
 **/
typedef struct xcb_input_ungrab_device_request_t {
    uint8_t         major_opcode; /**<  */
    uint8_t         minor_opcode; /**<  */
    uint16_t        length; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         device_id; /**<  */
} xcb_input_ungrab_device_request_t;

/** Opcode for xcb_input_grab_device_key. */
#define XCB_INPUT_GRAB_DEVICE_KEY 15

/**
 * @brief xcb_input_grab_device_key_request_t
 **/
typedef struct xcb_input_grab_device_key_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t grab_window; /**<  */
    uint16_t     num_classes; /**<  */
    uint16_t     modifiers; /**<  */
    uint8_t      modifier_device; /**<  */
    uint8_t      grabbed_device; /**<  */
    uint8_t      key; /**<  */
    uint8_t      this_device_mode; /**<  */
    uint8_t      other_device_mode; /**<  */
    uint8_t      owner_events; /**<  */
    uint8_t      pad0[2]; /**<  */
} xcb_input_grab_device_key_request_t;

/** Opcode for xcb_input_ungrab_device_key. */
#define XCB_INPUT_UNGRAB_DEVICE_KEY 16

/**
 * @brief xcb_input_ungrab_device_key_request_t
 **/
typedef struct xcb_input_ungrab_device_key_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t grabWindow; /**<  */
    uint16_t     modifiers; /**<  */
    uint8_t      modifier_device; /**<  */
    uint8_t      key; /**<  */
    uint8_t      grabbed_device; /**<  */
} xcb_input_ungrab_device_key_request_t;

/** Opcode for xcb_input_grab_device_button. */
#define XCB_INPUT_GRAB_DEVICE_BUTTON 17

/**
 * @brief xcb_input_grab_device_button_request_t
 **/
typedef struct xcb_input_grab_device_button_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t grab_window; /**<  */
    uint8_t      grabbed_device; /**<  */
    uint8_t      modifier_device; /**<  */
    uint16_t     num_classes; /**<  */
    uint16_t     modifiers; /**<  */
    uint8_t      this_device_mode; /**<  */
    uint8_t      other_device_mode; /**<  */
    uint8_t      button; /**<  */
    uint8_t      owner_events; /**<  */
    uint8_t      pad0[2]; /**<  */
} xcb_input_grab_device_button_request_t;

/** Opcode for xcb_input_ungrab_device_button. */
#define XCB_INPUT_UNGRAB_DEVICE_BUTTON 18

/**
 * @brief xcb_input_ungrab_device_button_request_t
 **/
typedef struct xcb_input_ungrab_device_button_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t grab_window; /**<  */
    uint16_t     modifiers; /**<  */
    uint8_t      modifier_device; /**<  */
    uint8_t      button; /**<  */
    uint8_t      grabbed_device; /**<  */
} xcb_input_ungrab_device_button_request_t;

typedef enum xcb_input_device_input_mode_t {
    XCB_INPUT_DEVICE_INPUT_MODE_ASYNC_THIS_DEVICE,
    XCB_INPUT_DEVICE_INPUT_MODE_SYNC_THIS_DEVICE,
    XCB_INPUT_DEVICE_INPUT_MODE_REPLAY_THIS_DEVICE,
    XCB_INPUT_DEVICE_INPUT_MODE_ASYNC_OTHER_DEVICES,
    XCB_INPUT_DEVICE_INPUT_MODE_ASYNC_ALL,
    XCB_INPUT_DEVICE_INPUT_MODE_SYNC_ALL
} xcb_input_device_input_mode_t;

/** Opcode for xcb_input_allow_device_events. */
#define XCB_INPUT_ALLOW_DEVICE_EVENTS 19

/**
 * @brief xcb_input_allow_device_events_request_t
 **/
typedef struct xcb_input_allow_device_events_request_t {
    uint8_t         major_opcode; /**<  */
    uint8_t         minor_opcode; /**<  */
    uint16_t        length; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         mode; /**<  */
    uint8_t         device_id; /**<  */
} xcb_input_allow_device_events_request_t;

/**
 * @brief xcb_input_get_device_focus_cookie_t
 **/
typedef struct xcb_input_get_device_focus_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_focus_cookie_t;

/** Opcode for xcb_input_get_device_focus. */
#define XCB_INPUT_GET_DEVICE_FOCUS 20

/**
 * @brief xcb_input_get_device_focus_request_t
 **/
typedef struct xcb_input_get_device_focus_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_get_device_focus_request_t;

/**
 * @brief xcb_input_get_device_focus_reply_t
 **/
typedef struct xcb_input_get_device_focus_reply_t {
    uint8_t         response_type; /**<  */
    uint8_t         pad0; /**<  */
    uint16_t        sequence; /**<  */
    uint32_t        length; /**<  */
    xcb_window_t    focus; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         revert_to; /**<  */
    uint8_t         pad1[15]; /**<  */
} xcb_input_get_device_focus_reply_t;

/** Opcode for xcb_input_set_device_focus. */
#define XCB_INPUT_SET_DEVICE_FOCUS 21

/**
 * @brief xcb_input_set_device_focus_request_t
 **/
typedef struct xcb_input_set_device_focus_request_t {
    uint8_t         major_opcode; /**<  */
    uint8_t         minor_opcode; /**<  */
    uint16_t        length; /**<  */
    xcb_window_t    focus; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         revert_to; /**<  */
    uint8_t         device_id; /**<  */
} xcb_input_set_device_focus_request_t;

/**
 * @brief xcb_input_get_feedback_control_cookie_t
 **/
typedef struct xcb_input_get_feedback_control_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_feedback_control_cookie_t;

/** Opcode for xcb_input_get_feedback_control. */
#define XCB_INPUT_GET_FEEDBACK_CONTROL 22

/**
 * @brief xcb_input_get_feedback_control_request_t
 **/
typedef struct xcb_input_get_feedback_control_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_get_feedback_control_request_t;

/**
 * @brief xcb_input_get_feedback_control_reply_t
 **/
typedef struct xcb_input_get_feedback_control_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t num_feedback; /**<  */
    uint8_t  pad1[22]; /**<  */
} xcb_input_get_feedback_control_reply_t;

typedef enum xcb_input_feedback_class_t {
    XCB_INPUT_FEEDBACK_CLASS_KEYBOARD,
    XCB_INPUT_FEEDBACK_CLASS_POINTER,
    XCB_INPUT_FEEDBACK_CLASS_STRING,
    XCB_INPUT_FEEDBACK_CLASS_INTEGER,
    XCB_INPUT_FEEDBACK_CLASS_LED,
    XCB_INPUT_FEEDBACK_CLASS_BELL
} xcb_input_feedback_class_t;

/**
 * @brief xcb_input_feedback_state_t
 **/
typedef struct xcb_input_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
} xcb_input_feedback_state_t;

/**
 * @brief xcb_input_feedback_state_iterator_t
 **/
typedef struct xcb_input_feedback_state_iterator_t {
    xcb_input_feedback_state_t *data; /**<  */
    int                         rem; /**<  */
    int                         index; /**<  */
} xcb_input_feedback_state_iterator_t;

/**
 * @brief xcb_input_kbd_feedback_state_t
 **/
typedef struct xcb_input_kbd_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint16_t pitch; /**<  */
    uint16_t duration; /**<  */
    uint32_t led_mask; /**<  */
    uint32_t led_values; /**<  */
    uint8_t  global_auto_repeat; /**<  */
    uint8_t  click; /**<  */
    uint8_t  percent; /**<  */
    uint8_t  pad0; /**<  */
    uint8_t  auto_repeats[32]; /**<  */
} xcb_input_kbd_feedback_state_t;

/**
 * @brief xcb_input_kbd_feedback_state_iterator_t
 **/
typedef struct xcb_input_kbd_feedback_state_iterator_t {
    xcb_input_kbd_feedback_state_t *data; /**<  */
    int                             rem; /**<  */
    int                             index; /**<  */
} xcb_input_kbd_feedback_state_iterator_t;

/**
 * @brief xcb_input_ptr_feedback_state_t
 **/
typedef struct xcb_input_ptr_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint8_t  pad0[2]; /**<  */
    uint16_t accel_num; /**<  */
    uint16_t accel_denom; /**<  */
    uint16_t threshold; /**<  */
} xcb_input_ptr_feedback_state_t;

/**
 * @brief xcb_input_ptr_feedback_state_iterator_t
 **/
typedef struct xcb_input_ptr_feedback_state_iterator_t {
    xcb_input_ptr_feedback_state_t *data; /**<  */
    int                             rem; /**<  */
    int                             index; /**<  */
} xcb_input_ptr_feedback_state_iterator_t;

/**
 * @brief xcb_input_integer_feedback_state_t
 **/
typedef struct xcb_input_integer_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint32_t resolution; /**<  */
    int32_t  min_value; /**<  */
    int32_t  max_value; /**<  */
} xcb_input_integer_feedback_state_t;

/**
 * @brief xcb_input_integer_feedback_state_iterator_t
 **/
typedef struct xcb_input_integer_feedback_state_iterator_t {
    xcb_input_integer_feedback_state_t *data; /**<  */
    int                                 rem; /**<  */
    int                                 index; /**<  */
} xcb_input_integer_feedback_state_iterator_t;

/**
 * @brief xcb_input_string_feedback_state_t
 **/
typedef struct xcb_input_string_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint16_t max_symbols; /**<  */
    uint16_t num_keysyms; /**<  */
} xcb_input_string_feedback_state_t;

/**
 * @brief xcb_input_string_feedback_state_iterator_t
 **/
typedef struct xcb_input_string_feedback_state_iterator_t {
    xcb_input_string_feedback_state_t *data; /**<  */
    int                                rem; /**<  */
    int                                index; /**<  */
} xcb_input_string_feedback_state_iterator_t;

/**
 * @brief xcb_input_bell_feedback_state_t
 **/
typedef struct xcb_input_bell_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint8_t  percent; /**<  */
    uint8_t  pad0[3]; /**<  */
    uint16_t pitch; /**<  */
    uint16_t duration; /**<  */
} xcb_input_bell_feedback_state_t;

/**
 * @brief xcb_input_bell_feedback_state_iterator_t
 **/
typedef struct xcb_input_bell_feedback_state_iterator_t {
    xcb_input_bell_feedback_state_t *data; /**<  */
    int                              rem; /**<  */
    int                              index; /**<  */
} xcb_input_bell_feedback_state_iterator_t;

/**
 * @brief xcb_input_led_feedback_state_t
 **/
typedef struct xcb_input_led_feedback_state_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint32_t led_mask; /**<  */
    uint32_t led_values; /**<  */
} xcb_input_led_feedback_state_t;

/**
 * @brief xcb_input_led_feedback_state_iterator_t
 **/
typedef struct xcb_input_led_feedback_state_iterator_t {
    xcb_input_led_feedback_state_t *data; /**<  */
    int                             rem; /**<  */
    int                             index; /**<  */
} xcb_input_led_feedback_state_iterator_t;

/**
 * @brief xcb_input_feedback_ctl_t
 **/
typedef struct xcb_input_feedback_ctl_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
} xcb_input_feedback_ctl_t;

/**
 * @brief xcb_input_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_feedback_ctl_iterator_t {
    xcb_input_feedback_ctl_t *data; /**<  */
    int                       rem; /**<  */
    int                       index; /**<  */
} xcb_input_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_kbd_feedback_ctl_t
 **/
typedef struct xcb_input_kbd_feedback_ctl_t {
    uint8_t              class_id; /**<  */
    uint8_t              id; /**<  */
    uint16_t             len; /**<  */
    xcb_input_key_code_t key; /**<  */
    uint8_t              auto_repeat_mode; /**<  */
    int8_t               key_click_percent; /**<  */
    int8_t               bell_percent; /**<  */
    int16_t              bell_pitch; /**<  */
    int16_t              bell_duration; /**<  */
    uint32_t             led_mask; /**<  */
    uint32_t             led_values; /**<  */
} xcb_input_kbd_feedback_ctl_t;

/**
 * @brief xcb_input_kbd_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_kbd_feedback_ctl_iterator_t {
    xcb_input_kbd_feedback_ctl_t *data; /**<  */
    int                           rem; /**<  */
    int                           index; /**<  */
} xcb_input_kbd_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_ptr_feedback_ctl_t
 **/
typedef struct xcb_input_ptr_feedback_ctl_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint8_t  pad0[2]; /**<  */
    int16_t  num; /**<  */
    int16_t  denom; /**<  */
    int16_t  threshold; /**<  */
} xcb_input_ptr_feedback_ctl_t;

/**
 * @brief xcb_input_ptr_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_ptr_feedback_ctl_iterator_t {
    xcb_input_ptr_feedback_ctl_t *data; /**<  */
    int                           rem; /**<  */
    int                           index; /**<  */
} xcb_input_ptr_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_integer_feedback_ctl_t
 **/
typedef struct xcb_input_integer_feedback_ctl_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    int32_t  int_to_display; /**<  */
} xcb_input_integer_feedback_ctl_t;

/**
 * @brief xcb_input_integer_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_integer_feedback_ctl_iterator_t {
    xcb_input_integer_feedback_ctl_t *data; /**<  */
    int                               rem; /**<  */
    int                               index; /**<  */
} xcb_input_integer_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_string_feedback_ctl_t
 **/
typedef struct xcb_input_string_feedback_ctl_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint8_t  pad0[2]; /**<  */
    uint16_t num_keysyms; /**<  */
} xcb_input_string_feedback_ctl_t;

/**
 * @brief xcb_input_string_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_string_feedback_ctl_iterator_t {
    xcb_input_string_feedback_ctl_t *data; /**<  */
    int                              rem; /**<  */
    int                              index; /**<  */
} xcb_input_string_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_bell_feedback_ctl_t
 **/
typedef struct xcb_input_bell_feedback_ctl_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    int8_t   percent; /**<  */
    uint8_t  pad0[3]; /**<  */
    int16_t  pitch; /**<  */
    int16_t  duration; /**<  */
} xcb_input_bell_feedback_ctl_t;

/**
 * @brief xcb_input_bell_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_bell_feedback_ctl_iterator_t {
    xcb_input_bell_feedback_ctl_t *data; /**<  */
    int                            rem; /**<  */
    int                            index; /**<  */
} xcb_input_bell_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_led_feedback_ctl_t
 **/
typedef struct xcb_input_led_feedback_ctl_t {
    uint8_t  class_id; /**<  */
    uint8_t  id; /**<  */
    uint16_t len; /**<  */
    uint32_t led_mask; /**<  */
    uint32_t led_values; /**<  */
} xcb_input_led_feedback_ctl_t;

/**
 * @brief xcb_input_led_feedback_ctl_iterator_t
 **/
typedef struct xcb_input_led_feedback_ctl_iterator_t {
    xcb_input_led_feedback_ctl_t *data; /**<  */
    int                           rem; /**<  */
    int                           index; /**<  */
} xcb_input_led_feedback_ctl_iterator_t;

/**
 * @brief xcb_input_get_device_key_mapping_cookie_t
 **/
typedef struct xcb_input_get_device_key_mapping_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_key_mapping_cookie_t;

/** Opcode for xcb_input_get_device_key_mapping. */
#define XCB_INPUT_GET_DEVICE_KEY_MAPPING 24

/**
 * @brief xcb_input_get_device_key_mapping_request_t
 **/
typedef struct xcb_input_get_device_key_mapping_request_t {
    uint8_t              major_opcode; /**<  */
    uint8_t              minor_opcode; /**<  */
    uint16_t             length; /**<  */
    uint8_t              device_id; /**<  */
    xcb_input_key_code_t first_keycode; /**<  */
    uint8_t              count; /**<  */
} xcb_input_get_device_key_mapping_request_t;

/**
 * @brief xcb_input_get_device_key_mapping_reply_t
 **/
typedef struct xcb_input_get_device_key_mapping_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  keysyms_per_keycode; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_get_device_key_mapping_reply_t;

/** Opcode for xcb_input_change_device_key_mapping. */
#define XCB_INPUT_CHANGE_DEVICE_KEY_MAPPING 25

/**
 * @brief xcb_input_change_device_key_mapping_request_t
 **/
typedef struct xcb_input_change_device_key_mapping_request_t {
    uint8_t              major_opcode; /**<  */
    uint8_t              minor_opcode; /**<  */
    uint16_t             length; /**<  */
    uint8_t              device_id; /**<  */
    xcb_input_key_code_t first_keycode; /**<  */
    uint8_t              keysyms_per_keycode; /**<  */
    uint8_t              keycode_count; /**<  */
} xcb_input_change_device_key_mapping_request_t;

/**
 * @brief xcb_input_get_device_modifier_mapping_cookie_t
 **/
typedef struct xcb_input_get_device_modifier_mapping_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_modifier_mapping_cookie_t;

/** Opcode for xcb_input_get_device_modifier_mapping. */
#define XCB_INPUT_GET_DEVICE_MODIFIER_MAPPING 26

/**
 * @brief xcb_input_get_device_modifier_mapping_request_t
 **/
typedef struct xcb_input_get_device_modifier_mapping_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_get_device_modifier_mapping_request_t;

/**
 * @brief xcb_input_get_device_modifier_mapping_reply_t
 **/
typedef struct xcb_input_get_device_modifier_mapping_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  keycodes_per_modifier; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_get_device_modifier_mapping_reply_t;

/**
 * @brief xcb_input_set_device_modifier_mapping_cookie_t
 **/
typedef struct xcb_input_set_device_modifier_mapping_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_set_device_modifier_mapping_cookie_t;

/** Opcode for xcb_input_set_device_modifier_mapping. */
#define XCB_INPUT_SET_DEVICE_MODIFIER_MAPPING 27

/**
 * @brief xcb_input_set_device_modifier_mapping_request_t
 **/
typedef struct xcb_input_set_device_modifier_mapping_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  keycodes_per_modifier; /**<  */
    uint8_t  pad0; /**<  */
} xcb_input_set_device_modifier_mapping_request_t;

/**
 * @brief xcb_input_set_device_modifier_mapping_reply_t
 **/
typedef struct xcb_input_set_device_modifier_mapping_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_set_device_modifier_mapping_reply_t;

/**
 * @brief xcb_input_get_device_button_mapping_cookie_t
 **/
typedef struct xcb_input_get_device_button_mapping_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_button_mapping_cookie_t;

/** Opcode for xcb_input_get_device_button_mapping. */
#define XCB_INPUT_GET_DEVICE_BUTTON_MAPPING 28

/**
 * @brief xcb_input_get_device_button_mapping_request_t
 **/
typedef struct xcb_input_get_device_button_mapping_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_get_device_button_mapping_request_t;

/**
 * @brief xcb_input_get_device_button_mapping_reply_t
 **/
typedef struct xcb_input_get_device_button_mapping_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  map_size; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_get_device_button_mapping_reply_t;

/**
 * @brief xcb_input_set_device_button_mapping_cookie_t
 **/
typedef struct xcb_input_set_device_button_mapping_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_set_device_button_mapping_cookie_t;

/** Opcode for xcb_input_set_device_button_mapping. */
#define XCB_INPUT_SET_DEVICE_BUTTON_MAPPING 29

/**
 * @brief xcb_input_set_device_button_mapping_request_t
 **/
typedef struct xcb_input_set_device_button_mapping_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  map_size; /**<  */
    uint8_t  pad0[2]; /**<  */
} xcb_input_set_device_button_mapping_request_t;

/**
 * @brief xcb_input_set_device_button_mapping_reply_t
 **/
typedef struct xcb_input_set_device_button_mapping_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_set_device_button_mapping_reply_t;

/**
 * @brief xcb_input_query_device_state_cookie_t
 **/
typedef struct xcb_input_query_device_state_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_query_device_state_cookie_t;

/** Opcode for xcb_input_query_device_state. */
#define XCB_INPUT_QUERY_DEVICE_STATE 30

/**
 * @brief xcb_input_query_device_state_request_t
 **/
typedef struct xcb_input_query_device_state_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_query_device_state_request_t;

/**
 * @brief xcb_input_query_device_state_reply_t
 **/
typedef struct xcb_input_query_device_state_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  num_classes; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_query_device_state_reply_t;

/**
 * @brief xcb_input_input_state_t
 **/
typedef struct xcb_input_input_state_t {
    uint8_t class_id; /**<  */
    uint8_t len; /**<  */
    uint8_t num_items; /**<  */
} xcb_input_input_state_t;

/**
 * @brief xcb_input_input_state_iterator_t
 **/
typedef struct xcb_input_input_state_iterator_t {
    xcb_input_input_state_t *data; /**<  */
    int                      rem; /**<  */
    int                      index; /**<  */
} xcb_input_input_state_iterator_t;

/**
 * @brief xcb_input_key_state_t
 **/
typedef struct xcb_input_key_state_t {
    uint8_t class_id; /**<  */
    uint8_t len; /**<  */
    uint8_t num_keys; /**<  */
    uint8_t pad0; /**<  */
    uint8_t keys[32]; /**<  */
} xcb_input_key_state_t;

/**
 * @brief xcb_input_key_state_iterator_t
 **/
typedef struct xcb_input_key_state_iterator_t {
    xcb_input_key_state_t *data; /**<  */
    int                    rem; /**<  */
    int                    index; /**<  */
} xcb_input_key_state_iterator_t;

/**
 * @brief xcb_input_button_state_t
 **/
typedef struct xcb_input_button_state_t {
    uint8_t class_id; /**<  */
    uint8_t len; /**<  */
    uint8_t num_buttons; /**<  */
    uint8_t pad0; /**<  */
    uint8_t buttons[32]; /**<  */
} xcb_input_button_state_t;

/**
 * @brief xcb_input_button_state_iterator_t
 **/
typedef struct xcb_input_button_state_iterator_t {
    xcb_input_button_state_t *data; /**<  */
    int                       rem; /**<  */
    int                       index; /**<  */
} xcb_input_button_state_iterator_t;

/**
 * @brief xcb_input_valuator_state_t
 **/
typedef struct xcb_input_valuator_state_t {
    uint8_t class_id; /**<  */
    uint8_t len; /**<  */
    uint8_t num_valuators; /**<  */
    uint8_t mode; /**<  */
} xcb_input_valuator_state_t;

/**
 * @brief xcb_input_valuator_state_iterator_t
 **/
typedef struct xcb_input_valuator_state_iterator_t {
    xcb_input_valuator_state_t *data; /**<  */
    int                         rem; /**<  */
    int                         index; /**<  */
} xcb_input_valuator_state_iterator_t;

/** Opcode for xcb_input_send_extension_event. */
#define XCB_INPUT_SEND_EXTENSION_EVENT 31

/**
 * @brief xcb_input_send_extension_event_request_t
 **/
typedef struct xcb_input_send_extension_event_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t destination; /**<  */
    uint8_t      device_id; /**<  */
    uint8_t      propagate; /**<  */
    uint16_t     num_classes; /**<  */
    uint8_t      num_events; /**<  */
    uint8_t      pad0[3]; /**<  */
} xcb_input_send_extension_event_request_t;

/** Opcode for xcb_input_device_bell. */
#define XCB_INPUT_DEVICE_BELL 32

/**
 * @brief xcb_input_device_bell_request_t
 **/
typedef struct xcb_input_device_bell_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  feedback_id; /**<  */
    uint8_t  feedback_class; /**<  */
    int8_t   percent; /**<  */
} xcb_input_device_bell_request_t;

/**
 * @brief xcb_input_set_device_valuators_cookie_t
 **/
typedef struct xcb_input_set_device_valuators_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_set_device_valuators_cookie_t;

/** Opcode for xcb_input_set_device_valuators. */
#define XCB_INPUT_SET_DEVICE_VALUATORS 33

/**
 * @brief xcb_input_set_device_valuators_request_t
 **/
typedef struct xcb_input_set_device_valuators_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  first_valuator; /**<  */
    uint8_t  num_valuators; /**<  */
    uint8_t  pad0; /**<  */
} xcb_input_set_device_valuators_request_t;

/**
 * @brief xcb_input_set_device_valuators_reply_t
 **/
typedef struct xcb_input_set_device_valuators_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_set_device_valuators_reply_t;

/**
 * @brief xcb_input_get_device_control_cookie_t
 **/
typedef struct xcb_input_get_device_control_cookie_t {
    unsigned int sequence; /**<  */
} xcb_input_get_device_control_cookie_t;

/** Opcode for xcb_input_get_device_control. */
#define XCB_INPUT_GET_DEVICE_CONTROL 34

/**
 * @brief xcb_input_get_device_control_request_t
 **/
typedef struct xcb_input_get_device_control_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint16_t control_id; /**<  */
    uint8_t  device_id; /**<  */
    uint8_t  pad0; /**<  */
} xcb_input_get_device_control_request_t;

/**
 * @brief xcb_input_get_device_control_reply_t
 **/
typedef struct xcb_input_get_device_control_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad1[23]; /**<  */
} xcb_input_get_device_control_reply_t;

/**
 * @brief xcb_input_device_state_t
 **/
typedef struct xcb_input_device_state_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
} xcb_input_device_state_t;

/**
 * @brief xcb_input_device_state_iterator_t
 **/
typedef struct xcb_input_device_state_iterator_t {
    xcb_input_device_state_t *data; /**<  */
    int                       rem; /**<  */
    int                       index; /**<  */
} xcb_input_device_state_iterator_t;

/**
 * @brief xcb_input_device_resolution_state_t
 **/
typedef struct xcb_input_device_resolution_state_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint32_t num_valuators; /**<  */
} xcb_input_device_resolution_state_t;

/**
 * @brief xcb_input_device_resolution_state_iterator_t
 **/
typedef struct xcb_input_device_resolution_state_iterator_t {
    xcb_input_device_resolution_state_t *data; /**<  */
    int                                  rem; /**<  */
    int                                  index; /**<  */
} xcb_input_device_resolution_state_iterator_t;

/**
 * @brief xcb_input_device_abs_calib_state_t
 **/
typedef struct xcb_input_device_abs_calib_state_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    int32_t  min_x; /**<  */
    int32_t  max_x; /**<  */
    int32_t  min_y; /**<  */
    int32_t  max_y; /**<  */
    uint32_t flip_x; /**<  */
    uint32_t flip_y; /**<  */
    uint32_t rotation; /**<  */
    uint32_t button_threshold; /**<  */
} xcb_input_device_abs_calib_state_t;

/**
 * @brief xcb_input_device_abs_calib_state_iterator_t
 **/
typedef struct xcb_input_device_abs_calib_state_iterator_t {
    xcb_input_device_abs_calib_state_t *data; /**<  */
    int                                 rem; /**<  */
    int                                 index; /**<  */
} xcb_input_device_abs_calib_state_iterator_t;

/**
 * @brief xcb_input_device_abs_area_state_t
 **/
typedef struct xcb_input_device_abs_area_state_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint32_t offset_x; /**<  */
    uint32_t offset_y; /**<  */
    uint32_t width; /**<  */
    uint32_t height; /**<  */
    uint32_t screen; /**<  */
    uint32_t following; /**<  */
} xcb_input_device_abs_area_state_t;

/**
 * @brief xcb_input_device_abs_area_state_iterator_t
 **/
typedef struct xcb_input_device_abs_area_state_iterator_t {
    xcb_input_device_abs_area_state_t *data; /**<  */
    int                                rem; /**<  */
    int                                index; /**<  */
} xcb_input_device_abs_area_state_iterator_t;

/**
 * @brief xcb_input_device_core_state_t
 **/
typedef struct xcb_input_device_core_state_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint8_t  status; /**<  */
    uint8_t  iscore; /**<  */
    uint8_t  pad0[2]; /**<  */
} xcb_input_device_core_state_t;

/**
 * @brief xcb_input_device_core_state_iterator_t
 **/
typedef struct xcb_input_device_core_state_iterator_t {
    xcb_input_device_core_state_t *data; /**<  */
    int                            rem; /**<  */
    int                            index; /**<  */
} xcb_input_device_core_state_iterator_t;

/**
 * @brief xcb_input_device_enable_state_t
 **/
typedef struct xcb_input_device_enable_state_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint8_t  enable; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_device_enable_state_t;

/**
 * @brief xcb_input_device_enable_state_iterator_t
 **/
typedef struct xcb_input_device_enable_state_iterator_t {
    xcb_input_device_enable_state_t *data; /**<  */
    int                              rem; /**<  */
    int                              index; /**<  */
} xcb_input_device_enable_state_iterator_t;

/**
 * @brief xcb_input_device_ctl_t
 **/
typedef struct xcb_input_device_ctl_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
} xcb_input_device_ctl_t;

/**
 * @brief xcb_input_device_ctl_iterator_t
 **/
typedef struct xcb_input_device_ctl_iterator_t {
    xcb_input_device_ctl_t *data; /**<  */
    int                     rem; /**<  */
    int                     index; /**<  */
} xcb_input_device_ctl_iterator_t;

/**
 * @brief xcb_input_device_resolution_ctl_t
 **/
typedef struct xcb_input_device_resolution_ctl_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint8_t  first_valuator; /**<  */
    uint8_t  num_valuators; /**<  */
} xcb_input_device_resolution_ctl_t;

/**
 * @brief xcb_input_device_resolution_ctl_iterator_t
 **/
typedef struct xcb_input_device_resolution_ctl_iterator_t {
    xcb_input_device_resolution_ctl_t *data; /**<  */
    int                                rem; /**<  */
    int                                index; /**<  */
} xcb_input_device_resolution_ctl_iterator_t;

/**
 * @brief xcb_input_device_abs_calib_ctl_t
 **/
typedef struct xcb_input_device_abs_calib_ctl_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    int32_t  min_x; /**<  */
    int32_t  max_x; /**<  */
    int32_t  min_y; /**<  */
    int32_t  max_y; /**<  */
    uint32_t flip_x; /**<  */
    uint32_t flip_y; /**<  */
    uint32_t rotation; /**<  */
    uint32_t button_threshold; /**<  */
} xcb_input_device_abs_calib_ctl_t;

/**
 * @brief xcb_input_device_abs_calib_ctl_iterator_t
 **/
typedef struct xcb_input_device_abs_calib_ctl_iterator_t {
    xcb_input_device_abs_calib_ctl_t *data; /**<  */
    int                               rem; /**<  */
    int                               index; /**<  */
} xcb_input_device_abs_calib_ctl_iterator_t;

/**
 * @brief xcb_input_device_abs_area_ctrl_t
 **/
typedef struct xcb_input_device_abs_area_ctrl_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint32_t offset_x; /**<  */
    uint32_t offset_y; /**<  */
    int32_t  width; /**<  */
    int32_t  height; /**<  */
    int32_t  screen; /**<  */
    uint32_t following; /**<  */
} xcb_input_device_abs_area_ctrl_t;

/**
 * @brief xcb_input_device_abs_area_ctrl_iterator_t
 **/
typedef struct xcb_input_device_abs_area_ctrl_iterator_t {
    xcb_input_device_abs_area_ctrl_t *data; /**<  */
    int                               rem; /**<  */
    int                               index; /**<  */
} xcb_input_device_abs_area_ctrl_iterator_t;

/**
 * @brief xcb_input_device_core_ctrl_t
 **/
typedef struct xcb_input_device_core_ctrl_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint8_t  status; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_device_core_ctrl_t;

/**
 * @brief xcb_input_device_core_ctrl_iterator_t
 **/
typedef struct xcb_input_device_core_ctrl_iterator_t {
    xcb_input_device_core_ctrl_t *data; /**<  */
    int                           rem; /**<  */
    int                           index; /**<  */
} xcb_input_device_core_ctrl_iterator_t;

/**
 * @brief xcb_input_device_enable_ctrl_t
 **/
typedef struct xcb_input_device_enable_ctrl_t {
    uint16_t control_id; /**<  */
    uint16_t len; /**<  */
    uint8_t  enable; /**<  */
    uint8_t  pad0[3]; /**<  */
} xcb_input_device_enable_ctrl_t;

/**
 * @brief xcb_input_device_enable_ctrl_iterator_t
 **/
typedef struct xcb_input_device_enable_ctrl_iterator_t {
    xcb_input_device_enable_ctrl_t *data; /**<  */
    int                             rem; /**<  */
    int                             index; /**<  */
} xcb_input_device_enable_ctrl_iterator_t;

/** Opcode for xcb_input_device_valuator. */
#define XCB_INPUT_DEVICE_VALUATOR 0

/**
 * @brief xcb_input_device_valuator_event_t
 **/
typedef struct xcb_input_device_valuator_event_t {
    uint8_t  response_type; /**<  */
    uint8_t  device_id; /**<  */
    uint16_t sequence; /**<  */
    uint16_t device_state; /**<  */
    uint8_t  num_valuators; /**<  */
    uint8_t  first_valuator; /**<  */
    int32_t  valuators[6]; /**<  */
} xcb_input_device_valuator_event_t;

/** Opcode for xcb_input_device_key_press. */
#define XCB_INPUT_DEVICE_KEY_PRESS 1

/**
 * @brief xcb_input_device_key_press_event_t
 **/
typedef struct xcb_input_device_key_press_event_t {
    uint8_t         response_type; /**<  */
    uint8_t         detail; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t time; /**<  */
    xcb_window_t    root; /**<  */
    xcb_window_t    event; /**<  */
    xcb_window_t    child; /**<  */
    int16_t         root_x; /**<  */
    int16_t         root_y; /**<  */
    int16_t         event_x; /**<  */
    int16_t         event_y; /**<  */
    uint16_t        state; /**<  */
    uint8_t         same_screen; /**<  */
    uint8_t         device_id; /**<  */
} xcb_input_device_key_press_event_t;

/** Opcode for xcb_input_device_key_release. */
#define XCB_INPUT_DEVICE_KEY_RELEASE 2

typedef xcb_input_device_key_press_event_t xcb_input_device_key_release_event_t;

/** Opcode for xcb_input_device_button_press. */
#define XCB_INPUT_DEVICE_BUTTON_PRESS 3

typedef xcb_input_device_key_press_event_t xcb_input_device_button_press_event_t;

/** Opcode for xcb_input_device_button_release. */
#define XCB_INPUT_DEVICE_BUTTON_RELEASE 4

typedef xcb_input_device_key_press_event_t xcb_input_device_button_release_event_t;

/** Opcode for xcb_input_device_motion_notify. */
#define XCB_INPUT_DEVICE_MOTION_NOTIFY 5

typedef xcb_input_device_key_press_event_t xcb_input_device_motion_notify_event_t;

/** Opcode for xcb_input_proximity_in. */
#define XCB_INPUT_PROXIMITY_IN 8

typedef xcb_input_device_key_press_event_t xcb_input_proximity_in_event_t;

/** Opcode for xcb_input_proximity_out. */
#define XCB_INPUT_PROXIMITY_OUT 9

typedef xcb_input_device_key_press_event_t xcb_input_proximity_out_event_t;

/** Opcode for xcb_input_focus_in. */
#define XCB_INPUT_FOCUS_IN 6

/**
 * @brief xcb_input_focus_in_event_t
 **/
typedef struct xcb_input_focus_in_event_t {
    uint8_t         response_type; /**<  */
    uint8_t         detail; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t time; /**<  */
    xcb_window_t    window; /**<  */
    uint8_t         mode; /**<  */
    uint8_t         device_id; /**<  */
    uint8_t         pad0[18]; /**<  */
} xcb_input_focus_in_event_t;

/** Opcode for xcb_input_focus_out. */
#define XCB_INPUT_FOCUS_OUT 7

typedef xcb_input_focus_in_event_t xcb_input_focus_out_event_t;

/** Opcode for xcb_input_device_state_notify. */
#define XCB_INPUT_DEVICE_STATE_NOTIFY 10

/**
 * @brief xcb_input_device_state_notify_event_t
 **/
typedef struct xcb_input_device_state_notify_event_t {
    uint8_t         response_type; /**<  */
    uint8_t         device_id; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         num_keys; /**<  */
    uint8_t         num_buttons; /**<  */
    uint8_t         num_valuators; /**<  */
    uint8_t         classes_reported; /**<  */
    uint8_t         buttons[4]; /**<  */
    uint8_t         keys[4]; /**<  */
    uint32_t        valuators[3]; /**<  */
} xcb_input_device_state_notify_event_t;

/** Opcode for xcb_input_device_mapping_notify. */
#define XCB_INPUT_DEVICE_MAPPING_NOTIFY 11

/**
 * @brief xcb_input_device_mapping_notify_event_t
 **/
typedef struct xcb_input_device_mapping_notify_event_t {
    uint8_t              response_type; /**<  */
    uint8_t              device_id; /**<  */
    uint16_t             sequence; /**<  */
    uint8_t              request; /**<  */
    xcb_input_key_code_t first_keycode; /**<  */
    uint8_t              count; /**<  */
    uint8_t              pad0; /**<  */
    xcb_timestamp_t      time; /**<  */
    uint8_t              pad1[20]; /**<  */
} xcb_input_device_mapping_notify_event_t;

/** Opcode for xcb_input_change_device_notify. */
#define XCB_INPUT_CHANGE_DEVICE_NOTIFY 12

/**
 * @brief xcb_input_change_device_notify_event_t
 **/
typedef struct xcb_input_change_device_notify_event_t {
    uint8_t         response_type; /**<  */
    uint8_t         device_id; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         request; /**<  */
    uint8_t         pad0[23]; /**<  */
} xcb_input_change_device_notify_event_t;

/** Opcode for xcb_input_device_key_state_notify. */
#define XCB_INPUT_DEVICE_KEY_STATE_NOTIFY 13

/**
 * @brief xcb_input_device_key_state_notify_event_t
 **/
typedef struct xcb_input_device_key_state_notify_event_t {
    uint8_t  response_type; /**<  */
    uint8_t  device_id; /**<  */
    uint16_t sequence; /**<  */
    uint8_t  keys[28]; /**<  */
} xcb_input_device_key_state_notify_event_t;

/** Opcode for xcb_input_device_button_state_notify. */
#define XCB_INPUT_DEVICE_BUTTON_STATE_NOTIFY 14

/**
 * @brief xcb_input_device_button_state_notify_event_t
 **/
typedef struct xcb_input_device_button_state_notify_event_t {
    uint8_t  response_type; /**<  */
    uint8_t  device_id; /**<  */
    uint16_t sequence; /**<  */
    uint8_t  buttons[28]; /**<  */
} xcb_input_device_button_state_notify_event_t;

/** Opcode for xcb_input_device_presence_notify. */
#define XCB_INPUT_DEVICE_PRESENCE_NOTIFY 15

/**
 * @brief xcb_input_device_presence_notify_event_t
 **/
typedef struct xcb_input_device_presence_notify_event_t {
    uint8_t         response_type; /**<  */
    uint8_t         pad0; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t time; /**<  */
    uint8_t         devchange; /**<  */
    uint8_t         device_id; /**<  */
    uint16_t        control; /**<  */
    uint8_t         pad1[20]; /**<  */
} xcb_input_device_presence_notify_event_t;

/** Opcode for xcb_input_device. */
#define XCB_INPUT_DEVICE 0

/**
 * @brief xcb_input_device_error_t
 **/
typedef struct xcb_input_device_error_t {
    uint8_t  response_type; /**<  */
    uint8_t  error_code; /**<  */
    uint16_t sequence; /**<  */
} xcb_input_device_error_t;

/** Opcode for xcb_input_event. */
#define XCB_INPUT_EVENT 1

/**
 * @brief xcb_input_event_error_t
 **/
typedef struct xcb_input_event_error_t {
    uint8_t  response_type; /**<  */
    uint8_t  error_code; /**<  */
    uint16_t sequence; /**<  */
} xcb_input_event_error_t;

/** Opcode for xcb_input_mode. */
#define XCB_INPUT_MODE 2

/**
 * @brief xcb_input_mode_error_t
 **/
typedef struct xcb_input_mode_error_t {
    uint8_t  response_type; /**<  */
    uint8_t  error_code; /**<  */
    uint16_t sequence; /**<  */
} xcb_input_mode_error_t;

/** Opcode for xcb_input_device_busy. */
#define XCB_INPUT_DEVICE_BUSY 3

/**
 * @brief xcb_input_device_busy_error_t
 **/
typedef struct xcb_input_device_busy_error_t {
    uint8_t  response_type; /**<  */
    uint8_t  error_code; /**<  */
    uint16_t sequence; /**<  */
} xcb_input_device_busy_error_t;

/** Opcode for xcb_input_class. */
#define XCB_INPUT_CLASS 4

/**
 * @brief xcb_input_class_error_t
 **/
typedef struct xcb_input_class_error_t {
    uint8_t  response_type; /**<  */
    uint8_t  error_code; /**<  */
    uint16_t sequence; /**<  */
} xcb_input_class_error_t;

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_key_code_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_key_code_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_key_code_next
 ** 
 ** @param xcb_input_key_code_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_key_code_next (xcb_input_key_code_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_key_code_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_key_code_end
 ** 
 ** @param xcb_input_key_code_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_key_code_end (xcb_input_key_code_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_event_class_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_event_class_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_event_class_next
 ** 
 ** @param xcb_input_event_class_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_event_class_next (xcb_input_event_class_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_event_class_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_event_class_end
 ** 
 ** @param xcb_input_event_class_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_event_class_end (xcb_input_event_class_iterator_t i  /**< */);

int
xcb_input_get_extension_version_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_extension_version_cookie_t xcb_input_get_extension_version
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_input_get_extension_version_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_extension_version_cookie_t
xcb_input_get_extension_version (xcb_connection_t *c  /**< */,
                                 uint16_t          name_len  /**< */,
                                 const char       *name  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_extension_version_cookie_t xcb_input_get_extension_version_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_input_get_extension_version_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_extension_version_cookie_t
xcb_input_get_extension_version_unchecked (xcb_connection_t *c  /**< */,
                                           uint16_t          name_len  /**< */,
                                           const char       *name  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_extension_version_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_extension_version_reply_t * xcb_input_get_extension_version_reply
 ** 
 ** @param xcb_connection_t                          *c
 ** @param xcb_input_get_extension_version_cookie_t   cookie
 ** @param xcb_generic_error_t                      **e
 ** @returns xcb_input_get_extension_version_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_extension_version_reply_t *
xcb_input_get_extension_version_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_get_extension_version_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_info_next
 ** 
 ** @param xcb_input_device_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_info_next (xcb_input_device_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_info_end
 ** 
 ** @param xcb_input_device_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_info_end (xcb_input_device_info_iterator_t i  /**< */);

int
xcb_input_list_input_devices_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_list_input_devices_cookie_t xcb_input_list_input_devices
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_input_list_input_devices_cookie_t
 **
 *****************************************************************************/
 
xcb_input_list_input_devices_cookie_t
xcb_input_list_input_devices (xcb_connection_t *c  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_list_input_devices_cookie_t xcb_input_list_input_devices_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_input_list_input_devices_cookie_t
 **
 *****************************************************************************/
 
xcb_input_list_input_devices_cookie_t
xcb_input_list_input_devices_unchecked (xcb_connection_t *c  /**< */);


/*****************************************************************************
 **
 ** xcb_input_device_info_t * xcb_input_list_input_devices_devices
 ** 
 ** @param const xcb_input_list_input_devices_reply_t *R
 ** @returns xcb_input_device_info_t *
 **
 *****************************************************************************/
 
xcb_input_device_info_t *
xcb_input_list_input_devices_devices (const xcb_input_list_input_devices_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_list_input_devices_devices_length
 ** 
 ** @param const xcb_input_list_input_devices_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_list_input_devices_devices_length (const xcb_input_list_input_devices_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_input_device_info_iterator_t xcb_input_list_input_devices_devices_iterator
 ** 
 ** @param const xcb_input_list_input_devices_reply_t *R
 ** @returns xcb_input_device_info_iterator_t
 **
 *****************************************************************************/
 
xcb_input_device_info_iterator_t
xcb_input_list_input_devices_devices_iterator (const xcb_input_list_input_devices_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_list_input_devices_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_list_input_devices_reply_t * xcb_input_list_input_devices_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_input_list_input_devices_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_input_list_input_devices_reply_t *
 **
 *****************************************************************************/
 
xcb_input_list_input_devices_reply_t *
xcb_input_list_input_devices_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_list_input_devices_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_input_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_input_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_input_info_next
 ** 
 ** @param xcb_input_input_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_input_info_next (xcb_input_input_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_input_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_input_info_end
 ** 
 ** @param xcb_input_input_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_input_info_end (xcb_input_input_info_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_key_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_key_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_key_info_next
 ** 
 ** @param xcb_input_key_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_key_info_next (xcb_input_key_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_key_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_key_info_end
 ** 
 ** @param xcb_input_key_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_key_info_end (xcb_input_key_info_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_button_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_button_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_button_info_next
 ** 
 ** @param xcb_input_button_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_button_info_next (xcb_input_button_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_button_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_button_info_end
 ** 
 ** @param xcb_input_button_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_button_info_end (xcb_input_button_info_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_axis_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_axis_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_axis_info_next
 ** 
 ** @param xcb_input_axis_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_axis_info_next (xcb_input_axis_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_axis_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_axis_info_end
 ** 
 ** @param xcb_input_axis_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_axis_info_end (xcb_input_axis_info_iterator_t i  /**< */);

int
xcb_input_valuator_info_sizeof (const void  *_buffer  /**< */);


/*****************************************************************************
 **
 ** xcb_input_axis_info_t * xcb_input_valuator_info_axes
 ** 
 ** @param const xcb_input_valuator_info_t *R
 ** @returns xcb_input_axis_info_t *
 **
 *****************************************************************************/
 
xcb_input_axis_info_t *
xcb_input_valuator_info_axes (const xcb_input_valuator_info_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_valuator_info_axes_length
 ** 
 ** @param const xcb_input_valuator_info_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_valuator_info_axes_length (const xcb_input_valuator_info_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_input_axis_info_iterator_t xcb_input_valuator_info_axes_iterator
 ** 
 ** @param const xcb_input_valuator_info_t *R
 ** @returns xcb_input_axis_info_iterator_t
 **
 *****************************************************************************/
 
xcb_input_axis_info_iterator_t
xcb_input_valuator_info_axes_iterator (const xcb_input_valuator_info_t *R  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_valuator_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_valuator_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_valuator_info_next
 ** 
 ** @param xcb_input_valuator_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_valuator_info_next (xcb_input_valuator_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_valuator_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_valuator_info_end
 ** 
 ** @param xcb_input_valuator_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_valuator_info_end (xcb_input_valuator_info_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_input_class_info_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_input_class_info_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_input_class_info_next
 ** 
 ** @param xcb_input_input_class_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_input_class_info_next (xcb_input_input_class_info_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_input_class_info_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_input_class_info_end
 ** 
 ** @param xcb_input_input_class_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_input_class_info_end (xcb_input_input_class_info_iterator_t i  /**< */);

int
xcb_input_open_device_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_open_device_cookie_t xcb_input_open_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_open_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_open_device_cookie_t
xcb_input_open_device (xcb_connection_t *c  /**< */,
                       uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_open_device_cookie_t xcb_input_open_device_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_open_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_open_device_cookie_t
xcb_input_open_device_unchecked (xcb_connection_t *c  /**< */,
                                 uint8_t           device_id  /**< */);


/*****************************************************************************
 **
 ** xcb_input_input_class_info_t * xcb_input_open_device_class_info
 ** 
 ** @param const xcb_input_open_device_reply_t *R
 ** @returns xcb_input_input_class_info_t *
 **
 *****************************************************************************/
 
xcb_input_input_class_info_t *
xcb_input_open_device_class_info (const xcb_input_open_device_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_open_device_class_info_length
 ** 
 ** @param const xcb_input_open_device_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_open_device_class_info_length (const xcb_input_open_device_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_input_input_class_info_iterator_t xcb_input_open_device_class_info_iterator
 ** 
 ** @param const xcb_input_open_device_reply_t *R
 ** @returns xcb_input_input_class_info_iterator_t
 **
 *****************************************************************************/
 
xcb_input_input_class_info_iterator_t
xcb_input_open_device_class_info_iterator (const xcb_input_open_device_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_open_device_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_open_device_reply_t * xcb_input_open_device_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_input_open_device_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_input_open_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_open_device_reply_t *
xcb_input_open_device_reply (xcb_connection_t                *c  /**< */,
                             xcb_input_open_device_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_close_device_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_close_device_checked (xcb_connection_t *c  /**< */,
                                uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_close_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_close_device (xcb_connection_t *c  /**< */,
                        uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_mode_cookie_t xcb_input_set_device_mode
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           mode
 ** @returns xcb_input_set_device_mode_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_mode_cookie_t
xcb_input_set_device_mode (xcb_connection_t *c  /**< */,
                           uint8_t           device_id  /**< */,
                           uint8_t           mode  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_mode_cookie_t xcb_input_set_device_mode_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           mode
 ** @returns xcb_input_set_device_mode_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_mode_cookie_t
xcb_input_set_device_mode_unchecked (xcb_connection_t *c  /**< */,
                                     uint8_t           device_id  /**< */,
                                     uint8_t           mode  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_set_device_mode_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_mode_reply_t * xcb_input_set_device_mode_reply
 ** 
 ** @param xcb_connection_t                    *c
 ** @param xcb_input_set_device_mode_cookie_t   cookie
 ** @param xcb_generic_error_t                **e
 ** @returns xcb_input_set_device_mode_reply_t *
 **
 *****************************************************************************/
 
xcb_input_set_device_mode_reply_t *
xcb_input_set_device_mode_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_input_set_device_mode_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */);

int
xcb_input_select_extension_event_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_select_extension_event_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_select_extension_event_checked (xcb_connection_t              *c  /**< */,
                                          xcb_window_t                   window  /**< */,
                                          uint16_t                       num_classes  /**< */,
                                          const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_select_extension_event
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_select_extension_event (xcb_connection_t              *c  /**< */,
                                  xcb_window_t                   window  /**< */,
                                  uint16_t                       num_classes  /**< */,
                                  const xcb_input_event_class_t *classes  /**< */);

int
xcb_input_get_selected_extension_events_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_selected_extension_events_cookie_t xcb_input_get_selected_extension_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_selected_extension_events_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_selected_extension_events_cookie_t
xcb_input_get_selected_extension_events (xcb_connection_t *c  /**< */,
                                         xcb_window_t      window  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_selected_extension_events_cookie_t xcb_input_get_selected_extension_events_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_selected_extension_events_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_selected_extension_events_cookie_t
xcb_input_get_selected_extension_events_unchecked (xcb_connection_t *c  /**< */,
                                                   xcb_window_t      window  /**< */);


/*****************************************************************************
 **
 ** xcb_input_event_class_t * xcb_input_get_selected_extension_events_this_classes
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_input_event_class_t *
 **
 *****************************************************************************/
 
xcb_input_event_class_t *
xcb_input_get_selected_extension_events_this_classes (const xcb_input_get_selected_extension_events_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_get_selected_extension_events_this_classes_length
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_selected_extension_events_this_classes_length (const xcb_input_get_selected_extension_events_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_selected_extension_events_this_classes_end
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_selected_extension_events_this_classes_end (const xcb_input_get_selected_extension_events_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_input_event_class_t * xcb_input_get_selected_extension_events_all_classes
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_input_event_class_t *
 **
 *****************************************************************************/
 
xcb_input_event_class_t *
xcb_input_get_selected_extension_events_all_classes (const xcb_input_get_selected_extension_events_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_get_selected_extension_events_all_classes_length
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_selected_extension_events_all_classes_length (const xcb_input_get_selected_extension_events_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_selected_extension_events_all_classes_end
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_selected_extension_events_all_classes_end (const xcb_input_get_selected_extension_events_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_selected_extension_events_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_selected_extension_events_reply_t * xcb_input_get_selected_extension_events_reply
 ** 
 ** @param xcb_connection_t                                  *c
 ** @param xcb_input_get_selected_extension_events_cookie_t   cookie
 ** @param xcb_generic_error_t                              **e
 ** @returns xcb_input_get_selected_extension_events_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_selected_extension_events_reply_t *
xcb_input_get_selected_extension_events_reply (xcb_connection_t                                  *c  /**< */,
                                               xcb_input_get_selected_extension_events_cookie_t   cookie  /**< */,
                                               xcb_generic_error_t                              **e  /**< */);

int
xcb_input_change_device_dont_propagate_list_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_dont_propagate_list_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        mode
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_change_device_dont_propagate_list_checked (xcb_connection_t              *c  /**< */,
                                                     xcb_window_t                   window  /**< */,
                                                     uint16_t                       num_classes  /**< */,
                                                     uint8_t                        mode  /**< */,
                                                     const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_dont_propagate_list
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        mode
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_change_device_dont_propagate_list (xcb_connection_t              *c  /**< */,
                                             xcb_window_t                   window  /**< */,
                                             uint16_t                       num_classes  /**< */,
                                             uint8_t                        mode  /**< */,
                                             const xcb_input_event_class_t *classes  /**< */);

int
xcb_input_get_device_dont_propagate_list_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_dont_propagate_list_cookie_t xcb_input_get_device_dont_propagate_list
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_device_dont_propagate_list_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_dont_propagate_list_cookie_t
xcb_input_get_device_dont_propagate_list (xcb_connection_t *c  /**< */,
                                          xcb_window_t      window  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_dont_propagate_list_cookie_t xcb_input_get_device_dont_propagate_list_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_device_dont_propagate_list_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_dont_propagate_list_cookie_t
xcb_input_get_device_dont_propagate_list_unchecked (xcb_connection_t *c  /**< */,
                                                    xcb_window_t      window  /**< */);


/*****************************************************************************
 **
 ** xcb_input_event_class_t * xcb_input_get_device_dont_propagate_list_classes
 ** 
 ** @param const xcb_input_get_device_dont_propagate_list_reply_t *R
 ** @returns xcb_input_event_class_t *
 **
 *****************************************************************************/
 
xcb_input_event_class_t *
xcb_input_get_device_dont_propagate_list_classes (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_get_device_dont_propagate_list_classes_length
 ** 
 ** @param const xcb_input_get_device_dont_propagate_list_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_dont_propagate_list_classes_length (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_dont_propagate_list_classes_end
 ** 
 ** @param const xcb_input_get_device_dont_propagate_list_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_dont_propagate_list_classes_end (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_dont_propagate_list_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_dont_propagate_list_reply_t * xcb_input_get_device_dont_propagate_list_reply
 ** 
 ** @param xcb_connection_t                                   *c
 ** @param xcb_input_get_device_dont_propagate_list_cookie_t   cookie
 ** @param xcb_generic_error_t                               **e
 ** @returns xcb_input_get_device_dont_propagate_list_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_dont_propagate_list_reply_t *
xcb_input_get_device_dont_propagate_list_reply (xcb_connection_t                                   *c  /**< */,
                                                xcb_input_get_device_dont_propagate_list_cookie_t   cookie  /**< */,
                                                xcb_generic_error_t                               **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_motion_events_cookie_t xcb_input_get_device_motion_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   start
 ** @param xcb_timestamp_t   stop
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_motion_events_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_motion_events_cookie_t
xcb_input_get_device_motion_events (xcb_connection_t *c  /**< */,
                                    xcb_timestamp_t   start  /**< */,
                                    xcb_timestamp_t   stop  /**< */,
                                    uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_motion_events_cookie_t xcb_input_get_device_motion_events_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   start
 ** @param xcb_timestamp_t   stop
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_motion_events_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_motion_events_cookie_t
xcb_input_get_device_motion_events_unchecked (xcb_connection_t *c  /**< */,
                                              xcb_timestamp_t   start  /**< */,
                                              xcb_timestamp_t   stop  /**< */,
                                              uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_motion_events_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_motion_events_reply_t * xcb_input_get_device_motion_events_reply
 ** 
 ** @param xcb_connection_t                             *c
 ** @param xcb_input_get_device_motion_events_cookie_t   cookie
 ** @param xcb_generic_error_t                         **e
 ** @returns xcb_input_get_device_motion_events_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_motion_events_reply_t *
xcb_input_get_device_motion_events_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_input_get_device_motion_events_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_time_coord_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_time_coord_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_time_coord_next
 ** 
 ** @param xcb_input_device_time_coord_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_time_coord_next (xcb_input_device_time_coord_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_time_coord_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_time_coord_end
 ** 
 ** @param xcb_input_device_time_coord_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_time_coord_end (xcb_input_device_time_coord_iterator_t i  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_change_keyboard_device_cookie_t xcb_input_change_keyboard_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_keyboard_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_change_keyboard_device_cookie_t
xcb_input_change_keyboard_device (xcb_connection_t *c  /**< */,
                                  uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_change_keyboard_device_cookie_t xcb_input_change_keyboard_device_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_keyboard_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_change_keyboard_device_cookie_t
xcb_input_change_keyboard_device_unchecked (xcb_connection_t *c  /**< */,
                                            uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_change_keyboard_device_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_change_keyboard_device_reply_t * xcb_input_change_keyboard_device_reply
 ** 
 ** @param xcb_connection_t                           *c
 ** @param xcb_input_change_keyboard_device_cookie_t   cookie
 ** @param xcb_generic_error_t                       **e
 ** @returns xcb_input_change_keyboard_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_change_keyboard_device_reply_t *
xcb_input_change_keyboard_device_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_change_keyboard_device_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_change_pointer_device_cookie_t xcb_input_change_pointer_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           x_axis
 ** @param uint8_t           y_axis
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_pointer_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_change_pointer_device_cookie_t
xcb_input_change_pointer_device (xcb_connection_t *c  /**< */,
                                 uint8_t           x_axis  /**< */,
                                 uint8_t           y_axis  /**< */,
                                 uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_change_pointer_device_cookie_t xcb_input_change_pointer_device_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           x_axis
 ** @param uint8_t           y_axis
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_pointer_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_change_pointer_device_cookie_t
xcb_input_change_pointer_device_unchecked (xcb_connection_t *c  /**< */,
                                           uint8_t           x_axis  /**< */,
                                           uint8_t           y_axis  /**< */,
                                           uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_change_pointer_device_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_change_pointer_device_reply_t * xcb_input_change_pointer_device_reply
 ** 
 ** @param xcb_connection_t                          *c
 ** @param xcb_input_change_pointer_device_cookie_t   cookie
 ** @param xcb_generic_error_t                      **e
 ** @returns xcb_input_change_pointer_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_change_pointer_device_reply_t *
xcb_input_change_pointer_device_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_change_pointer_device_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */);

int
xcb_input_grab_device_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_grab_device_cookie_t xcb_input_grab_device
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param xcb_timestamp_t                time
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param uint8_t                        device_id
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_input_grab_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_grab_device_cookie_t
xcb_input_grab_device (xcb_connection_t              *c  /**< */,
                       xcb_window_t                   grab_window  /**< */,
                       xcb_timestamp_t                time  /**< */,
                       uint16_t                       num_classes  /**< */,
                       uint8_t                        this_device_mode  /**< */,
                       uint8_t                        other_device_mode  /**< */,
                       uint8_t                        owner_events  /**< */,
                       uint8_t                        device_id  /**< */,
                       const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_grab_device_cookie_t xcb_input_grab_device_unchecked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param xcb_timestamp_t                time
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param uint8_t                        device_id
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_input_grab_device_cookie_t
 **
 *****************************************************************************/
 
xcb_input_grab_device_cookie_t
xcb_input_grab_device_unchecked (xcb_connection_t              *c  /**< */,
                                 xcb_window_t                   grab_window  /**< */,
                                 xcb_timestamp_t                time  /**< */,
                                 uint16_t                       num_classes  /**< */,
                                 uint8_t                        this_device_mode  /**< */,
                                 uint8_t                        other_device_mode  /**< */,
                                 uint8_t                        owner_events  /**< */,
                                 uint8_t                        device_id  /**< */,
                                 const xcb_input_event_class_t *classes  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_grab_device_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_grab_device_reply_t * xcb_input_grab_device_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_input_grab_device_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_input_grab_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_grab_device_reply_t *
xcb_input_grab_device_reply (xcb_connection_t                *c  /**< */,
                             xcb_input_grab_device_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_ungrab_device_checked (xcb_connection_t *c  /**< */,
                                 xcb_timestamp_t   time  /**< */,
                                 uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_ungrab_device (xcb_connection_t *c  /**< */,
                         xcb_timestamp_t   time  /**< */,
                         uint8_t           device_id  /**< */);

int
xcb_input_grab_device_key_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_key_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        modifier_device
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        key
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_grab_device_key_checked (xcb_connection_t              *c  /**< */,
                                   xcb_window_t                   grab_window  /**< */,
                                   uint16_t                       num_classes  /**< */,
                                   uint16_t                       modifiers  /**< */,
                                   uint8_t                        modifier_device  /**< */,
                                   uint8_t                        grabbed_device  /**< */,
                                   uint8_t                        key  /**< */,
                                   uint8_t                        this_device_mode  /**< */,
                                   uint8_t                        other_device_mode  /**< */,
                                   uint8_t                        owner_events  /**< */,
                                   const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_key
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        modifier_device
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        key
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_grab_device_key (xcb_connection_t              *c  /**< */,
                           xcb_window_t                   grab_window  /**< */,
                           uint16_t                       num_classes  /**< */,
                           uint16_t                       modifiers  /**< */,
                           uint8_t                        modifier_device  /**< */,
                           uint8_t                        grabbed_device  /**< */,
                           uint8_t                        key  /**< */,
                           uint8_t                        this_device_mode  /**< */,
                           uint8_t                        other_device_mode  /**< */,
                           uint8_t                        owner_events  /**< */,
                           const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_key_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grabWindow
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           key
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_ungrab_device_key_checked (xcb_connection_t *c  /**< */,
                                     xcb_window_t      grabWindow  /**< */,
                                     uint16_t          modifiers  /**< */,
                                     uint8_t           modifier_device  /**< */,
                                     uint8_t           key  /**< */,
                                     uint8_t           grabbed_device  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_key
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grabWindow
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           key
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_ungrab_device_key (xcb_connection_t *c  /**< */,
                             xcb_window_t      grabWindow  /**< */,
                             uint16_t          modifiers  /**< */,
                             uint8_t           modifier_device  /**< */,
                             uint8_t           key  /**< */,
                             uint8_t           grabbed_device  /**< */);

int
xcb_input_grab_device_button_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_button_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        modifier_device
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        button
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_grab_device_button_checked (xcb_connection_t              *c  /**< */,
                                      xcb_window_t                   grab_window  /**< */,
                                      uint8_t                        grabbed_device  /**< */,
                                      uint8_t                        modifier_device  /**< */,
                                      uint16_t                       num_classes  /**< */,
                                      uint16_t                       modifiers  /**< */,
                                      uint8_t                        this_device_mode  /**< */,
                                      uint8_t                        other_device_mode  /**< */,
                                      uint8_t                        button  /**< */,
                                      uint8_t                        owner_events  /**< */,
                                      const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_button
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        modifier_device
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        button
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_grab_device_button (xcb_connection_t              *c  /**< */,
                              xcb_window_t                   grab_window  /**< */,
                              uint8_t                        grabbed_device  /**< */,
                              uint8_t                        modifier_device  /**< */,
                              uint16_t                       num_classes  /**< */,
                              uint16_t                       modifiers  /**< */,
                              uint8_t                        this_device_mode  /**< */,
                              uint8_t                        other_device_mode  /**< */,
                              uint8_t                        button  /**< */,
                              uint8_t                        owner_events  /**< */,
                              const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_button_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           button
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_ungrab_device_button_checked (xcb_connection_t *c  /**< */,
                                        xcb_window_t      grab_window  /**< */,
                                        uint16_t          modifiers  /**< */,
                                        uint8_t           modifier_device  /**< */,
                                        uint8_t           button  /**< */,
                                        uint8_t           grabbed_device  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_button
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           button
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_ungrab_device_button (xcb_connection_t *c  /**< */,
                                xcb_window_t      grab_window  /**< */,
                                uint16_t          modifiers  /**< */,
                                uint8_t           modifier_device  /**< */,
                                uint8_t           button  /**< */,
                                uint8_t           grabbed_device  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_allow_device_events_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           mode
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_allow_device_events_checked (xcb_connection_t *c  /**< */,
                                       xcb_timestamp_t   time  /**< */,
                                       uint8_t           mode  /**< */,
                                       uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_allow_device_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           mode
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_allow_device_events (xcb_connection_t *c  /**< */,
                               xcb_timestamp_t   time  /**< */,
                               uint8_t           mode  /**< */,
                               uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_focus_cookie_t xcb_input_get_device_focus
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_focus_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_focus_cookie_t
xcb_input_get_device_focus (xcb_connection_t *c  /**< */,
                            uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_focus_cookie_t xcb_input_get_device_focus_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_focus_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_focus_cookie_t
xcb_input_get_device_focus_unchecked (xcb_connection_t *c  /**< */,
                                      uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_focus_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_focus_reply_t * xcb_input_get_device_focus_reply
 ** 
 ** @param xcb_connection_t                     *c
 ** @param xcb_input_get_device_focus_cookie_t   cookie
 ** @param xcb_generic_error_t                 **e
 ** @returns xcb_input_get_device_focus_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_focus_reply_t *
xcb_input_get_device_focus_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_input_get_device_focus_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_set_device_focus_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      focus
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           revert_to
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_set_device_focus_checked (xcb_connection_t *c  /**< */,
                                    xcb_window_t      focus  /**< */,
                                    xcb_timestamp_t   time  /**< */,
                                    uint8_t           revert_to  /**< */,
                                    uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_set_device_focus
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      focus
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           revert_to
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_set_device_focus (xcb_connection_t *c  /**< */,
                            xcb_window_t      focus  /**< */,
                            xcb_timestamp_t   time  /**< */,
                            uint8_t           revert_to  /**< */,
                            uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_feedback_control_cookie_t xcb_input_get_feedback_control
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_feedback_control_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_feedback_control_cookie_t
xcb_input_get_feedback_control (xcb_connection_t *c  /**< */,
                                uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_feedback_control_cookie_t xcb_input_get_feedback_control_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_feedback_control_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_feedback_control_cookie_t
xcb_input_get_feedback_control_unchecked (xcb_connection_t *c  /**< */,
                                          uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_feedback_control_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_feedback_control_reply_t * xcb_input_get_feedback_control_reply
 ** 
 ** @param xcb_connection_t                         *c
 ** @param xcb_input_get_feedback_control_cookie_t   cookie
 ** @param xcb_generic_error_t                     **e
 ** @returns xcb_input_get_feedback_control_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_feedback_control_reply_t *
xcb_input_get_feedback_control_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_input_get_feedback_control_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_feedback_state_next
 ** 
 ** @param xcb_input_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_feedback_state_next (xcb_input_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_feedback_state_end
 ** 
 ** @param xcb_input_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_feedback_state_end (xcb_input_feedback_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_kbd_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_kbd_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_kbd_feedback_state_next
 ** 
 ** @param xcb_input_kbd_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_kbd_feedback_state_next (xcb_input_kbd_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_kbd_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_kbd_feedback_state_end
 ** 
 ** @param xcb_input_kbd_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_kbd_feedback_state_end (xcb_input_kbd_feedback_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_ptr_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_ptr_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_ptr_feedback_state_next
 ** 
 ** @param xcb_input_ptr_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_ptr_feedback_state_next (xcb_input_ptr_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_ptr_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_ptr_feedback_state_end
 ** 
 ** @param xcb_input_ptr_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_ptr_feedback_state_end (xcb_input_ptr_feedback_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_integer_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_integer_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_integer_feedback_state_next
 ** 
 ** @param xcb_input_integer_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_integer_feedback_state_next (xcb_input_integer_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_integer_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_integer_feedback_state_end
 ** 
 ** @param xcb_input_integer_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_integer_feedback_state_end (xcb_input_integer_feedback_state_iterator_t i  /**< */);

int
xcb_input_string_feedback_state_sizeof (const void  *_buffer  /**< */);


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_input_string_feedback_state_keysyms
 ** 
 ** @param const xcb_input_string_feedback_state_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_input_string_feedback_state_keysyms (const xcb_input_string_feedback_state_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_string_feedback_state_keysyms_length
 ** 
 ** @param const xcb_input_string_feedback_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_string_feedback_state_keysyms_length (const xcb_input_string_feedback_state_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_state_keysyms_end
 ** 
 ** @param const xcb_input_string_feedback_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_string_feedback_state_keysyms_end (const xcb_input_string_feedback_state_t *R  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_string_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_string_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_string_feedback_state_next
 ** 
 ** @param xcb_input_string_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_string_feedback_state_next (xcb_input_string_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_string_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_state_end
 ** 
 ** @param xcb_input_string_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_string_feedback_state_end (xcb_input_string_feedback_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_bell_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_bell_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_bell_feedback_state_next
 ** 
 ** @param xcb_input_bell_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_bell_feedback_state_next (xcb_input_bell_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_bell_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_bell_feedback_state_end
 ** 
 ** @param xcb_input_bell_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_bell_feedback_state_end (xcb_input_bell_feedback_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_led_feedback_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_led_feedback_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_led_feedback_state_next
 ** 
 ** @param xcb_input_led_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_led_feedback_state_next (xcb_input_led_feedback_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_led_feedback_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_led_feedback_state_end
 ** 
 ** @param xcb_input_led_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_led_feedback_state_end (xcb_input_led_feedback_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_feedback_ctl_next
 ** 
 ** @param xcb_input_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_feedback_ctl_next (xcb_input_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_feedback_ctl_end
 ** 
 ** @param xcb_input_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_feedback_ctl_end (xcb_input_feedback_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_kbd_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_kbd_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_kbd_feedback_ctl_next
 ** 
 ** @param xcb_input_kbd_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_kbd_feedback_ctl_next (xcb_input_kbd_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_kbd_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_kbd_feedback_ctl_end
 ** 
 ** @param xcb_input_kbd_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_kbd_feedback_ctl_end (xcb_input_kbd_feedback_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_ptr_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_ptr_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_ptr_feedback_ctl_next
 ** 
 ** @param xcb_input_ptr_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_ptr_feedback_ctl_next (xcb_input_ptr_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_ptr_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_ptr_feedback_ctl_end
 ** 
 ** @param xcb_input_ptr_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_ptr_feedback_ctl_end (xcb_input_ptr_feedback_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_integer_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_integer_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_integer_feedback_ctl_next
 ** 
 ** @param xcb_input_integer_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_integer_feedback_ctl_next (xcb_input_integer_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_integer_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_integer_feedback_ctl_end
 ** 
 ** @param xcb_input_integer_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_integer_feedback_ctl_end (xcb_input_integer_feedback_ctl_iterator_t i  /**< */);

int
xcb_input_string_feedback_ctl_sizeof (const void  *_buffer  /**< */);


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_input_string_feedback_ctl_keysyms
 ** 
 ** @param const xcb_input_string_feedback_ctl_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_input_string_feedback_ctl_keysyms (const xcb_input_string_feedback_ctl_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_string_feedback_ctl_keysyms_length
 ** 
 ** @param const xcb_input_string_feedback_ctl_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_string_feedback_ctl_keysyms_length (const xcb_input_string_feedback_ctl_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_ctl_keysyms_end
 ** 
 ** @param const xcb_input_string_feedback_ctl_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_string_feedback_ctl_keysyms_end (const xcb_input_string_feedback_ctl_t *R  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_string_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_string_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_string_feedback_ctl_next
 ** 
 ** @param xcb_input_string_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_string_feedback_ctl_next (xcb_input_string_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_string_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_ctl_end
 ** 
 ** @param xcb_input_string_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_string_feedback_ctl_end (xcb_input_string_feedback_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_bell_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_bell_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_bell_feedback_ctl_next
 ** 
 ** @param xcb_input_bell_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_bell_feedback_ctl_next (xcb_input_bell_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_bell_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_bell_feedback_ctl_end
 ** 
 ** @param xcb_input_bell_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_bell_feedback_ctl_end (xcb_input_bell_feedback_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_led_feedback_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_led_feedback_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_led_feedback_ctl_next
 ** 
 ** @param xcb_input_led_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_led_feedback_ctl_next (xcb_input_led_feedback_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_led_feedback_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_led_feedback_ctl_end
 ** 
 ** @param xcb_input_led_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_led_feedback_ctl_end (xcb_input_led_feedback_ctl_iterator_t i  /**< */);

int
xcb_input_get_device_key_mapping_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_key_mapping_cookie_t xcb_input_get_device_key_mapping
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               count
 ** @returns xcb_input_get_device_key_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_key_mapping_cookie_t
xcb_input_get_device_key_mapping (xcb_connection_t     *c  /**< */,
                                  uint8_t               device_id  /**< */,
                                  xcb_input_key_code_t  first_keycode  /**< */,
                                  uint8_t               count  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_key_mapping_cookie_t xcb_input_get_device_key_mapping_unchecked
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               count
 ** @returns xcb_input_get_device_key_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_key_mapping_cookie_t
xcb_input_get_device_key_mapping_unchecked (xcb_connection_t     *c  /**< */,
                                            uint8_t               device_id  /**< */,
                                            xcb_input_key_code_t  first_keycode  /**< */,
                                            uint8_t               count  /**< */);


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_input_get_device_key_mapping_keysyms
 ** 
 ** @param const xcb_input_get_device_key_mapping_reply_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_input_get_device_key_mapping_keysyms (const xcb_input_get_device_key_mapping_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_get_device_key_mapping_keysyms_length
 ** 
 ** @param const xcb_input_get_device_key_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_key_mapping_keysyms_length (const xcb_input_get_device_key_mapping_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_key_mapping_keysyms_end
 ** 
 ** @param const xcb_input_get_device_key_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_key_mapping_keysyms_end (const xcb_input_get_device_key_mapping_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_key_mapping_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_key_mapping_reply_t * xcb_input_get_device_key_mapping_reply
 ** 
 ** @param xcb_connection_t                           *c
 ** @param xcb_input_get_device_key_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                       **e
 ** @returns xcb_input_get_device_key_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_key_mapping_reply_t *
xcb_input_get_device_key_mapping_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_get_device_key_mapping_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */);

int
xcb_input_change_device_key_mapping_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_key_mapping_checked
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               keysyms_per_keycode
 ** @param uint8_t               keycode_count
 ** @param const xcb_keysym_t   *keysyms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_change_device_key_mapping_checked (xcb_connection_t     *c  /**< */,
                                             uint8_t               device_id  /**< */,
                                             xcb_input_key_code_t  first_keycode  /**< */,
                                             uint8_t               keysyms_per_keycode  /**< */,
                                             uint8_t               keycode_count  /**< */,
                                             const xcb_keysym_t   *keysyms  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_key_mapping
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               keysyms_per_keycode
 ** @param uint8_t               keycode_count
 ** @param const xcb_keysym_t   *keysyms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_change_device_key_mapping (xcb_connection_t     *c  /**< */,
                                     uint8_t               device_id  /**< */,
                                     xcb_input_key_code_t  first_keycode  /**< */,
                                     uint8_t               keysyms_per_keycode  /**< */,
                                     uint8_t               keycode_count  /**< */,
                                     const xcb_keysym_t   *keysyms  /**< */);

int
xcb_input_get_device_modifier_mapping_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_modifier_mapping_cookie_t xcb_input_get_device_modifier_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_modifier_mapping_cookie_t
xcb_input_get_device_modifier_mapping (xcb_connection_t *c  /**< */,
                                       uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_modifier_mapping_cookie_t xcb_input_get_device_modifier_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_modifier_mapping_cookie_t
xcb_input_get_device_modifier_mapping_unchecked (xcb_connection_t *c  /**< */,
                                                 uint8_t           device_id  /**< */);


/*****************************************************************************
 **
 ** uint8_t * xcb_input_get_device_modifier_mapping_keymaps
 ** 
 ** @param const xcb_input_get_device_modifier_mapping_reply_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_input_get_device_modifier_mapping_keymaps (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_get_device_modifier_mapping_keymaps_length
 ** 
 ** @param const xcb_input_get_device_modifier_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_modifier_mapping_keymaps_length (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_modifier_mapping_keymaps_end
 ** 
 ** @param const xcb_input_get_device_modifier_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_modifier_mapping_keymaps_end (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_modifier_mapping_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_modifier_mapping_reply_t * xcb_input_get_device_modifier_mapping_reply
 ** 
 ** @param xcb_connection_t                                *c
 ** @param xcb_input_get_device_modifier_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                            **e
 ** @returns xcb_input_get_device_modifier_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_modifier_mapping_reply_t *
xcb_input_get_device_modifier_mapping_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_input_get_device_modifier_mapping_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */);

int
xcb_input_set_device_modifier_mapping_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_modifier_mapping_cookie_t xcb_input_set_device_modifier_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           keycodes_per_modifier
 ** @param const uint8_t    *keymaps
 ** @returns xcb_input_set_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_modifier_mapping_cookie_t
xcb_input_set_device_modifier_mapping (xcb_connection_t *c  /**< */,
                                       uint8_t           device_id  /**< */,
                                       uint8_t           keycodes_per_modifier  /**< */,
                                       const uint8_t    *keymaps  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_modifier_mapping_cookie_t xcb_input_set_device_modifier_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           keycodes_per_modifier
 ** @param const uint8_t    *keymaps
 ** @returns xcb_input_set_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_modifier_mapping_cookie_t
xcb_input_set_device_modifier_mapping_unchecked (xcb_connection_t *c  /**< */,
                                                 uint8_t           device_id  /**< */,
                                                 uint8_t           keycodes_per_modifier  /**< */,
                                                 const uint8_t    *keymaps  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_set_device_modifier_mapping_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_modifier_mapping_reply_t * xcb_input_set_device_modifier_mapping_reply
 ** 
 ** @param xcb_connection_t                                *c
 ** @param xcb_input_set_device_modifier_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                            **e
 ** @returns xcb_input_set_device_modifier_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_input_set_device_modifier_mapping_reply_t *
xcb_input_set_device_modifier_mapping_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_input_set_device_modifier_mapping_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */);

int
xcb_input_get_device_button_mapping_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_button_mapping_cookie_t xcb_input_get_device_button_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_button_mapping_cookie_t
xcb_input_get_device_button_mapping (xcb_connection_t *c  /**< */,
                                     uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_button_mapping_cookie_t xcb_input_get_device_button_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_button_mapping_cookie_t
xcb_input_get_device_button_mapping_unchecked (xcb_connection_t *c  /**< */,
                                               uint8_t           device_id  /**< */);


/*****************************************************************************
 **
 ** uint8_t * xcb_input_get_device_button_mapping_map
 ** 
 ** @param const xcb_input_get_device_button_mapping_reply_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_input_get_device_button_mapping_map (const xcb_input_get_device_button_mapping_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_get_device_button_mapping_map_length
 ** 
 ** @param const xcb_input_get_device_button_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_button_mapping_map_length (const xcb_input_get_device_button_mapping_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_button_mapping_map_end
 ** 
 ** @param const xcb_input_get_device_button_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_button_mapping_map_end (const xcb_input_get_device_button_mapping_reply_t *R  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_button_mapping_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_button_mapping_reply_t * xcb_input_get_device_button_mapping_reply
 ** 
 ** @param xcb_connection_t                              *c
 ** @param xcb_input_get_device_button_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                          **e
 ** @returns xcb_input_get_device_button_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_button_mapping_reply_t *
xcb_input_get_device_button_mapping_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_input_get_device_button_mapping_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */);

int
xcb_input_set_device_button_mapping_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_button_mapping_cookie_t xcb_input_set_device_button_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           map_size
 ** @param const uint8_t    *map
 ** @returns xcb_input_set_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_button_mapping_cookie_t
xcb_input_set_device_button_mapping (xcb_connection_t *c  /**< */,
                                     uint8_t           device_id  /**< */,
                                     uint8_t           map_size  /**< */,
                                     const uint8_t    *map  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_button_mapping_cookie_t xcb_input_set_device_button_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           map_size
 ** @param const uint8_t    *map
 ** @returns xcb_input_set_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_button_mapping_cookie_t
xcb_input_set_device_button_mapping_unchecked (xcb_connection_t *c  /**< */,
                                               uint8_t           device_id  /**< */,
                                               uint8_t           map_size  /**< */,
                                               const uint8_t    *map  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_set_device_button_mapping_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_button_mapping_reply_t * xcb_input_set_device_button_mapping_reply
 ** 
 ** @param xcb_connection_t                              *c
 ** @param xcb_input_set_device_button_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                          **e
 ** @returns xcb_input_set_device_button_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_input_set_device_button_mapping_reply_t *
xcb_input_set_device_button_mapping_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_input_set_device_button_mapping_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_query_device_state_cookie_t xcb_input_query_device_state
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_query_device_state_cookie_t
 **
 *****************************************************************************/
 
xcb_input_query_device_state_cookie_t
xcb_input_query_device_state (xcb_connection_t *c  /**< */,
                              uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_query_device_state_cookie_t xcb_input_query_device_state_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_query_device_state_cookie_t
 **
 *****************************************************************************/
 
xcb_input_query_device_state_cookie_t
xcb_input_query_device_state_unchecked (xcb_connection_t *c  /**< */,
                                        uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_query_device_state_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_query_device_state_reply_t * xcb_input_query_device_state_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_input_query_device_state_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_input_query_device_state_reply_t *
 **
 *****************************************************************************/
 
xcb_input_query_device_state_reply_t *
xcb_input_query_device_state_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_query_device_state_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_input_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_input_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_input_state_next
 ** 
 ** @param xcb_input_input_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_input_state_next (xcb_input_input_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_input_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_input_state_end
 ** 
 ** @param xcb_input_input_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_input_state_end (xcb_input_input_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_key_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_key_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_key_state_next
 ** 
 ** @param xcb_input_key_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_key_state_next (xcb_input_key_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_key_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_key_state_end
 ** 
 ** @param xcb_input_key_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_key_state_end (xcb_input_key_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_button_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_button_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_button_state_next
 ** 
 ** @param xcb_input_button_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_button_state_next (xcb_input_button_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_button_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_button_state_end
 ** 
 ** @param xcb_input_button_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_button_state_end (xcb_input_button_state_iterator_t i  /**< */);

int
xcb_input_valuator_state_sizeof (const void  *_buffer  /**< */);


/*****************************************************************************
 **
 ** uint32_t * xcb_input_valuator_state_valuators
 ** 
 ** @param const xcb_input_valuator_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_valuator_state_valuators (const xcb_input_valuator_state_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_valuator_state_valuators_length
 ** 
 ** @param const xcb_input_valuator_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_valuator_state_valuators_length (const xcb_input_valuator_state_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_valuator_state_valuators_end
 ** 
 ** @param const xcb_input_valuator_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_valuator_state_valuators_end (const xcb_input_valuator_state_t *R  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_valuator_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_valuator_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_valuator_state_next
 ** 
 ** @param xcb_input_valuator_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_valuator_state_next (xcb_input_valuator_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_valuator_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_valuator_state_end
 ** 
 ** @param xcb_input_valuator_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_valuator_state_end (xcb_input_valuator_state_iterator_t i  /**< */);

int
xcb_input_send_extension_event_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_send_extension_event_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   destination
 ** @param uint8_t                        device_id
 ** @param uint8_t                        propagate
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        num_events
 ** @param const char                    *events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_send_extension_event_checked (xcb_connection_t              *c  /**< */,
                                        xcb_window_t                   destination  /**< */,
                                        uint8_t                        device_id  /**< */,
                                        uint8_t                        propagate  /**< */,
                                        uint16_t                       num_classes  /**< */,
                                        uint8_t                        num_events  /**< */,
                                        const char                    *events  /**< */,
                                        const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_send_extension_event
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   destination
 ** @param uint8_t                        device_id
 ** @param uint8_t                        propagate
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        num_events
 ** @param const char                    *events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_send_extension_event (xcb_connection_t              *c  /**< */,
                                xcb_window_t                   destination  /**< */,
                                uint8_t                        device_id  /**< */,
                                uint8_t                        propagate  /**< */,
                                uint16_t                       num_classes  /**< */,
                                uint8_t                        num_events  /**< */,
                                const char                    *events  /**< */,
                                const xcb_input_event_class_t *classes  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will not cause
 * a reply to be generated. Any returned error will be
 * saved for handling by xcb_request_check().
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_device_bell_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           feedback_id
 ** @param uint8_t           feedback_class
 ** @param int8_t            percent
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_device_bell_checked (xcb_connection_t *c  /**< */,
                               uint8_t           device_id  /**< */,
                               uint8_t           feedback_id  /**< */,
                               uint8_t           feedback_class  /**< */,
                               int8_t            percent  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_device_bell
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           feedback_id
 ** @param uint8_t           feedback_class
 ** @param int8_t            percent
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_device_bell (xcb_connection_t *c  /**< */,
                       uint8_t           device_id  /**< */,
                       uint8_t           feedback_id  /**< */,
                       uint8_t           feedback_class  /**< */,
                       int8_t            percent  /**< */);

int
xcb_input_set_device_valuators_sizeof (const void  *_buffer  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_valuators_cookie_t xcb_input_set_device_valuators
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           first_valuator
 ** @param uint8_t           num_valuators
 ** @param const int32_t    *valuators
 ** @returns xcb_input_set_device_valuators_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_valuators_cookie_t
xcb_input_set_device_valuators (xcb_connection_t *c  /**< */,
                                uint8_t           device_id  /**< */,
                                uint8_t           first_valuator  /**< */,
                                uint8_t           num_valuators  /**< */,
                                const int32_t    *valuators  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_valuators_cookie_t xcb_input_set_device_valuators_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           first_valuator
 ** @param uint8_t           num_valuators
 ** @param const int32_t    *valuators
 ** @returns xcb_input_set_device_valuators_cookie_t
 **
 *****************************************************************************/
 
xcb_input_set_device_valuators_cookie_t
xcb_input_set_device_valuators_unchecked (xcb_connection_t *c  /**< */,
                                          uint8_t           device_id  /**< */,
                                          uint8_t           first_valuator  /**< */,
                                          uint8_t           num_valuators  /**< */,
                                          const int32_t    *valuators  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_set_device_valuators_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_set_device_valuators_reply_t * xcb_input_set_device_valuators_reply
 ** 
 ** @param xcb_connection_t                         *c
 ** @param xcb_input_set_device_valuators_cookie_t   cookie
 ** @param xcb_generic_error_t                     **e
 ** @returns xcb_input_set_device_valuators_reply_t *
 **
 *****************************************************************************/
 
xcb_input_set_device_valuators_reply_t *
xcb_input_set_device_valuators_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_input_set_device_valuators_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_control_cookie_t xcb_input_get_device_control
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          control_id
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_control_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_control_cookie_t
xcb_input_get_device_control (xcb_connection_t *c  /**< */,
                              uint16_t          control_id  /**< */,
                              uint8_t           device_id  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 * 
 * This form can be used only if the request will cause
 * a reply to be generated. Any returned error will be
 * placed in the event queue.
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_control_cookie_t xcb_input_get_device_control_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          control_id
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_control_cookie_t
 **
 *****************************************************************************/
 
xcb_input_get_device_control_cookie_t
xcb_input_get_device_control_unchecked (xcb_connection_t *c  /**< */,
                                        uint16_t          control_id  /**< */,
                                        uint8_t           device_id  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 * 
 * The parameter @p e supplied to this function must be NULL if
 * xcb_input_get_device_control_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */

/*****************************************************************************
 **
 ** xcb_input_get_device_control_reply_t * xcb_input_get_device_control_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_input_get_device_control_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_input_get_device_control_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_control_reply_t *
xcb_input_get_device_control_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_get_device_control_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_state_next
 ** 
 ** @param xcb_input_device_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_state_next (xcb_input_device_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_state_end
 ** 
 ** @param xcb_input_device_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_state_end (xcb_input_device_state_iterator_t i  /**< */);

int
xcb_input_device_resolution_state_sizeof (const void  *_buffer  /**< */);


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_state_resolution_values
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_state_resolution_values (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_state_resolution_values_length
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_state_resolution_values_length (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_resolution_values_end
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_values_end (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_state_resolution_min
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_state_resolution_min (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_state_resolution_min_length
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_state_resolution_min_length (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_resolution_min_end
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_min_end (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_state_resolution_max
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_state_resolution_max (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_state_resolution_max_length
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_state_resolution_max_length (const xcb_input_device_resolution_state_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_resolution_max_end
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_max_end (const xcb_input_device_resolution_state_t *R  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_resolution_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_resolution_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_resolution_state_next
 ** 
 ** @param xcb_input_device_resolution_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_resolution_state_next (xcb_input_device_resolution_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_resolution_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_end
 ** 
 ** @param xcb_input_device_resolution_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_state_end (xcb_input_device_resolution_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_abs_calib_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_abs_calib_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_abs_calib_state_next
 ** 
 ** @param xcb_input_device_abs_calib_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_calib_state_next (xcb_input_device_abs_calib_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_abs_calib_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_calib_state_end
 ** 
 ** @param xcb_input_device_abs_calib_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_calib_state_end (xcb_input_device_abs_calib_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_abs_area_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_abs_area_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_abs_area_state_next
 ** 
 ** @param xcb_input_device_abs_area_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_area_state_next (xcb_input_device_abs_area_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_abs_area_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_area_state_end
 ** 
 ** @param xcb_input_device_abs_area_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_area_state_end (xcb_input_device_abs_area_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_core_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_core_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_core_state_next
 ** 
 ** @param xcb_input_device_core_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_core_state_next (xcb_input_device_core_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_core_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_core_state_end
 ** 
 ** @param xcb_input_device_core_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_core_state_end (xcb_input_device_core_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_enable_state_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_enable_state_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_enable_state_next
 ** 
 ** @param xcb_input_device_enable_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_enable_state_next (xcb_input_device_enable_state_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_enable_state_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_enable_state_end
 ** 
 ** @param xcb_input_device_enable_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_enable_state_end (xcb_input_device_enable_state_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_ctl_next
 ** 
 ** @param xcb_input_device_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_ctl_next (xcb_input_device_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_ctl_end
 ** 
 ** @param xcb_input_device_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_ctl_end (xcb_input_device_ctl_iterator_t i  /**< */);

int
xcb_input_device_resolution_ctl_sizeof (const void  *_buffer  /**< */);


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_ctl_resolution_values
 ** 
 ** @param const xcb_input_device_resolution_ctl_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_ctl_resolution_values (const xcb_input_device_resolution_ctl_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_ctl_resolution_values_length
 ** 
 ** @param const xcb_input_device_resolution_ctl_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_ctl_resolution_values_length (const xcb_input_device_resolution_ctl_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_ctl_resolution_values_end
 ** 
 ** @param const xcb_input_device_resolution_ctl_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_ctl_resolution_values_end (const xcb_input_device_resolution_ctl_t *R  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_resolution_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_resolution_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_resolution_ctl_next
 ** 
 ** @param xcb_input_device_resolution_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_resolution_ctl_next (xcb_input_device_resolution_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_resolution_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_ctl_end
 ** 
 ** @param xcb_input_device_resolution_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_ctl_end (xcb_input_device_resolution_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_abs_calib_ctl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_abs_calib_ctl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_abs_calib_ctl_next
 ** 
 ** @param xcb_input_device_abs_calib_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_calib_ctl_next (xcb_input_device_abs_calib_ctl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_abs_calib_ctl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_calib_ctl_end
 ** 
 ** @param xcb_input_device_abs_calib_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_calib_ctl_end (xcb_input_device_abs_calib_ctl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_abs_area_ctrl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_abs_area_ctrl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_abs_area_ctrl_next
 ** 
 ** @param xcb_input_device_abs_area_ctrl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_area_ctrl_next (xcb_input_device_abs_area_ctrl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_abs_area_ctrl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_area_ctrl_end
 ** 
 ** @param xcb_input_device_abs_area_ctrl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_area_ctrl_end (xcb_input_device_abs_area_ctrl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_core_ctrl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_core_ctrl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_core_ctrl_next
 ** 
 ** @param xcb_input_device_core_ctrl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_core_ctrl_next (xcb_input_device_core_ctrl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_core_ctrl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_core_ctrl_end
 ** 
 ** @param xcb_input_device_core_ctrl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_core_ctrl_end (xcb_input_device_core_ctrl_iterator_t i  /**< */);

/**
 * Get the next element of the iterator
 * @param i Pointer to a xcb_input_device_enable_ctrl_iterator_t
 *
 * Get the next element in the iterator. The member rem is
 * decreased by one. The member data points to the next
 * element. The member index is increased by sizeof(xcb_input_device_enable_ctrl_t)
 */

/*****************************************************************************
 **
 ** void xcb_input_device_enable_ctrl_next
 ** 
 ** @param xcb_input_device_enable_ctrl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_enable_ctrl_next (xcb_input_device_enable_ctrl_iterator_t *i  /**< */);

/**
 * Return the iterator pointing to the last element
 * @param i An xcb_input_device_enable_ctrl_iterator_t
 * @return  The iterator pointing to the last element
 *
 * Set the current element in the iterator to the last element.
 * The member rem is set to 0. The member data points to the
 * last element.
 */

/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_enable_ctrl_end
 ** 
 ** @param xcb_input_device_enable_ctrl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_enable_ctrl_end (xcb_input_device_enable_ctrl_iterator_t i  /**< */);


#ifdef __cplusplus
}
#endif

#endif

/**
 * @}
 */
