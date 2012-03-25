#include "syslib.h"
#include <assert.h>
#include <signal.h>
#include <minix/sysutil.h>

/* SEF Signal callbacks. */
static struct sef_cbs {
    sef_cb_signal_handler_t             sef_cb_signal_handler;
    sef_cb_signal_manager_t             sef_cb_signal_manager;
} sef_cbs = {
    SEF_CB_SIGNAL_HANDLER_DEFAULT,
    SEF_CB_SIGNAL_MANAGER_DEFAULT
};

/* SEF Signal prototypes for sef_receive(). */
int do_sef_signal_request(message *m_ptr);

/* Debug. */
EXTERN char* sef_debug_header(void);

/* Information about SELF. */
EXTERN endpoint_t sef_self_endpoint;

/*===========================================================================*
 *                         process_sigmgr_signals               	     *
 *===========================================================================*/
static void process_sigmgr_signals(void)
{
/* A signal manager has pending signals in the kernel. Process them. */
  endpoint_t target;
  sigset_t sigset;
  int signo, r;

  while (TRUE) {
      /* Get an arbitrary pending signal. */
      if((r=sys_getksig(&target, &sigset)) != OK)
          panic("SEF: sys_getksig failed: %d", r);

      if (target == NONE) {
          /* Stop if there are no more pending signals. */
          break;
      } else {
          /* Process every signal in the signal set. */
          r = OK;
          for (signo = SIGS_FIRST; signo <= SIGS_LAST; signo++) {
              if(sigismember(&sigset, signo)) {
                  /* Let the callback code process the signal. */
                  r = sef_cbs.sef_cb_signal_manager(target, signo);

                  /* Stop if process is gone. */
                  if(r == EDEADSRCDST) {
                      break;
                  }
              }
          }
          /* Tell the kernel we are done if the target is still alive. */
          if(r == OK) {
              if((r=sys_endksig(target)) != OK)
                  panic("SEF: sys_endksig failed :%d", r);
          }
      }
  }
}

/*===========================================================================*
 *                         process_sigmgr_self_signals               	     *
 *===========================================================================*/
static void process_sigmgr_self_signals(sigset_t sigset)
{
/* A signal manager has pending signals for itself. Process them. */
  int signo;

  for (signo = SIGS_FIRST; signo <= SIGS_LAST; signo++) {
      if(sigismember(&sigset, signo)) {
          /* Let the callback code process the signal. */
          sef_cbs.sef_cb_signal_handler(signo);
      }
  }
}

/*===========================================================================*
 *                           do_sef_signal_request             		     *
 *===========================================================================*/
int do_sef_signal_request(message *m_ptr)
{
/* Handle a SEF Signal request. */
  int signo;
  sigset_t sigset;

  if(m_ptr->m_source == SYSTEM) {
      /* Handle kernel signals. */
      sigset = m_ptr->NOTIFY_ARG;
      for (signo = SIGK_FIRST; signo <= SIGK_LAST; signo++) {
          if (sigismember(&sigset, signo)) {
              /* Let the callback code handle the kernel signal. */
              sef_cbs.sef_cb_signal_handler(signo);

              /* Handle SIGKSIG for a signal manager. */
              if(signo == SIGKSIG) {
                  process_sigmgr_signals();
              }
              /* Handle SIGKSIGSM for a signal manager. */
              else if(signo == SIGKSIGSM) {
                  process_sigmgr_self_signals(sigset);
              }
          }
      }
  }
  else {
      /* Handle system signals from a signal manager. */
      signo = m_ptr->SIGS_SIG_NUM;

      /* Debug. */
#if SEF_SIGNAL_DEBUG
      sef_signal_debug_begin();
      sef_signal_dprint("%s. Got a SEF Signal request for signal %d! About to handle signal.\n", 
          sef_debug_header(), signo);
      sef_signal_debug_end();
#endif

      /* Let the callback code handle the signal. */
      sef_cbs.sef_cb_signal_handler(signo);
  }

  /* Return OK not to let anybody else intercept the request. */
  return OK;
}

/*===========================================================================*
 *                        sef_setcb_signal_handler                           *
 *===========================================================================*/
void sef_setcb_signal_handler(sef_cb_signal_handler_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_signal_handler = cb;
}

/*===========================================================================*
 *                        sef_setcb_signal_manager                           *
 *===========================================================================*/
void sef_setcb_signal_manager(sef_cb_signal_manager_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_signal_manager = cb;
}

/*===========================================================================*
 *       	          sef_cb_signal_handler_null                         *
 *===========================================================================*/
void sef_cb_signal_handler_null(int signo)
{
}

/*===========================================================================*
 *       	          sef_cb_signal_manager_null                         *
 *===========================================================================*/
int sef_cb_signal_manager_null(endpoint_t target, int signo)
{
  return OK;
}

/*===========================================================================*
 *       	          sef_cb_signal_handler_term                         *
 *===========================================================================*/
void sef_cb_signal_handler_term(int signo)
{
  /* Terminate in case of SIGTERM, ignore other signals. */
  if(signo == SIGTERM) {
      sef_exit(1);
  }
}

/*===========================================================================*
 *      	      sef_cb_signal_handler_posix_default                    *
 *===========================================================================*/
void sef_cb_signal_handler_posix_default(int signo)
{
  switch(signo) {
      /* Ignore when possible. */
      case SIGCHLD:
      case SIGWINCH:
      case SIGCONT:
      case SIGTSTP:
      case SIGTTIN:
      case SIGTTOU:
      break;

      /* Terminate in any other case unless it is a kernel signal. */
      default:
          if(!IS_SIGK(signo)) {
              sef_exit(1);
          }
      break;
  }
}

