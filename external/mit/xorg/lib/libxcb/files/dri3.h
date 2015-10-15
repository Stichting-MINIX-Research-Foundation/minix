/*
 * This file generated automatically from dri3.xml by c_client.py.
 * Edit at your peril.
 */

/**
 * @defgroup XCB_DRI3_API XCB DRI3 API
 * @brief DRI3 XCB Protocol Implementation.
 * @{
 **/

#ifndef __DRI3_H
#define __DRI3_H

#include "xcb.h"
#include "xproto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XCB_DRI3_MAJOR_VERSION 1
#define XCB_DRI3_MINOR_VERSION 0

extern xcb_extension_t xcb_dri3_id;

/**
 * @brief xcb_dri3_query_version_cookie_t
 **/
typedef struct xcb_dri3_query_version_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_query_version_cookie_t;

/** Opcode for xcb_dri3_query_version. */
#define XCB_DRI3_QUERY_VERSION 0

/**
 * @brief xcb_dri3_query_version_request_t
 **/
typedef struct xcb_dri3_query_version_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t major_version; /**<  */
    uint32_t minor_version; /**<  */
} xcb_dri3_query_version_request_t;

/**
 * @brief xcb_dri3_query_version_reply_t
 **/
typedef struct xcb_dri3_query_version_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t major_version; /**<  */
    uint32_t minor_version; /**<  */
} xcb_dri3_query_version_reply_t;

/**
 * @brief xcb_dri3_open_cookie_t
 **/
typedef struct xcb_dri3_open_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_open_cookie_t;

/** Opcode for xcb_dri3_open. */
#define XCB_DRI3_OPEN 1

/**
 * @brief xcb_dri3_open_request_t
 **/
typedef struct xcb_dri3_open_request_t {
    uint8_t        major_opcode; /**<  */
    uint8_t        minor_opcode; /**<  */
    uint16_t       length; /**<  */
    xcb_drawable_t drawable; /**<  */
    uint32_t       provider; /**<  */
} xcb_dri3_open_request_t;

/**
 * @brief xcb_dri3_open_reply_t
 **/
typedef struct xcb_dri3_open_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  nfd; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad0[24]; /**<  */
} xcb_dri3_open_reply_t;

/** Opcode for xcb_dri3_pixmap_from_buffer. */
#define XCB_DRI3_PIXMAP_FROM_BUFFER 2

/**
 * @brief xcb_dri3_pixmap_from_buffer_request_t
 **/
typedef struct xcb_dri3_pixmap_from_buffer_request_t {
    uint8_t        major_opcode; /**<  */
    uint8_t        minor_opcode; /**<  */
    uint16_t       length; /**<  */
    xcb_pixmap_t   pixmap; /**<  */
    xcb_drawable_t drawable; /**<  */
    uint32_t       size; /**<  */
    uint16_t       width; /**<  */
    uint16_t       height; /**<  */
    uint16_t       stride; /**<  */
    uint8_t        depth; /**<  */
    uint8_t        bpp; /**<  */
} xcb_dri3_pixmap_from_buffer_request_t;

/**
 * @brief xcb_dri3_buffer_from_pixmap_cookie_t
 **/
typedef struct xcb_dri3_buffer_from_pixmap_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_buffer_from_pixmap_cookie_t;

/** Opcode for xcb_dri3_buffer_from_pixmap. */
#define XCB_DRI3_BUFFER_FROM_PIXMAP 3

/**
 * @brief xcb_dri3_buffer_from_pixmap_request_t
 **/
typedef struct xcb_dri3_buffer_from_pixmap_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_pixmap_t pixmap; /**<  */
} xcb_dri3_buffer_from_pixmap_request_t;

/**
 * @brief xcb_dri3_buffer_from_pixmap_reply_t
 **/
typedef struct xcb_dri3_buffer_from_pixmap_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  nfd; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t size; /**<  */
    uint16_t width; /**<  */
    uint16_t height; /**<  */
    uint16_t stride; /**<  */
    uint8_t  depth; /**<  */
    uint8_t  bpp; /**<  */
    uint8_t  pad0[12]; /**<  */
} xcb_dri3_buffer_from_pixmap_reply_t;

/** Opcode for xcb_dri3_fence_from_fd. */
#define XCB_DRI3_FENCE_FROM_FD 4

/**
 * @brief xcb_dri3_fence_from_fd_request_t
 **/
typedef struct xcb_dri3_fence_from_fd_request_t {
    uint8_t        major_opcode; /**<  */
    uint8_t        minor_opcode; /**<  */
    uint16_t       length; /**<  */
    xcb_drawable_t drawable; /**<  */
    uint32_t       fence; /**<  */
    uint8_t        initially_triggered; /**<  */
    uint8_t        pad0[3]; /**<  */
} xcb_dri3_fence_from_fd_request_t;

/**
 * @brief xcb_dri3_fd_from_fence_cookie_t
 **/
typedef struct xcb_dri3_fd_from_fence_cookie_t {
    unsigned int sequence; /**<  */
} xcb_dri3_fd_from_fence_cookie_t;

/** Opcode for xcb_dri3_fd_from_fence. */
#define XCB_DRI3_FD_FROM_FENCE 5

/**
 * @brief xcb_dri3_fd_from_fence_request_t
 **/
typedef struct xcb_dri3_fd_from_fence_request_t {
    uint8_t        major_opcode; /**<  */
    uint8_t        minor_opcode; /**<  */
    uint16_t       length; /**<  */
    xcb_drawable_t drawable; /**<  */
    uint32_t       fence; /**<  */
} xcb_dri3_fd_from_fence_request_t;

/**
 * @brief xcb_dri3_fd_from_fence_reply_t
 **/
typedef struct xcb_dri3_fd_from_fence_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  nfd; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad0[24]; /**<  */
} xcb_dri3_fd_from_fence_reply_t;

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 *
 */
xcb_dri3_query_version_cookie_t
xcb_dri3_query_version (xcb_connection_t *c  /**< */,
                        uint32_t          major_version  /**< */,
                        uint32_t          minor_version  /**< */);

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
xcb_dri3_query_version_cookie_t
xcb_dri3_query_version_unchecked (xcb_connection_t *c  /**< */,
                                  uint32_t          major_version  /**< */,
                                  uint32_t          minor_version  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 *
 * The parameter @p e supplied to this function must be NULL if
 * xcb_dri3_query_version_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */
xcb_dri3_query_version_reply_t *
xcb_dri3_query_version_reply (xcb_connection_t                 *c  /**< */,
                              xcb_dri3_query_version_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 *
 */
xcb_dri3_open_cookie_t
xcb_dri3_open (xcb_connection_t *c  /**< */,
               xcb_drawable_t    drawable  /**< */,
               uint32_t          provider  /**< */);

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
xcb_dri3_open_cookie_t
xcb_dri3_open_unchecked (xcb_connection_t *c  /**< */,
                         xcb_drawable_t    drawable  /**< */,
                         uint32_t          provider  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 *
 * The parameter @p e supplied to this function must be NULL if
 * xcb_dri3_open_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */
xcb_dri3_open_reply_t *
xcb_dri3_open_reply (xcb_connection_t        *c  /**< */,
                     xcb_dri3_open_cookie_t   cookie  /**< */,
                     xcb_generic_error_t    **e  /**< */);

/**
 * Return the reply fds
 * @param c      The connection
 * @param reply  The reply
 *
 * Returns the array of reply fds of the request asked by
 *
 * The returned value must be freed by the caller using free().
 */
int *
xcb_dri3_open_reply_fds (xcb_connection_t       *c  /**< */,
                         xcb_dri3_open_reply_t  *reply  /**< */);

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
xcb_void_cookie_t
xcb_dri3_pixmap_from_buffer_checked (xcb_connection_t *c  /**< */,
                                     xcb_pixmap_t      pixmap  /**< */,
                                     xcb_drawable_t    drawable  /**< */,
                                     uint32_t          size  /**< */,
                                     uint16_t          width  /**< */,
                                     uint16_t          height  /**< */,
                                     uint16_t          stride  /**< */,
                                     uint8_t           depth  /**< */,
                                     uint8_t           bpp  /**< */,
                                     int32_t           pixmap_fd  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 *
 */
xcb_void_cookie_t
xcb_dri3_pixmap_from_buffer (xcb_connection_t *c  /**< */,
                             xcb_pixmap_t      pixmap  /**< */,
                             xcb_drawable_t    drawable  /**< */,
                             uint32_t          size  /**< */,
                             uint16_t          width  /**< */,
                             uint16_t          height  /**< */,
                             uint16_t          stride  /**< */,
                             uint8_t           depth  /**< */,
                             uint8_t           bpp  /**< */,
                             int32_t           pixmap_fd  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 *
 */
xcb_dri3_buffer_from_pixmap_cookie_t
xcb_dri3_buffer_from_pixmap (xcb_connection_t *c  /**< */,
                             xcb_pixmap_t      pixmap  /**< */);

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
xcb_dri3_buffer_from_pixmap_cookie_t
xcb_dri3_buffer_from_pixmap_unchecked (xcb_connection_t *c  /**< */,
                                       xcb_pixmap_t      pixmap  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 *
 * The parameter @p e supplied to this function must be NULL if
 * xcb_dri3_buffer_from_pixmap_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */
xcb_dri3_buffer_from_pixmap_reply_t *
xcb_dri3_buffer_from_pixmap_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_dri3_buffer_from_pixmap_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */);

/**
 * Return the reply fds
 * @param c      The connection
 * @param reply  The reply
 *
 * Returns the array of reply fds of the request asked by
 *
 * The returned value must be freed by the caller using free().
 */
int *
xcb_dri3_buffer_from_pixmap_reply_fds (xcb_connection_t                     *c  /**< */,
                                       xcb_dri3_buffer_from_pixmap_reply_t  *reply  /**< */);

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
xcb_void_cookie_t
xcb_dri3_fence_from_fd_checked (xcb_connection_t *c  /**< */,
                                xcb_drawable_t    drawable  /**< */,
                                uint32_t          fence  /**< */,
                                uint8_t           initially_triggered  /**< */,
                                int32_t           fence_fd  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 *
 */
xcb_void_cookie_t
xcb_dri3_fence_from_fd (xcb_connection_t *c  /**< */,
                        xcb_drawable_t    drawable  /**< */,
                        uint32_t          fence  /**< */,
                        uint8_t           initially_triggered  /**< */,
                        int32_t           fence_fd  /**< */);

/**
 *
 * @param c The connection
 * @return A cookie
 *
 * Delivers a request to the X server.
 *
 */
xcb_dri3_fd_from_fence_cookie_t
xcb_dri3_fd_from_fence (xcb_connection_t *c  /**< */,
                        xcb_drawable_t    drawable  /**< */,
                        uint32_t          fence  /**< */);

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
xcb_dri3_fd_from_fence_cookie_t
xcb_dri3_fd_from_fence_unchecked (xcb_connection_t *c  /**< */,
                                  xcb_drawable_t    drawable  /**< */,
                                  uint32_t          fence  /**< */);

/**
 * Return the reply
 * @param c      The connection
 * @param cookie The cookie
 * @param e      The xcb_generic_error_t supplied
 *
 * Returns the reply of the request asked by
 *
 * The parameter @p e supplied to this function must be NULL if
 * xcb_dri3_fd_from_fence_unchecked(). is used.
 * Otherwise, it stores the error if any.
 *
 * The returned value must be freed by the caller using free().
 */
xcb_dri3_fd_from_fence_reply_t *
xcb_dri3_fd_from_fence_reply (xcb_connection_t                 *c  /**< */,
                              xcb_dri3_fd_from_fence_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */);

/**
 * Return the reply fds
 * @param c      The connection
 * @param reply  The reply
 *
 * Returns the array of reply fds of the request asked by
 *
 * The returned value must be freed by the caller using free().
 */
int *
xcb_dri3_fd_from_fence_reply_fds (xcb_connection_t                *c  /**< */,
                                  xcb_dri3_fd_from_fence_reply_t  *reply  /**< */);


#ifdef __cplusplus
}
#endif

#endif

/**
 * @}
 */
